// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_STAGE_BOUNDS_PROVIDER_BASIC_H_
#define DEVICE_VR_OPENXR_OPENXR_STAGE_BOUNDS_PROVIDER_BASIC_H_

#include "device/vr/openxr/openxr_stage_bounds_provider.h"

#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// Provides a default implementation of an OpenXrStageBoundsProvider by
// leveraging the core-spec functionality `xrGetReferenceSpaceBoundsRect`.
class OpenXrStageBoundsProviderBasic : public OpenXrStageBoundsProvider {
 public:
  explicit OpenXrStageBoundsProviderBasic(XrSession session);
  // Returns the bounds of the current stage, with points defined in a clockwise
  // order.
  std::vector<gfx::Point3F> GetStageBounds() override;

 private:
  XrSession session_;
};

// We create an ExtensionHandler for this class even though it doesn't leverage
// extensions because it serves as a fallback since we *do* have
// ExtensionHandlers for this type of data.
class OpenXrStageBoundsProviderBasicFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrStageBoundsProviderBasicFactory();
  ~OpenXrStageBoundsProviderBasicFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  bool IsEnabled(
      const OpenXrExtensionEnumeration* extension_enum) const override;
  std::unique_ptr<OpenXrStageBoundsProvider> CreateStageBoundsProvider(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session) const override;
};
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_STAGE_BOUNDS_PROVIDER_BASIC_H_
