// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_
#define DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"

namespace device {

class XRSessionModeUtils {
 public:
  static bool IsImmersive(mojom::XRSessionMode mode) {
    return mode == mojom::XRSessionMode::kImmersiveVr ||
           mode == mojom::XRSessionMode::kImmersiveAr;
  }
};
}  // namespace device
#endif  // DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_
