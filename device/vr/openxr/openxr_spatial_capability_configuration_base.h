// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_CAPABILITY_CONFIGURATION_BASE_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_CAPABILITY_CONFIGURATION_BASE_H_

#include <vector>

#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// Helper class for initializing the SpatialContext as part of the spatial
// entities framework (managed by OpenXrSpatialFrameworkManager).
// In order to create a spatial context, a list of |capabilityConfigs| must
// be supplied. These capabilityConfigs are all of the base type
// XrSpatialCapabilityConfigurationBaseHeaderEXT, but may have additional
// structs and fields added to them. From observation, most of the spatial
// capability configurations only have the same types and fields as the base
// class in the same order. This base class manages all configurations who's
// subclass is basically just a simple alias for the BaseHeaderEXT, while
// providing flexibility to allow creating more complex capability configs.
class OpenXrSpatialCapabilityConfigurationBase {
 public:
  OpenXrSpatialCapabilityConfigurationBase(
      XrSpatialCapabilityEXT capability,
      const absl::flat_hash_set<XrSpatialComponentTypeEXT>& components);
  virtual ~OpenXrSpatialCapabilityConfigurationBase();

  OpenXrSpatialCapabilityConfigurationBase(
      const OpenXrSpatialCapabilityConfigurationBase&);
  OpenXrSpatialCapabilityConfigurationBase& operator=(
      const OpenXrSpatialCapabilityConfigurationBase&);

  OpenXrSpatialCapabilityConfigurationBase(
      OpenXrSpatialCapabilityConfigurationBase&&);
  OpenXrSpatialCapabilityConfigurationBase& operator=(
      OpenXrSpatialCapabilityConfigurationBase&&);

  // Returns an XrSpatialCapabilityConfigurationBaseHeaderEXT* representing
  // this object. This pointer is valid for as long as the object is, but
  // moving this object invalidates the pointer.
  virtual XrSpatialCapabilityConfigurationBaseHeaderEXT* GetAsBaseHeader();

 private:
  std::vector<XrSpatialComponentTypeEXT> components_;
  XrSpatialCapabilityConfigurationBaseHeaderEXT config_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_CAPABILITY_CONFIGURATION_BASE_H_
