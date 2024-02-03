// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_

#include "device/vr/openxr/openxr_anchor_manager.h"

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionEnumeration;
class OpenXrExtensionHelper;

class OpenXrAnchorManagerAndroid : public OpenXrAnchorManager {
 public:
  OpenXrAnchorManagerAndroid(const OpenXrExtensionHelper& extension_helper,
                             XrSession session,
                             XrSpace mojo_space);

  ~OpenXrAnchorManagerAndroid() override;

 private:
  XrSpace CreateAnchor(XrPosef pose,
                       XrSpace space,
                       XrTime predicted_display_time) override;
  void OnDetachAnchor(const XrSpace& anchor_data) override;
  base::expected<device::Pose, OpenXrAnchorManager::AnchorTrackingErrorType>
  GetAnchorFromMojom(XrSpace anchor_space,
                     XrTime predicted_display_time) const override;

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;
};

class OpenXrAnchorManagerAndroidFactory : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrAnchorManagerAndroidFactory();
  ~OpenXrAnchorManagerAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXrAnchorManager> CreateAnchorManager(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space) const override;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_
