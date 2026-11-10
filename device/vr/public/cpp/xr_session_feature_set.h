// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_XR_SESSION_FEATURE_SET_H_
#define DEVICE_VR_PUBLIC_CPP_XR_SESSION_FEATURE_SET_H_

#include "base/containers/enum_set.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"

namespace device {

using XRSessionFeatureSet = base::EnumSet<mojom::XRSessionFeature>;

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_XR_SESSION_FEATURE_SET_H_
