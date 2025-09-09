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

std::vector<XrSpatialComponentTypeEXT> GetSupportedComponentTypes(
    PFN_xrEnumerateSpatialCapabilityComponentTypesEXT
        xrEnumerateSpatialCapabilityComponentTypesEXT,
    XrInstance instance,
    XrSystemId system,
    XrSpatialCapabilityEXT capability) {
  std::vector<XrSpatialComponentTypeEXT> supported_components;
  XrSpatialCapabilityComponentTypesEXT component_types{
      XR_TYPE_SPATIAL_CAPABILITY_COMPONENT_TYPES_EXT};
  if (XR_FAILED(xrEnumerateSpatialCapabilityComponentTypesEXT(
          instance, system, capability, &component_types))) {
    return supported_components;
  }

  supported_components.resize(component_types.componentTypeCountOutput);
  component_types.componentTypeCapacityInput =
      component_types.componentTypeCountOutput;
  component_types.componentTypes = supported_components.data();

  if (XR_FAILED(xrEnumerateSpatialCapabilityComponentTypesEXT(
          instance, system, capability, &component_types))) {
    supported_components.clear();
  }

  return supported_components;
}

}  // namespace device
