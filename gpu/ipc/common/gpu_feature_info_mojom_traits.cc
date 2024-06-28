// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_feature_info_mojom_traits.h"

#include "base/ranges/algorithm.h"
#include "build/build_config.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuFeatureInfoDataView, gpu::GpuFeatureInfo>::
    Read(gpu::mojom::GpuFeatureInfoDataView data, gpu::GpuFeatureInfo* out) {
  return data.ReadStatusValues(&out->status_values) &&
         data.ReadEnabledGpuDriverBugWorkarounds(
             &out->enabled_gpu_driver_bug_workarounds) &&
         data.ReadDisabledExtensions(&out->disabled_extensions) &&
         data.ReadDisabledWebglExtensions(&out->disabled_webgl_extensions) &&
         data.ReadAppliedGpuBlocklistEntries(
             &out->applied_gpu_blocklist_entries) &&
         gpu::GpuBlocklist::AreEntryIndicesValid(
             out->applied_gpu_blocklist_entries) &&
         data.ReadAppliedGpuDriverBugListEntries(
             &out->applied_gpu_driver_bug_list_entries) &&
         gpu::GpuDriverBugList::AreEntryIndicesValid(
             out->applied_gpu_driver_bug_list_entries) &&
         data.ReadSupportedBufferFormatsForAllocationAndTexturing(
             &out->supported_buffer_formats_for_allocation_and_texturing);
}

}  // namespace mojo
