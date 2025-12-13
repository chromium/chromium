// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scope.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/style_name_scope.h"

namespace blink {

AnchorScopedName* ToAnchorScopedName(const ScopedCSSName& name,
                                     const LayoutObject& layout_object) {
  return MakeGarbageCollected<AnchorScopedName>(
      name,
      AnchorScopedName::FindScopeElement(
          name, To<Element>(*layout_object.GetNode()),
          [](const ComputedStyle& style) { return style.AnchorScope(); }));
}

}  // namespace blink
