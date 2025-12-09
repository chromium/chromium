// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/trigger_scoped_name.h"

#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/style/style_name_scope.h"

namespace blink {

TriggerScopedName* ToTriggerScopedName(const ScopedCSSName& name,
                                       const Element& originating_element) {
  return MakeGarbageCollected<TriggerScopedName>(
      name, TriggerScopedName::FindScopeElement(name, originating_element,
                                                [](const ComputedStyle& style) {
                                                  return style.TriggerScope();
                                                }));
}

}  // namespace blink
