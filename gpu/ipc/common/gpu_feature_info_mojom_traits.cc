// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_feature_info_mojom_traits.h"
#include "build/build_config.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuFeatureInfoDataView, gpu::GpuFeatureInfo>::
    Read(gpu::mojom::GpuFeatureInfoDataView data, gpu::GpuFeatureInfo* out) {
  std::vector<gpu::GpuFeatureStatus> info_status;
  if (!data.ReadStatusValues(&info_status))
    return false;
  if (info_status.size() != gpu::NUMBER_OF_GPU_FEATURE_TYPES)
    return false;
  std::copy(info_status.begin(), info_status.end(), out->status_values);
  return data.ReadEnabledGpuDriverBugWorkarounds(
             &out->enabled_gpu_driver_bug_workarounds) &&
         data.ReadDisabledExtensions(&out->disabled_extensions) &&
         data.ReadDisabledWebglExtensions(&out->disabled_webgl_extensions) &&
         data.ReadAppliedGpuBlacklistEntries(
             &out->applied_gpu_blacklist_entries) &&
         gpu::GpuBlocklist::AreEntryIndicesValid(
             out->applied_gpu_blacklist_entries) &&
         data.ReadAppliedGpuDriverBugListEntries(
             &out->applied_gpu_driver_bug_list_entries) &&
         gpu::GpuDriverBugList::AreEntryIndicesValid(
             out->applied_gpu_driver_bug_list_entries) &&
         data.ReadSupportedBufferFormatsForAllocationAndTexturing(
             &out->supported_buffer_formats_for_allocation_and_texturing);
}

}  // namespace mojo
