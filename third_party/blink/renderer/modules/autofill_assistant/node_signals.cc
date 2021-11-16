// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {
namespace {

void CollectSignalsForNode(
    const Node& node,
    WebVector<AutofillAssistantNodeSignals>* node_signals) {
  // Note: This does not pierce documents like ShadowDom.
  for (Element& element : ElementTraversal::DescendantsOf(node)) {
    if (element.HasTagName(html_names::kInputTag) ||
        element.HasTagName(html_names::kTextareaTag) ||
        element.HasTagName(html_names::kSelectTag)) {
      AutofillAssistantNodeSignals signals;
      signals.backend_node_id =
          static_cast<int>(DOMNodeIds::IdForNode(&element));

      // TODO(sandromaggi): Collect all features for the node.
      signals.node_features.html_tag = element.tagName();

      node_signals->push_back(std::move(signals));
    }
    ShadowRoot* shadow_root = element.GetShadowRoot();
    if (shadow_root && shadow_root->GetType() != ShadowRootType::kUserAgent) {
      CollectSignalsForNode(*shadow_root, node_signals);
    }
  }
}

}  // namespace

WebVector<AutofillAssistantNodeSignals> GetAutofillAssistantNodeSignals(
    const WebDocument& web_document) {
  WebVector<AutofillAssistantNodeSignals> node_signals;
  Document* document = web_document;
  CollectSignalsForNode(*document, &node_signals);
  return node_signals;
}

}  // namespace blink
