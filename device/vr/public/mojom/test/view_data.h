// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_

#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace device {

struct ViewData {
  SkColor color;
  mojom::XREye eye;
  gfx::Rect viewport;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_
