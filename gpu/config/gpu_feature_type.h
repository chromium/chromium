// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_FEATURE_TYPE_H_
#define GPU_CONFIG_GPU_FEATURE_TYPE_H_

namespace gpu {

// Provides flags indicating which gpu features are blocklisted for the system
// on which chrome is currently running.
// If a bit is set to 1, corresponding feature is blocklisted.
enum GpuFeatureType {
  GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS = 0,
  GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
  GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
  GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
  GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION,
  GPU_FEATURE_TYPE_ACCELERATED_WEBGL2,
  GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL,
  GPU_FEATURE_TYPE_ACCELERATED_GL,
  GPU_FEATURE_TYPE_VULKAN,
  GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION,
  GPU_FEATURE_TYPE_ACCELERATED_WEBGPU,
  GPU_FEATURE_TYPE_SKIA_GRAPHITE,
  GPU_FEATURE_TYPE_WEBNN,
  NUMBER_OF_GPU_FEATURE_TYPES
};
static_assert(GpuFeatureType::NUMBER_OF_GPU_FEATURE_TYPES == 13,
              "Please update the mojo definition of the length of "
              "GpuFeatureInfo.status_values");

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_FEATURE_TYPE_H_
