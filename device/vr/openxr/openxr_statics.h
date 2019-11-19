// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_STATICS_H_
#define DEVICE_VR_OPENXR_OPENXR_STATICS_H_

#include <d3d11.h>
#include <memory>

#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace device {

class DEVICE_VR_EXPORT OpenXrStatics {
 public:
  OpenXrStatics();
  ~OpenXrStatics();

  bool IsHardwareAvailable();
  bool IsApiAvailable();

 private:
  XrInstance instance_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_STATICS_H_