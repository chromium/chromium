// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/openxr_stage_bounds_provider_basic.h"

#include <vector>

#include "base/logging.h"
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

}  // namespace device
