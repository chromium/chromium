// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_ycbcr_info.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"

namespace gpu {

VulkanYCbCrInfo::VulkanYCbCrInfo() = default;

VulkanYCbCrInfo::VulkanYCbCrInfo(uint32_t image_format,
                                 uint64_t external_format,
                                 uint32_t suggested_ycbcr_model,
                                 uint32_t suggested_ycbcr_range,
                                 uint32_t suggested_xchroma_offset,
                                 uint32_t suggested_ychroma_offset,
                                 uint32_t format_features)
    : image_format(image_format),
      external_format(external_format),
      suggested_ycbcr_model(suggested_ycbcr_model),
      suggested_ycbcr_range(suggested_ycbcr_range),
      suggested_xchroma_offset(suggested_xchroma_offset),
      suggested_ychroma_offset(suggested_ychroma_offset),
      format_features(format_features) {
  // One and only one of the format fields must be non-zero.
  DCHECK((image_format == 0) ^ (external_format == 0));

  // |format_features| must be set for external images.
  DCHECK(external_format == 0 || format_features != 0);
}

void VulkanYCbCrInfo::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("image_format", image_format);
  value->SetString("external_format",
                   base::StringPrintf("0x%" PRIx64, external_format));
  value->SetInteger("suggested_ycbcr_model", suggested_ycbcr_model);
  value->SetInteger("suggested_ycbcr_range", suggested_ycbcr_range);
  value->SetInteger("suggested_xchroma_offset", suggested_xchroma_offset);
  value->SetInteger("suggested_ychroma_offset", suggested_ychroma_offset);
  value->SetInteger("format_features", format_features);
}

}  // namespace gpu
