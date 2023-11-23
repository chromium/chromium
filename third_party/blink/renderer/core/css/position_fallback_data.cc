// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/position_fallback_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"

namespace blink {

void PositionFallbackData::Trace(Visitor* visitor) const {
  visitor->Trace(try_set_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
