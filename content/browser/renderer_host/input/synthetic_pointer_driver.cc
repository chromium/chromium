// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"

#include "content/browser/renderer_host/input/synthetic_mouse_driver.h"
#include "content/browser/renderer_host/input/synthetic_pen_driver.h"
#include "content/browser/renderer_host/input/synthetic_touch_driver.h"

namespace content {

SyntheticPointerDriver::SyntheticPointerDriver() {}
SyntheticPointerDriver::~SyntheticPointerDriver() {}

// static
std::unique_ptr<SyntheticPointerDriver> SyntheticPointerDriver::Create(
    content::mojom::GestureSourceType gesture_source_type) {
  switch (gesture_source_type) {
    case content::mojom::GestureSourceType::kTouchInput:
      return std::make_unique<SyntheticTouchDriver>();
    case content::mojom::GestureSourceType::kMouseInput:
      return std::make_unique<SyntheticMouseDriver>();
    case content::mojom::GestureSourceType::kPenInput:
      return std::make_unique<SyntheticPenDriver>();
    case content::mojom::GestureSourceType::kDefaultInput:
      return std::unique_ptr<SyntheticPointerDriver>();
  }
  NOTREACHED();
  return std::unique_ptr<SyntheticPointerDriver>();
}

}  // namespace content
