// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_blocklist.h"

#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/software_rendering_list_autogen.h"

namespace gpu {

GpuBlocklist::GpuBlocklist(base::span<const GpuControlList::Entry> data)
    : GpuControlList(data) {}

GpuBlocklist::~GpuBlocklist() = default;

// static
std::unique_ptr<GpuBlocklist> GpuBlocklist::Create() {
  return Create(GetSoftwareRenderingListEntries());
}

// static
std::unique_ptr<GpuBlocklist> GpuBlocklist::Create(
    base::span<const GpuControlList::Entry> data) {
  std::unique_ptr<GpuBlocklist> list(new GpuBlocklist(data));
  list->AddSupportedFeature("accelerated_2d_canvas",
                            GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  list->AddSupportedFeature("accelerated_webgl",
                            GPU_FEATURE_TYPE_ACCELERATED_WEBGL);
  list->AddSupportedFeature("accelerated_video_decode",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  list->AddSupportedFeature("accelerated_video_encode",
                            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE);
  list->AddSupportedFeature("gpu_tile_rasterization",
                            GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION);
  list->AddSupportedFeature("accelerated_webgl2",
                            GPU_FEATURE_TYPE_ACCELERATED_WEBGL2);
  list->AddSupportedFeature("android_surface_control",
                            GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL);
  list->AddSupportedFeature("accelerated_gl", GPU_FEATURE_TYPE_ACCELERATED_GL);
  list->AddSupportedFeature("vulkan", GPU_FEATURE_TYPE_VULKAN);
  list->AddSupportedFeature("canvas_oop_rasterization",
                            GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION);
  list->AddSupportedFeature("accelerated_webgpu",
                            GPU_FEATURE_TYPE_ACCELERATED_WEBGPU);
  list->AddSupportedFeature("skia_graphite", GPU_FEATURE_TYPE_SKIA_GRAPHITE);
  list->AddSupportedFeature("webnn", GPU_FEATURE_TYPE_WEBNN);
  return list;
}

// static
bool GpuBlocklist::AreEntryIndicesValid(
    const std::vector<uint32_t>& entry_indices) {
  return GpuControlList::AreEntryIndicesValid(
      entry_indices, GetSoftwareRenderingListEntries().size());
}

}  // namespace gpu
