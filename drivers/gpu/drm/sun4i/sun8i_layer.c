/*
 * Copyright (C) Icenowy Zheng <icenowy@aosc.io>
 *
 * Based on sun4i_layer.h, which is:
 *   Copyright (C) 2015 Free Electrons
 *   Copyright (C) 2015 NextThing Co
 *
 *   Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_plane_helper.h>
#include <drm/drmP.h>

#include "sun8i_layer.h"
#include "sun8i_mixer.h"

static int sun8i_mixer_layer_atomic_check(struct drm_plane *plane,
					  struct drm_plane_state *state)
{
	struct sun8i_layer *layer = plane_to_sun8i_layer(plane);
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	int min_scale, max_scale;
	bool scaler_supported;
	struct drm_rect clip;

	if (!crtc)
		return 0;

	crtc_state = drm_atomic_get_existing_crtc_state(state->state, crtc);
	if (WARN_ON(!crtc_state))
		return -EINVAL;

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = crtc_state->adjusted_mode.hdisplay;
	clip.y2 = crtc_state->adjusted_mode.vdisplay;

	scaler_supported = !!(layer->mixer->cfg->scaler_mask & BIT(layer->id));

	min_scale = scaler_supported ? 1 : DRM_PLANE_HELPER_NO_SCALING;
	max_scale = scaler_supported ? (1UL << 20) - 1 :
				       DRM_PLANE_HELPER_NO_SCALING;

	return drm_plane_helper_check_state(state, &clip,
					    min_scale, max_scale,
					    true, true);
}

static void sun8i_mixer_layer_atomic_disable(struct drm_plane *plane,
					     struct drm_plane_state *old_state)
{
	struct sun8i_layer *layer = plane_to_sun8i_layer(plane);
	struct sun8i_mixer *mixer = layer->mixer;

	sun8i_mixer_layer_enable(mixer, layer->id, false);
}

static void sun8i_mixer_layer_atomic_update(struct drm_plane *plane,
					      struct drm_plane_state *old_state)
{
	struct sun8i_layer *layer = plane_to_sun8i_layer(plane);
	struct sun8i_mixer *mixer = layer->mixer;

	if (!plane->state->visible) {
		sun8i_mixer_layer_enable(mixer, layer->id, false);
		return;
	}

	if (layer->id < mixer->cfg->vi_num) {
		sun8i_mixer_update_vi_layer_coord(mixer, layer->id, plane);
		sun8i_mixer_update_vi_layer_formats(mixer, layer->id, plane);
		sun8i_mixer_update_vi_layer_buffer(mixer, layer->id, plane);
	} else {
		sun8i_mixer_update_ui_layer_coord(mixer, layer->id, plane);
		sun8i_mixer_update_ui_layer_formats(mixer, layer->id, plane);
		sun8i_mixer_update_ui_layer_buffer(mixer, layer->id, plane);
	}

	sun8i_mixer_layer_enable(mixer, layer->id, true);
}

static struct drm_plane_helper_funcs sun8i_mixer_layer_helper_funcs = {
	.atomic_check	= sun8i_mixer_layer_atomic_check,
	.atomic_disable	= sun8i_mixer_layer_atomic_disable,
	.atomic_update	= sun8i_mixer_layer_atomic_update,
};

static const struct drm_plane_funcs sun8i_mixer_layer_funcs = {
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
	.atomic_duplicate_state	= drm_atomic_helper_plane_duplicate_state,
	.destroy		= drm_plane_cleanup,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.reset			= drm_atomic_helper_plane_reset,
	.update_plane		= drm_atomic_helper_update_plane,
};

static const u32 sun8i_mixer_ui_layer_formats[] = {
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
};

/*
 * While all RGB formats are supported, VI planes don't support
 * alpha blending, so there is no point having formats with alpha
 * channel if their opaque analog exist.
 */
static const u32 sun8i_mixer_vi_layer_formats[] = {
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,

	DRM_FORMAT_NV16,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV61,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_YUV411,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
	DRM_FORMAT_YVU411,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_YVU422,
	DRM_FORMAT_YVU444,
};

static struct sun8i_layer *sun8i_layer_init_one(struct drm_device *drm,
						struct sun8i_mixer *mixer,
						int index)
{
	struct sun8i_layer *layer;
	enum drm_plane_type type;
	unsigned int format_count;
	const u32 *formats;
	int ret;

	layer = devm_kzalloc(drm->dev, sizeof(*layer), GFP_KERNEL);
	if (!layer)
		return ERR_PTR(-ENOMEM);

	if (index < mixer->cfg->vi_num) {
		formats = sun8i_mixer_vi_layer_formats;
		format_count = ARRAY_SIZE(sun8i_mixer_vi_layer_formats);
	} else {
		formats = sun8i_mixer_ui_layer_formats;
		format_count = ARRAY_SIZE(sun8i_mixer_ui_layer_formats);
	}

	/* possible crtcs are set later */
	type = index == mixer->cfg->vi_num ?
		DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(drm, &layer->plane, 0,
				       &sun8i_mixer_layer_funcs,
				       formats, format_count,
				       NULL, type, NULL);
	if (ret) {
		dev_err(drm->dev, "Couldn't initialize layer\n");
		return ERR_PTR(ret);
	}

	/* TODO implement zpos support */
	ret = drm_plane_create_zpos_immutable_property(&layer->plane, index);
	if (ret) {
		dev_err(drm->dev, "Couldn't add zpos property\n");
		return ERR_PTR(ret);
	}

	drm_plane_helper_add(&layer->plane,
			     &sun8i_mixer_layer_helper_funcs);
	layer->mixer = mixer;
	layer->id = index;

	return layer;
}

struct drm_plane **sun8i_layers_init(struct drm_device *drm,
				     struct sunxi_engine *engine)
{
	struct drm_plane **planes;
	struct sun8i_mixer *mixer = engine_to_sun8i_mixer(engine);
	int i, plane_cnt;

	plane_cnt = mixer->cfg->vi_num + mixer->cfg->ui_num;
	planes = devm_kcalloc(drm->dev, plane_cnt + 1, sizeof(*planes),
			      GFP_KERNEL);
	if (!planes)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < plane_cnt; i++) {
		struct sun8i_layer *layer;

		layer = sun8i_layer_init_one(drm, mixer, i);
		if (IS_ERR(layer)) {
			dev_err(drm->dev, "Couldn't initialize %s plane\n",
				i == mixer->cfg->vi_num ?
					"primary" : "overlay");
			return ERR_CAST(layer);
		};

		planes[i] = &layer->plane;
	};

	return planes;
}
