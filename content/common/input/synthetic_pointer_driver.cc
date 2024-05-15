// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_driver.h"

#include "content/common/input/synthetic_mouse_driver.h"
#include "content/common/input/synthetic_pen_driver.h"
#include "content/common/input/synthetic_touch_driver.h"

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
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
std::unique_ptr<SyntheticPointerDriver> SyntheticPointerDriver::Create(
    content::mojom::GestureSourceType gesture_source_type,
    bool from_devtools_debugger) {
  auto driver = Create(gesture_source_type);
  driver->from_devtools_debugger_ = from_devtools_debugger;
  return driver;
}

}  // namespace content
