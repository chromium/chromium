// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_UNBOUNDED_SPACE_PROVIDER_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_UNBOUNDED_SPACE_PROVIDER_MSFT_H_
#include "device/vr/openxr/openxr_unbounded_space_provider.h"

#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
namespace device {
class OpenXrUnboundedSpaceProviderMsft : public OpenXrUnboundedSpaceProvider {
 public:
  OpenXrUnboundedSpaceProviderMsft();
  ~OpenXrUnboundedSpaceProviderMsft() override;
  XrReferenceSpaceType GetType() const override;
};

class OpenXrUnboundedSpaceProviderMsftFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrUnboundedSpaceProviderMsftFactory();
  ~OpenXrUnboundedSpaceProviderMsftFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXrUnboundedSpaceProvider> CreateUnboundedSpaceProvider(
      const OpenXrExtensionHelper& extension_helper) const override;
};
}  // namespace device
#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_UNBOUNDED_SPACE_PROVIDER_MSFT_H_
