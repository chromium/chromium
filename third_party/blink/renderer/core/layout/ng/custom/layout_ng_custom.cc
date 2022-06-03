// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/custom/layout_ng_custom.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_worklet.h"

namespace blink {

LayoutNGCustom::LayoutNGCustom(Element* element)
    : LayoutNGBlockFlow(element), state_(kUnloaded) {
  DCHECK(element);
}

void LayoutNGCustom::AddChild(LayoutObject* new_child,
                              LayoutObject* before_child) {
  // Only use the block-flow AddChild logic when we are unloaded, i.e. we
  // should behave exactly like a block-flow.
  if (state_ == kUnloaded) {
    LayoutNGBlockFlow::AddChild(new_child, before_child);
    return;
  }
  LayoutBlock::AddChild(new_child, before_child);
}

void LayoutNGCustom::RemoveChild(LayoutObject* child) {
  // Only use the block-flow RemoveChild logic when we are unloaded, i.e. we
  // should behave exactly like a block-flow.
  if (state_ == kUnloaded) {
    LayoutNGBlockFlow::RemoveChild(child);
    return;
  }
  LayoutBlock::RemoveChild(child);
}

void LayoutNGCustom::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  if (state_ == kUnloaded) {
    const AtomicString& name = StyleRef().DisplayLayoutCustomName();
    LayoutWorklet* worklet = LayoutWorklet::From(*GetDocument().domWindow());
    // Register if we'll need to reattach the layout tree when a matching
    // "layout()" is registered.
    worklet->AddPendingLayout(name, GetNode());

    LayoutWorklet::DocumentDefinitionMap* document_definition_map =
        worklet->GetDocumentDefinitionMap();
    if (document_definition_map->Contains(name)) {
      DocumentLayoutDefinition* existing_document_definition =
          document_definition_map->at(name);
      if (existing_document_definition->GetRegisteredDefinitionCount() ==
          LayoutWorklet::kNumGlobalScopes)
        state_ = kBlock;
    }
  }

  // Make our children "block-level" before invoking StyleDidChange. As the
  // current multi-col logic may invoke a call to AddChild, failing a DCHECK.
  if (state_ != kUnloaded)
    SetChildrenInline(false);

  // TODO(ikilpatrick): Investigate reducing the properties which
  // LayoutNGBlockFlow::StyleDidChange invalidates upon. (For example margins).
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);
}

}  // namespace blink
