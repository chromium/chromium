// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_STAGE_BOUNDS_PROVIDER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_STAGE_BOUNDS_PROVIDER_ANDROID_H_

#include "device/vr/openxr/openxr_stage_bounds_provider.h"

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// Provides an implementation of an OpenXrStageBoundsProvider backed by the
// XR_ANDROID_reference_space_bounds_polygon extension.
class OpenXrStageBoundsProviderAndroid : public OpenXrStageBoundsProvider {
 public:
  OpenXrStageBoundsProviderAndroid(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session);
  ~OpenXrStageBoundsProviderAndroid() override;
  std::vector<gfx::Point3F> GetStageBounds() override;

 private:
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
};

class OpenXrStageBoundsProviderAndroidFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrStageBoundsProviderAndroidFactory();
  ~OpenXrStageBoundsProviderAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXrStageBoundsProvider> CreateStageBoundsProvider(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session) const override;
};
}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_STAGE_BOUNDS_PROVIDER_ANDROID_H_
