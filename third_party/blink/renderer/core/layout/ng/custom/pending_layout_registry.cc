// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/pending_layout_registry.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void PendingLayoutRegistry::NotifyLayoutReady(const AtomicString& name) {
  PendingSet* set = pending_layouts_.at(name);
  if (set) {
    for (const auto& node : *set) {
      // If the node hasn't been gc'd, trigger a reattachment so that the
      // children are correctly blockified.
      //
      // NOTE: From the time when this node was added as having a pending
      // layout, its display value may have changed to something (block) which
      // doesn't need a layout tree reattachment.
      if (node && node->GetLayoutObject()) {
        const ComputedStyle& style = node->GetLayoutObject()->StyleRef();
        if (style.IsDisplayLayoutCustomBox() &&
            style.DisplayLayoutCustomName() == name)
          node->SetForceReattachLayoutTree();
      }
    }
  }
  pending_layouts_.erase(name);
}

void PendingLayoutRegistry::AddPendingLayout(const AtomicString& name,
                                             Node* node) {
  Member<PendingSet>& set =
      pending_layouts_.insert(name, nullptr).stored_value->value;
  if (!set)
    set = MakeGarbageCollected<PendingSet>();
  set->insert(node);
}

void PendingLayoutRegistry::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_layouts_);
}

}  // namespace blink
