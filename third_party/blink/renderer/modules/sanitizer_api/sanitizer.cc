// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_parse_from_string_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_documentfragment_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_documentfragment_string_trustedhtml.h"
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

namespace {

bool ConfigIsEmpty(const SanitizerConfig* config) {
  return !config ||
         (!config->hasDropElements() && !config->hasBlockElements() &&
          !config->hasAllowElements() && !config->hasDropAttributes() &&
          !config->hasAllowAttributes() && !config->hasAllowCustomElements());
}

SanitizerConfig* SanitizerConfigCopy(const SanitizerConfig* config) {
  if (!config)
    return nullptr;

  SanitizerConfig* copy = SanitizerConfig::Create();
  if (config->hasAllowAttributes()) {
    copy->setAllowAttributes(config->allowAttributes());
  }
  if (config->hasAllowCustomElements()) {
    copy->setAllowCustomElements(config->allowCustomElements());
  }
  if (config->hasAllowElements()) {
    copy->setAllowElements(config->allowElements());
  }
  if (config->hasBlockElements()) {
    copy->setBlockElements(config->blockElements());
  }
  if (config->hasDropAttributes()) {
    copy->setDropAttributes(config->dropAttributes());
  }
  if (config->hasDropElements()) {
    copy->setDropElements(config->dropElements());
  }
  return copy;
}

}  // anonymous namespace

Sanitizer::Sanitizer(ExecutionContext* execution_context,
                     const SanitizerConfig* config) {
  // The spec treats an absent config as the default. We'll handle this by
  // normalizing this here to make sure the config_dictionary_ is nullptr
  // in these cases, while the config_ will be a copy of the default config.
  if (ConfigIsEmpty(config)) {
    config = nullptr;
  }

  config_ = SanitizerConfigImpl::From(config);
  config_dictionary_ = SanitizerConfigCopy(config);
  if (!config_dictionary_) {
    UseCounter::Count(execution_context,
                      WebFeature::kSanitizerAPIDefaultConfiguration);
  }
}

Sanitizer::~Sanitizer() = default;

Sanitizer* Sanitizer::Create(ExecutionContext* execution_context,
                             const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(execution_context, config);
}

String Sanitizer::sanitizeToString(ScriptState* script_state,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   const V8SanitizerInput* input,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   StringOrDocumentFragmentOrDocument& input,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                   ExceptionState& exception_state) {
  return CreateMarkup(SanitizeImpl(script_state, input, exception_state),
                      kChildrenOnly);
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
DocumentFragment* Sanitizer::sanitize(
    ScriptState* script_state,
    const V8SanitizerInputWithTrustedHTML* input,
    ExceptionState& exception_state) {
  V8SanitizerInput* new_input = nullptr;
  switch (input->GetContentType()) {
    case V8SanitizerInputWithTrustedHTML::ContentType::kDocument:
      new_input =
          MakeGarbageCollected<V8SanitizerInput>(input->GetAsDocument());
      break;
    case V8SanitizerInputWithTrustedHTML::ContentType::kDocumentFragment:
      new_input = MakeGarbageCollected<V8SanitizerInput>(
          input->GetAsDocumentFragment());
      break;
    case V8SanitizerInputWithTrustedHTML::ContentType::kString: {
      LocalDOMWindow* window = LocalDOMWindow::From(script_state);
      if (!window) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "Cannot find current DOM window.");
        return nullptr;
      }
      new_input =
          MakeGarbageCollected<V8SanitizerInput>(TrustedTypesCheckForHTML(
              input->GetAsString(), window->GetExecutionContext(),
              exception_state));
      if (exception_state.HadException()) {
        return nullptr;
      }
      break;
    }
    case V8SanitizerInputWithTrustedHTML::ContentType::kTrustedHTML:
      new_input = MakeGarbageCollected<V8SanitizerInput>(
          input->GetAsTrustedHTML()->toString());
      break;
  }
  return SanitizeImpl(script_state, new_input, exception_state);
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
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
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
DocumentFragment* Sanitizer::PrepareFragment(LocalDOMWindow* window,
                                             ScriptState* script_state,
                                             const V8SanitizerInput* input,
                                             ExceptionState& exception_state) {
  DCHECK(input);

  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot find current DOM window.");
    return nullptr;
  }

  switch (input->GetContentType()) {
    case V8SanitizerInput::ContentType::kDocument: {
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIFromDocument);
      DocumentFragment* fragment =
          input->GetAsDocument()->createDocumentFragment();
      fragment->CloneChildNodesFrom(*(input->GetAsDocument()->body()),
                                    CloneChildrenFlag::kClone);
      return fragment;
    }
    case V8SanitizerInput::ContentType::kDocumentFragment:
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIFromFragment);
      return input->GetAsDocumentFragment();
    case V8SanitizerInput::ContentType::kString: {
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIFromString);
      Document* document =
          window->document()
              ? window->document()->implementation().createHTMLDocument()
              : DOMParser::Create(script_state)
                    ->parseFromString(
                        "<!DOCTYPE html><html><body></body></html>",
                        "text/html", ParseFromStringOptions::Create());
      // TODO(https://crbug.com/1178774): Behavior difference need further
      // investgate.
      return document->createRange()->createContextualFragment(
          input->GetAsString(), exception_state);
    }
  }
  NOTREACHED();
  return nullptr;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
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
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

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
    } else if (is_custom_element && !config_.allow_custom_elements_) {
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
    } else if (config_.drop_elements_.Contains(name)) {
      // 5. If |name| is in |config|'s [=element drop list=] then 'drop'.
      node = DropElement(node, fragment);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (config_.block_elements_.Contains(name)) {
      // 6. If |name| is in |config|'s [=element block list=] then 'block'.
      node = BlockElement(node, fragment, exception_state);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    } else if (!config_.allow_elements_.Contains(name)) {
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

DocumentFragment* Sanitizer::SanitizeImpl(ScriptState* script_state,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                          const V8SanitizerInput* input,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
                                          StringOrDocumentFragmentOrDocument&
                                              input,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
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
  if (config_.allow_attributes_.at("*").Contains(node_name)) {
  } else if (config_.drop_attributes_.at("*").Contains(node_name)) {
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
                  (config_.drop_attributes_.Contains(name) &&
                   (config_.drop_attributes_.at(name) == kVectorStar ||
                    config_.drop_attributes_.at(name).Contains(node_name))) ||
                  !(config_.allow_attributes_.Contains(name) &&
                    (config_.allow_attributes_.at(name) == kVectorStar ||
                     config_.allow_attributes_.at(name).Contains(node_name)));
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

SanitizerConfig* Sanitizer::config() const {
  return SanitizerConfigCopy(config_dictionary_
                                 ? config_dictionary_.Get()
                                 : SanitizerConfigImpl::defaultConfig());
}

SanitizerConfig* Sanitizer::defaultConfig() {
  return SanitizerConfigCopy(SanitizerConfigImpl::defaultConfig());
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(config_dictionary_);
}

}  // namespace blink
