// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_filter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_parse_from_string_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_documentfragment.h"
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
          !config->hasAllowAttributes() && !config->hasAllowCustomElements() &&
          !config->hasAllowComments());
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
  if (config->hasAllowComments()) {
    copy->setAllowComments(config->allowComments());
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
  // checking for this an empty dictionary and representing it as nullptr.
  if (ConfigIsEmpty(config)) {
    config = nullptr;
    UseCounter::Count(execution_context,
                      WebFeature::kSanitizerAPIDefaultConfiguration);
  }
  config_ = SanitizerConfigImpl::From(config);
}

Sanitizer::~Sanitizer() = default;

Sanitizer* Sanitizer::Create(ExecutionContext* execution_context,
                             const SanitizerConfig* config,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>(execution_context, config);
}

DocumentFragment* Sanitizer::sanitize(ScriptState* script_state,
                                      const V8SanitizerInput* input,
                                      ExceptionState& exception_state) {
  V8SanitizerInput* new_input = nullptr;
  switch (input->GetContentType()) {
    case V8SanitizerInput::ContentType::kDocument:
      new_input =
          MakeGarbageCollected<V8SanitizerInput>(input->GetAsDocument());
      break;
    case V8SanitizerInput::ContentType::kDocumentFragment:
      new_input = MakeGarbageCollected<V8SanitizerInput>(
          input->GetAsDocumentFragment());
      break;
  }
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentFragment* fragment =
      PrepareFragment(window, script_state, new_input, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  DoSanitizing(fragment, window, exception_state);
  return fragment;
}

Element* Sanitizer::sanitizeFor(ScriptState* script_state,
                                const String& local_name,
                                const String& markup,
                                ExceptionState& exception_state) {
  if (baseline_drop_elements_.Contains(local_name.LowerASCII()))
    return nullptr;
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  Document* inert_document = DocumentInit::Create()
                                 .WithURL(window->Url())
                                 .WithTypeFrom("text/html")
                                 .WithExecutionContext(window)
                                 .CreateDocument();
  Element* element = inert_document->CreateElementForBinding(
      AtomicString(local_name), exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return nullptr;
  }
  element->setInnerHTML(markup, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return nullptr;
  }
  DoSanitizing(element, window, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return nullptr;
  }
  // Edge case: The template element treatment also applies to the newly created
  // element in .sanitizeFor.
  if (IsA<HTMLTemplateElement>(element)) {
    DoSanitizing(To<HTMLTemplateElement>(element)->content(), window,
                 exception_state);
    if (exception_state.HadException()) {
      exception_state.ClearException();
      return nullptr;
    }
  }
  return element;
}

void Sanitizer::ElementSetHTML(ScriptState* script_state,
                               Element& element,
                               const String& markup,
                               ExceptionState& exception_state) {
  Element* new_element =
      sanitizeFor(script_state, element.localName(), markup, exception_state);
  if (exception_state.HadException())
    return;
  if (!new_element) {
    exception_state.ThrowTypeError("setHTML not allowed on this element type.");
    return;
  }
  element.RemoveChildren();
  while (Node* to_be_moved = new_element->firstChild())
    element.AppendChild(to_be_moved);
}

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
  }
  NOTREACHED();
  return nullptr;
}

void Sanitizer::DoSanitizing(ContainerNode* fragment,
                             LocalDOMWindow* window,
                             ExceptionState& exception_state) {
  Node* node = fragment->firstChild();

  while (node) {
    switch (node->getNodeType()) {
      case Node::NodeType::kElementNode: {
        // TODO(crbug.com/1126936): Review the sanitising algorithm for
        // non-HTMLs.
        // 1. Let |name| be |element|'s tag name.
        String name = node->nodeName().LowerASCII();

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
          // 4. If |kind| is `custom` and if allow_custom_elements_ is unset or
          // set to anything other than `true`, then 'drop'.
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
          // 7. if |name| is not in |config|'s [=element allow list=] then
          // 'block'.
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
        break;
      }
      case Node::NodeType::kTextNode:
        // Text node: Keep (by skipping over it).
        node = NodeTraversal::Next(*node, fragment);
        break;
      case Node::NodeType::kCommentNode:
        // Comment: Drop (unless allowed by config).
        node = config_.allow_comments_ ? NodeTraversal::Next(*node, fragment)
                                       : DropElement(node, fragment);
        break;
      case Node::NodeType::kDocumentNode:
      case Node::NodeType::kDocumentFragmentNode:
        // Document & DocumentFragment: Drop (unless it's the root).
        node = !node->parentNode() ? NodeTraversal::Next(*node, fragment)
                                   : DropElement(node, fragment);
        break;
      default:
        // Default: Drop anything not explicitly handled.
        node = DropElement(node, fragment);
        break;
    }
  }
}

// If the current element needs to be dropped, remove current element entirely
// and proceed to its next sibling.
Node* Sanitizer::DropElement(Node* node, ContainerNode* fragment) {
  Node* tmp = node;
  node = NodeTraversal::NextSkippingChildren(*node, fragment);
  tmp->remove();
  return node;
}

// If the current element should be blocked, append its children after current
// node to parent node, remove current element and proceed to the next node.
Node* Sanitizer::BlockElement(Node* node,
                              ContainerNode* fragment,
                              ExceptionState& exception_state) {
  ContainerNode* parent = node->parentNode();
  Node* next_sibling = node->nextSibling();
  while (Node* child = node->firstChild()) {
    parent->InsertBefore(child, next_sibling, exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
  }
  Node* tmp = node;
  node = NodeTraversal::Next(*node, fragment);
  tmp->remove();
  return node;
}

// Helper to check whether a given attribute match list matches an attribute /
// element name pair. This observes wildcard ("*") as element name.
bool Sanitizer::AttrListMatches(const HashMap<String, Vector<String>>& map,
                                const String& attr,
                                const String& element) {
  const auto node_iter = map.find(attr);
  return (node_iter != map.end()) && (node_iter->value == kVectorStar ||
                                      node_iter->value.Contains(element));
}

// Remove any attributes to be dropped from the current element, and proceed to
// the next node (preorder, depth-first traversal).
Node* Sanitizer::KeepElement(Node* node,
                             ContainerNode* fragment,
                             String& node_name,
                             LocalDOMWindow* window) {
  Element* element = To<Element>(node);
  if (AttrListMatches(config_.allow_attributes_, "*", node_name)) {
  } else if (AttrListMatches(config_.drop_attributes_, "*", node_name)) {
    for (const auto& name : element->getAttributeNames()) {
      element->removeAttribute(name);
      UseCounter::Count(window->GetExecutionContext(),
                        WebFeature::kSanitizerAPIActionTaken);
    }
  } else {
    for (const auto& name : element->getAttributeNames()) {
      // Attributes in drop list or not in allow list while allow list
      // exists will be dropped.
      bool drop = AttrListMatches(baseline_drop_attributes_, name, node_name) ||
                  AttrListMatches(config_.drop_attributes_, name, node_name) ||
                  !AttrListMatches(config_.allow_attributes_, name, node_name);
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

SanitizerConfig* Sanitizer::getConfiguration() const {
  return SanitizerConfigImpl::ToAPI(config_);
}

SanitizerConfig* Sanitizer::getDefaultConfiguration() {
  return SanitizerConfigCopy(SanitizerConfigImpl::DefaultConfig());
}

Sanitizer* Sanitizer::getDefaultInstance() {
  DEFINE_STATIC_LOCAL(
      Persistent<Sanitizer>, default_sanitizer_,
      (Sanitizer::Create(nullptr, nullptr, ASSERT_NO_EXCEPTION)));
  return default_sanitizer_;
}

void Sanitizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
