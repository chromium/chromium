// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_map.h"

namespace blink {

void CSSToggleMap::Trace(Visitor* visitor) const {
  visitor->Trace(toggles_);

  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
