// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_unbounded_space_provider_android.h"

#include <set>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/dev/xr_android.h"

namespace device {
OpenXrUnboundedSpaceProviderAndroid::OpenXrUnboundedSpaceProviderAndroid() =
    default;
OpenXrUnboundedSpaceProviderAndroid::~OpenXrUnboundedSpaceProviderAndroid() =
    default;

XrReferenceSpaceType OpenXrUnboundedSpaceProviderAndroid::GetType() const {
  return XR_REFERENCE_SPACE_TYPE_UNBOUNDED_ANDROID;
}

OpenXrUnboundedSpaceProviderAndroidFactory::
    OpenXrUnboundedSpaceProviderAndroidFactory() = default;
OpenXrUnboundedSpaceProviderAndroidFactory::
    ~OpenXrUnboundedSpaceProviderAndroidFactory() = default;

const base::flat_set<std::string_view>&
OpenXrUnboundedSpaceProviderAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrUnboundedSpaceProviderAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED};
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrUnboundedSpaceProviderAndroidFactory::CreateUnboundedSpaceProvider(
    const OpenXrExtensionHelper& extension_helper) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrUnboundedSpaceProviderAndroid>();
  }

  return nullptr;
}
}  // namespace device
