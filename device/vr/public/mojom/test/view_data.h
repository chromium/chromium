// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_

#include "device/vr/public/mojom/test/color.h"
#include "ui/gfx/geometry/rect.h"

namespace device {

enum class XrEye {
  kLeft = 0,
  kRight = 1,
  kNone = 2,
};

struct ViewData {
  Color color;
  XrEye eye;
  gfx::Rect viewport;
  char raw_buffer[256];  // Can encode raw data here.
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_VIEW_DATA_H_
