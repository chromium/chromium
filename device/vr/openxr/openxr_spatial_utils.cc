// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_utils.h"

namespace device {

std::vector<XrSpatialCapabilityEXT> GetCapabilities(
    PFN_xrEnumerateSpatialCapabilitiesEXT xrEnumerateSpatialCapabilitiesEXT,
    XrInstance instance,
    XrSystemId system) {
  std::vector<XrSpatialCapabilityEXT> capabilities;
  uint32_t count;
  if (XR_FAILED(xrEnumerateSpatialCapabilitiesEXT(instance, system, 0, &count,
                                                  nullptr)) ||
      count == 0) {
    return capabilities;
  }

  capabilities.resize(count);
  if (XR_FAILED(xrEnumerateSpatialCapabilitiesEXT(
          instance, system, count, &count, capabilities.data()))) {
    capabilities.clear();
  }

  return capabilities;
}

}  // namespace device
