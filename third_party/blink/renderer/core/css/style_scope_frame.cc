// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope_frame.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

void StyleScopeActivation::Trace(blink::Visitor* visitor) const {
  visitor->Trace(root);
}

}  // namespace blink
