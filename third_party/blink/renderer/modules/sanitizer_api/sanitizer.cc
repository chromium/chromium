// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_parse_from_string_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_document_fragment_or_document.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_trusted_html_or_document_fragment_or_document.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Sanitizer* Sanitizer::Create(ExecutionContext* execution_context,
                             const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(execution_context, config);
}

Sanitizer::Sanitizer(ExecutionContext* execution_context,
                     const SanitizerConfig* config)
    : allow_custom_elements_(config->allowCustomElements()) {
  bool use_default_config = true;
  if (config->allowCustomElements()) {
    use_default_config = false;
  }

  // Format dropElements to uppercase.
  if (config->hasDropElements()) {
    ElementFormatter(drop_elements_, config->dropElements());
    use_default_config = false;
  }

  // Format blockElements to uppercase.
  if (config->hasBlockElements()) {
    ElementFormatter(block_elements_, config->blockElements());
    use_default_config = false;
  }

  // Format allowElements to uppercase.
  if (config->hasAllowElements()) {
    ElementFormatter(allow_elements_, config->allowElements());
    use_default_config = false;
  } else {
    allow_elements_ = default_allow_elements_;
  }

  // Format dropAttributes to lowercase.
  if (config->hasDropAttributes()) {
    AttrFormatter(drop_attributes_, config->dropAttributes());
    use_default_config = false;
  }

  // Format allowAttributes to lowercase.
  if (config->hasAllowAttributes()) {
    AttrFormatter(allow_attributes_, config->allowAttributes());
    use_default_config = false;
  } else {
    allow_attributes_ = default_allow_attributes_;
  }

  if (use_default_config) {
    // TODO(lyf): Add unit tests for counters.
    UseCounter::Count(execution_context,
                      WebFeature::kSanitizerAPIDefaultConfiguration);
  }
}

void Sanitizer::ElementFormatter(HashSet<String>& element_set,
                                 const Vector<String>& elements) {
  for (const String& s : elements) {
    element_set.insert(s.UpperASCII());
  }
}

void Sanitizer::AttrFormatter(
    HashMap<String, Vector<String>>& attr_map,
    const Vector<std::pair<String, Vector<String>>>& attrs) {
  for (const std::pair<String, Vector<String>>& pair : attrs) {
    const String& lower_attr = pair.first.LowerASCII();
    if (pair.second == kVectorStar || pair.second.Contains("*")) {
      attr_map.insert(lower_attr, kVectorStar);
    } else {
      Vector<String> elements;
      for (const String& s : pair.second) {
        elements.push_back(s.UpperASCII());
      }
      attr_map.insert(lower_attr, elements);
    }
  }
}

Sanitizer::~Sanitizer() = default;

String Sanitizer::sanitizeToString(ScriptState* script_state,
                                   StringOrDocumentFragmentOrDocument& input,
                                   ExceptionState& exception_state) {
  return CreateMarkup(SanitizeImpl(script_state, input, exception_state),
                      kChildrenOnly);
}

DocumentFragment* Sanitizer::sanitize(
    ScriptState* script_state,
    StringOrTrustedHTMLOrDocumentFragmentOrDocument& input,
    ExceptionState& exception_state) {
  StringOrDocumentFragmentOrDocument new_input;
  if (input.IsString() || input.IsNull()) {
    LocalDOMWindow* window = LocalDOMWindow::From(script_state);
    if (!window) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Cannot find current DOM window.");
      return nullptr;
    }
    new_input.SetString(TrustedTypesCheckForHTML(
        input.GetAsString(), window->GetExecutionContext(), exception_state));
    if (exception_state.HadException()) {
      return nullptr;
    }
  } else if (input.IsTrustedHTML()) {
    new_input.SetString(input.GetAsTrustedHTML()->toString());
  } else if (input.IsDocument()) {
    new_input.SetDocument(input.GetAsDocument());
  } else if (input.IsDocumentFragment()) {
    new_input.SetDocumentFragment(input.GetAsDocumentFragment());
  }
  return SanitizeImpl(script_state, new_input, exception_state);
}

DocumentFragment* Sanitizer::PrepareFragment(
    LocalDOMWindow* window,
    ScriptState* script_state,
    StringOrDocumentFragmentOrDocument& input,
    ExceptionState& exception_state) {
  DocumentFragment* fragment = nullptr;

  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }
  if (input.IsDocumentFragment()) {
    UseCounter::Count(window->GetExecutionContext(),
                      WebFeature::kSanitizerAPIFromFragment);
    fragment = input.GetAsDocumentFragment();
  } else if (input.IsString() || input.IsNull()) {
    UseCounter::Count(window->GetExecutionContext(),
                      WebFeature::kSanitizerAPIFromString);

    Document* document =
        window->document()
            ? window->document()->implementation().createHTMLDocument()
            : DOMParser::Create(script_state)
                  ->parseFromString("<!DOCTYPE html><html><body></body></html>",
                                    "text/html",
                                    ParseFromStringOptions::Create());
    // TODO(https://crbug.com/1178774): Behavior difference need further
    // investgate.
    fragment = document->createRange()->createContextualFragment(
        input.GetAsString(), exception_state);
  } else if (input.IsDocument()) {
    UseCounter::Count(window->GetExecutionContext(),
                      WebFeature::kSanitizerAPIFromDocument);

    fragment = input.GetAsDocument()->createDocumentFragment();
    fragment->CloneChildNodesFrom(*(input.GetAsDocument()->body()),
                                  CloneChildrenFlag::kClone);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }
  return fragment;
}

DocumentFragment* Sanitizer::DoSanitizing(DocumentFragment* fragment,
                                          LocalDOMWindow* window,
                                          ExceptionState& exception_state) {
  Node* node = fragment->firstChild();

  while (node) {
    // Skip non-Element nodes.
    if (node->getNodeType() != Node::NodeType::kElementNode) {
      node = NodeTraversal::Next(*node, fragment);
      continue;
    }

    // TODO(crbug.com/1126936): Review the sanitising algorithm for non-HTMLs.
    // 1. Let |name| be |element|'s tag name.
    String name = node->nodeName().UpperASCII();

    // 2. Detect whether current element is a custom element or not.
    bool is_custom_element =
        CustomElement::IsValidName(AtomicString(name.LowerASCII()), false);

    // 3. If |kind| is `regular` and if |name| is not contained in the
    // default element allow list, then 'drop'
    if (baseline_drop_elements_.Contains(name)) {
      node = DropElement(node, fragment);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (is_custom_element && !allow_custom_elements_) {
      // 4. If |kind| is `custom` and if allow_custom_elements_ is unset or set
      // to anything other than `true`, then 'drop'.
      node = DropElement(node, fragment);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (!node->IsHTMLElement()) {
      // Presently unspec-ed: If |node| is in a non-HTML namespace: Drop.
      node = DropElement(node, fragment);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (drop_elements_.Contains(name)) {
      // 5. If |name| is in |config|'s [=element drop list=] then 'drop'.
      node = DropElement(node, fragment);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (block_elements_.Contains(name)) {
      // 6. If |name| is in |config|'s [=element block list=] then 'block'.
      node = BlockElement(node, fragment, exception_state);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (!allow_elements_.Contains(name)) {
      // 7. if |name| is not in |config|'s [=element allow list=] then 'block'.
      node = BlockElement(node, fragment, exception_state);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (IsA<HTMLTemplateElement>(node)) {
      // 8. If |element|'s [=element interface=] is {{HTMLTemplateElement}}
      // Run the steps of the [=sanitize document fragment=] algorithm on
      // |element|'s |content| attribute.
      DoSanitizing(To<HTMLTemplateElement>(node)->content(), window,
                   exception_state);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
      node = KeepElement(node, fragment, name, window);
    } else {
      node = KeepElement(node, fragment, name, window);
    }
  }

  return fragment;
}

DocumentFragment* Sanitizer::SanitizeImpl(
    ScriptState* script_state,
    StringOrDocumentFragmentOrDocument& input,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentFragment* fragment =
      PrepareFragment(window, script_state, input, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return DoSanitizing(fragment, window, exception_state);
}

// If the current element needs to be dropped, remove current element entirely
// and proceed to its next sibling.
Node* Sanitizer::DropElement(Node* node, DocumentFragment* fragment) {
  Node* tmp = node;
  node = NodeTraversal::NextSkippingChildren(*node, fragment);
  tmp->remove();
  return node;
}

// If the current element should be blocked, append its children after current
// node to parent node, remove current element and proceed to the next node.
Node* Sanitizer::BlockElement(Node* node,
                              DocumentFragment* fragment,
                              ExceptionState& exception_state) {
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
  return node;
}

// Remove any attributes to be dropped from the current element, and proceed to
// the next node (preorder, depth-first traversal).
Node* Sanitizer::KeepElement(Node* node,
                             DocumentFragment* fragment,
                             String& node_name,
                             LocalDOMWindow* window) {
  Element* element = To<Element>(node);
  if (allow_attributes_.at("*").Contains(node_name)) {
  } else if (drop_attributes_.at("*").Contains(node_name)) {
    for (const auto& name : element->getAttributeNames()) {
      element->removeAttribute(name);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    }
  } else {
    for (const auto& name : element->getAttributeNames()) {
      // Attributes in drop list or not in allow list while allow list
      // exists will be dropped.
      bool drop = (baseline_drop_attributes_.Contains(name) &&
                   (baseline_drop_attributes_.at(name) == kVectorStar ||
                    baseline_drop_attributes_.at(name).Contains(node_name))) ||
                  (drop_attributes_.Contains(name) &&
                   (drop_attributes_.at(name) == kVectorStar ||
                    drop_attributes_.at(name).Contains(node_name))) ||
                  !(allow_attributes_.Contains(name) &&
                    (allow_attributes_.at(name) == kVectorStar ||
                     allow_attributes_.at(name).Contains(node_name)));
      // 9. If |element|'s [=element interface=] is {{HTMLAnchorElement}} or
      // {{HTMLAreaElement}} and |element|'s `protocol` property is
      // "javascript:", then remove the `href` attribute from |element|.
      if (IsA<HTMLAnchorElement>(element) && name == "href" &&
          To<HTMLAnchorElement>(element)->Href().Protocol() == "javascript") {
        drop = true;
      } else if (IsA<HTMLAreaElement>(element) && name == "href" &&
                 To<HTMLAreaElement>(element)->Href().Protocol() ==
                     "javascript") {
        drop = true;
      } else if (IsA<HTMLFormElement>(element) && name == "action" &&
                 To<HTMLFormElement>(element)->action().StartsWith(
                     "javascript:")) {
        // 10. If |element|'s [=element interface=] is {{HTMLFormElement}} and
        // |element|'s `action` attribute is a [[URL]] with `javascript:`
        // protocol, them drop it.
        drop = true;
      } else if ((IsA<HTMLInputElement>(element) ||
                  IsA<HTMLButtonElement>(element)) &&
                 name == "formaction" &&
                 To<HTMLFormControlElement>(element)->formAction().StartsWith(
                     "javascript:")) {
        // 11. If |element|'s [=element interface=] is {{HTMLInputElement}}
        // or {{HTMLButtonElement}} and |element|'s `action` attribute is a
        // [[URL]] with `javascript:` protocol, them drop it.
        drop = true;
      }

      if (drop) {
        element->removeAttribute(name);
        UseCounter::Count(window->GetExecutionContext(),
                          WebFeature::kSanitizerAPIActionTaken);
      }
    }
  }
  node = NodeTraversal::Next(*node, fragment);
  return node;
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
