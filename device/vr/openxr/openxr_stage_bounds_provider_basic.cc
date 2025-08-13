// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/openxr_stage_bounds_provider_basic.h"

#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "device/vr/util/stage_utils.h"
#include "ui/gfx/geometry/point3_f.h"

namespace device {

OpenXrStageBoundsProviderBasic::OpenXrStageBoundsProviderBasic(
    XrSession session)
    : session_(session) {}

std::vector<gfx::Point3F> OpenXrStageBoundsProviderBasic::GetStageBounds() {
  XrExtent2Df stage_bounds;
  XrResult xr_result = xrGetReferenceSpaceBoundsRect(
      session_, XR_REFERENCE_SPACE_TYPE_STAGE, &stage_bounds);
  if (XR_FAILED(xr_result)) {
    DVLOG(1) << __func__ << " failed with: " << xr_result;
  }

  // GetStageBoundsFromSize handles the case of the stage bounds being 0,0 which
  // is what `xrGetReferenceSpaceBoundsRect` sets the XrExtent2Df to on failure.
  return vr_utils::GetStageBoundsFromSize(stage_bounds.width,
                                          stage_bounds.height);
}

OpenXrStageBoundsProviderBasicFactory::OpenXrStageBoundsProviderBasicFactory() =
    default;
OpenXrStageBoundsProviderBasicFactory::
    ~OpenXrStageBoundsProviderBasicFactory() = default;

const base::flat_set<std::string_view>&
OpenXrStageBoundsProviderBasicFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions;
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrStageBoundsProviderBasicFactory::GetSupportedFeatures() const {
  return {device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR};
}

void OpenXrStageBoundsProviderBasicFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  SetEnabled(true);
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrStageBoundsProviderBasicFactory::CreateStageBoundsProvider(
    XrSession session) const {
  DVLOG(2) << __func__;
  return std::make_unique<OpenXrStageBoundsProviderBasic>(session);
}

}  // namespace device
