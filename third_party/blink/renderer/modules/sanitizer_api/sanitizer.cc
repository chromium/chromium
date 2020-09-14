// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Sanitizer* Sanitizer::Create(const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(config);
}

Sanitizer::Sanitizer(const SanitizerConfig* config)
    : config_(const_cast<SanitizerConfig*>(config)) {
  // Format dropElements to uppercases.
  if (config->hasDropElements()) {
    Vector<String> l;
    for (const String& s : config->dropElements()) {
      l.push_back(s.UpperASCII());
    }
    config_->setDropElements(l);
  }
}

Sanitizer::~Sanitizer() = default;

String Sanitizer::sanitizeToString(ScriptState* script_state,
                                   const String& input,
                                   ExceptionState& exception_state) {
  return CreateMarkup(sanitize(script_state, input, exception_state),
                      kChildrenOnly);
}

DocumentFragment* Sanitizer::sanitize(ScriptState* script_state,
                                      const String& input,
                                      ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }
  Document* document = window->document();
  DocumentFragment* fragment = document->createDocumentFragment();
  DCHECK(document->QuerySelector("body"));
  fragment->ParseHTML(input, document->QuerySelector("body"));

  // Remove all the elements in the dropElements list.
  if (config_->hasDropElements()) {
    Node* node = fragment->firstChild();

    while (node) {
      // Skip non-Element nodes.
      if (node->getNodeType() != Node::NodeType::kElementNode) {
        node = NodeTraversal::Next(*node, fragment);
        continue;
      }

      // TODO(crbug.com/1126936): Review the sanitising algorithm for non-HTMLs.
      String node_name = node->nodeName();
      // If the current element is dropped, remove current element entirely and
      // proceed to its next sibling.
      if (config_->dropElements().Contains(node_name.UpperASCII())) {
        Node* tmp = node;
        node = NodeTraversal::NextSkippingChildren(*node, fragment);
        tmp->remove();
      } else {
        // Otherwise, proceed to the next node (preorder, depth-first
        // traversal).
        node = NodeTraversal::Next(*node, fragment);
      }
    }
  }

  return fragment;
}

// TODO(lyf): https://github.com/WICG/sanitizer-api/issues/34
SanitizerConfig* Sanitizer::creationOptions() const {
  return config_;
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(config_);
}

}  // namespace blink
