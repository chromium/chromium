// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/css_selector_fragment_anchor.h"

#include "base/feature_list.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fragment_directive/css_selector_directive.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

CssSelectorFragmentAnchor* CssSelectorFragmentAnchor::TryCreate(
    const KURL& url,
    LocalFrame& frame,
    bool should_scroll) {
  DCHECK(RuntimeEnabledFeatures::CSSSelectorFragmentAnchorEnabled());

  Document& doc = *frame.GetDocument();
  HeapVector<Member<CssSelectorDirective>> css_selector_directives =
      frame.GetDocument()
          ->fragmentDirective()
          .GetDirectives<CssSelectorDirective>();

  if (css_selector_directives.IsEmpty())
    return nullptr;

  Element* anchor_node = nullptr;
  for (CssSelectorDirective* directive : css_selector_directives) {
    if (!directive->value().IsEmpty())
      anchor_node = doc.RootNode().QuerySelector(directive->value());
    // TODO(crbug.com/1265721): this will ignore directives after the first
    // successful match, for now we are just scrolling the element into view,
    // later when we add highlighting, it's good considering highlighting all
    // matching elements.
    if (anchor_node)
      break;
  }

  doc.SetCSSTarget(anchor_node);

  if (!anchor_node)
    return nullptr;

  return MakeGarbageCollected<CssSelectorFragmentAnchor>(*anchor_node, frame,
                                                         should_scroll);
}

CssSelectorFragmentAnchor::CssSelectorFragmentAnchor(Element& anchor_node,
                                                     LocalFrame& frame,
                                                     bool should_scroll)
    : SelectorFragmentAnchor(frame, should_scroll),
      anchor_node_(&anchor_node) {}

bool CssSelectorFragmentAnchor::InvokeSelector() {
  DCHECK(anchor_node_);

  // if user has not scrolled yet, do the necessary work to scroll anchor_node_
  // into view, otherwise we are done scrolling.
  if (!user_scrolled_ && should_scroll_ &&
      frame_->GetDocument()->IsLoadCompleted()) {
    ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
    options->setBlock("center");
    options->setInlinePosition("nearest");
    ScrollElementIntoViewWithOptions(anchor_node_, options);
    should_scroll_ = false;
  }

  return true;
}

void CssSelectorFragmentAnchor::PerformPreRafActions() {}

void CssSelectorFragmentAnchor::Installed() {}

void CssSelectorFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_node_);
  SelectorFragmentAnchor::Trace(visitor);
}

}  // namespace blink
