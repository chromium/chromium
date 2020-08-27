// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

namespace blink {

DOMUint16Array* XRDepthInformation::data() const {
  return nullptr;
}

uint32_t XRDepthInformation::width() const {
  return 0;
}

uint32_t XRDepthInformation::height() const {
  return 0;
}

float XRDepthInformation::getDepth(uint32_t col, uint32_t row) const {
  return 0.0;
}

}  // namespace blink
