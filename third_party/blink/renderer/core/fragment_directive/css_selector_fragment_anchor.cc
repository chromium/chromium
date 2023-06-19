// Copyright 2021 The Chromium Authors
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
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive.h"
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

  if (css_selector_directives.empty())
    return nullptr;

  Element* anchor_node = nullptr;
  for (CssSelectorDirective* directive : css_selector_directives) {
    if (!directive->value().empty() && !directive->IsConsumed()) {
      anchor_node = doc.RootNode().QuerySelector(directive->value());

      // TODO(crbug.com/1265721): this will ignore directives after the first
      // successful match, for now we are just scrolling the element into view,
      // later when we add highlighting, it's good considering highlighting all
      // matching elements.
      if (anchor_node)
        break;
    }
  }

  doc.SetSelectorFragmentAnchorCSSTarget(anchor_node);

  if (!anchor_node)
    return nullptr;

  // On the same page navigation i.e. <a href="#element>Go to element</a>
  // we don't want to create a CssSelectorFragmentAnchor again,
  // we want to create an ElementFragmentAnchor instead, so consume all of them
  for (CssSelectorDirective* directive : css_selector_directives)
    directive->SetConsumed(true);

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
  return true;
}

void CssSelectorFragmentAnchor::Installed() {}

void CssSelectorFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_node_);
  SelectorFragmentAnchor::Trace(visitor);
}

}  // namespace blink
