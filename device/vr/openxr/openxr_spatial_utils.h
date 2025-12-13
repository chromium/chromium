// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_UTILS_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_UTILS_H_

#include <vector>

#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// Returns a vector of XrSpatialCapabilityEXT supported by the current runtime.
std::vector<XrSpatialCapabilityEXT> GetCapabilities(
    PFN_xrEnumerateSpatialCapabilitiesEXT xrEnumerateSpatialCapabilitiesEXT,
    XrInstance instance,
    XrSystemId system);

// Returns a vector of XrSpatialComponentTypeEXT for the given capability.
std::vector<XrSpatialComponentTypeEXT> GetSupportedComponentTypes(
    PFN_xrEnumerateSpatialCapabilityComponentTypesEXT
        xrEnumerateSpatialCapabilityComponentTypesEXT,
    XrInstance instance,
    XrSystemId system,
    XrSpatialCapabilityEXT capability);

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_UTILS_H_
