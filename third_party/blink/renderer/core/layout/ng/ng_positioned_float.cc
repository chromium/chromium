// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

void NGPositionedFloat::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
}

}  // namespace blink
