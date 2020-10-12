// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Sanitizer* Sanitizer::Create(const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(config);
}

Sanitizer::Sanitizer(const SanitizerConfig* config)
    : config_(const_cast<SanitizerConfig*>(config)) {
  // Format dropElements to uppercase.
  Vector<String> drop_elements = default_drop_elements_;
  if (config->hasDropElements()) {
    for (const String& s : config->dropElements()) {
      const String& upper_s = s.UpperASCII();
      if (!drop_elements.Contains(upper_s)) {
        drop_elements.push_back(upper_s);
      }
    }
  }
  config_->setDropElements(drop_elements);

  // Format blockElements to uppercase.
  Vector<String> block_elements = default_block_elements_;
  if (config->hasBlockElements()) {
    for (const String& s : config->blockElements()) {
      const String& upper_s = s.UpperASCII();
      if (!drop_elements.Contains(upper_s) &&
          !block_elements.Contains(upper_s)) {
        block_elements.push_back(upper_s);
      }
    }
  }
  config_->setBlockElements(block_elements);

  if (config->hasAllowElements()) {
    // Format allowElements to uppercase.
    Vector<String> l;
    for (const String& s : config->allowElements()) {
      if (!config_->dropElements().Contains(s))
        l.push_back(s.UpperASCII());
    }
    config_->setAllowElements(l);
  }

  // Format dropAttributes to lowercase.
  drop_attributes_ = default_drop_attributes_;
  if (config->hasDropAttributes()) {
    for (const String& s : config->dropAttributes()) {
      drop_attributes_.push_back(WTF::AtomicString(s.LowerASCII()));
    }
  }
  if (config->hasAllowAttributes()) {
    Vector<String> l;
    for (const String& s : config->allowAttributes()) {
      const String& lower_s = s.LowerASCII();
      if (!default_drop_attributes_.Contains(lower_s) &&
          !default_block_elements_.Contains(lower_s))
        l.push_back(lower_s);
    }
    config_->setAllowAttributes(l);
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

  Node* node = fragment->firstChild();

  while (node) {
    // Skip non-Element nodes.
    if (node->getNodeType() != Node::NodeType::kElementNode) {
      node = NodeTraversal::Next(*node, fragment);
      continue;
    }

    // TODO(crbug.com/1126936): Review the sanitising algorithm for non-HTMLs.
    String node_name = node->nodeName().UpperASCII();
    // If the current element is dropped, remove current element entirely and
    // proceed to its next sibling.
    if (config_->dropElements().Contains(node_name)) {
      Node* tmp = node;
      node = NodeTraversal::NextSkippingChildren(*node, fragment);
      tmp->remove();
    } else if ((config_->hasBlockElements() &&
                config_->blockElements().Contains(node_name)) ||
               (config_->hasAllowElements() &&
                !config_->allowElements().Contains(node_name))) {
      // If the current element is blocked, append its children after current
      // node to parent node, remove current element and proceed to the next
      // node.
      Node* parent = node->parentNode();
      Node* next_sibling = node->nextSibling();
      while (node->hasChildren()) {
        Node* n = node->firstChild();
        if (next_sibling) {
          parent->insertBefore(n, next_sibling, exception_state);
        } else {
          parent->appendChild(n, exception_state);
        }
        if (exception_state.HadException()) {
          return nullptr;
        }
      }
      Node* tmp = node;
      node = NodeTraversal::Next(*node, fragment);
      tmp->remove();
    } else {
      // Otherwise, remove any attributes to be dropped from the current
      // element, and proceed to the next node (preorder, depth-first
      // traversal).
      Element* element = To<Element>(node);
      for (const auto& name : element->getAttributeNames()) {
        bool drop = drop_attributes_.Contains(name) ||
                    (config_->hasAllowAttributes() &&
                     !config_->allowAttributes().Contains(name));
        if (drop)
          element->removeAttribute(name);
      }
      node = NodeTraversal::Next(*node, fragment);
    }
  }

  return fragment;
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(config_);
}

}  // namespace blink
