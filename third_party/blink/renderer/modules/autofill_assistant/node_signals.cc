// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

bool IsVisible(const Element& element) {
  const ComputedStyle* computed_style = element.GetComputedStyle();
  if (computed_style && computed_style->Visibility() != EVisibility::kVisible) {
    return false;
  }

  FloatRect rect = element.GetBoundingClientRectNoLifecycleUpdate();
  if (rect.width() < 1e-6 || rect.height() < 1e-6) {
    return false;
  }

  return true;
}

WebString GetAttributes(const Element& element,
                        const WebVector<QualifiedName>& attributes) {
  std::string result;
  std::string separator;
  for (const QualifiedName& attribute : attributes) {
    const auto& attribute_value = element.getAttribute(attribute);
    if (!attribute_value.IsNull() && !attribute_value.IsEmpty()) {
      result += separator + attribute_value.Utf8();
      separator = " ";
    }
  }
  return WebString::FromUTF8(result);
}

void AddAtomicIfNotNullOrEmpty(const AtomicString& atomic,
                               WebVector<WebString>* text) {
  if (!atomic.IsNull() && !atomic.IsEmpty()) {
    text->emplace_back(WebString(atomic));
  }
}

void AddStringIfNotNullOrEmpty(const WebString& web_string,
                               WebVector<WebString>* text) {
  if (!web_string.IsNull() && !web_string.IsEmpty()) {
    text->emplace_back(web_string);
  }
}

absl::optional<WebVector<Element*>> ListOptions(const Element& element) {
  // TODO(sandromaggi): Handle radio buttons.

  if (!element.HasTagName(html_names::kSelectTag)) {
    return absl::nullopt;
  }

  WebVector<Element*> options;
  for (Element& child : ElementTraversal::DescendantsOf(element)) {
    if (!child.HasTagName(html_names::kOptionTag)) {
      continue;
    }
    options.emplace_back(&child);
  }

  return options;
}

WebString GetType(const Element& element) {
  if (!element.HasTagName(html_names::kInputTag)) {
    return WebString();
  }
  return WebString(element.getAttribute(html_names::kTypeAttr));
}

WebString GetAria(const Element& element) {
  WebVector<QualifiedName> attributes;
  attributes.emplace_back(html_names::kAriaLabelAttr);
  attributes.emplace_back(html_names::kAriaDescriptionAttr);
  attributes.emplace_back(html_names::kAriaPlaceholderAttr);
  return GetAttributes(element, attributes);
}

WebString GetInvisibleAttributes(const Element& element) {
  WebVector<QualifiedName> attributes;
  attributes.emplace_back(html_names::kNameAttr);
  attributes.emplace_back(html_names::kTitleAttr);
  attributes.emplace_back(html_names::kLabelAttr);
  attributes.emplace_back(html_names::kPatternAttr);
  return GetAttributes(element, attributes);
}

void GetUserFacingText(const Element& element,
                       const absl::optional<WebVector<Element*>> options,
                       WebVector<WebString>* text) {
  AddAtomicIfNotNullOrEmpty(element.getAttribute(html_names::kPlaceholderAttr),
                            text);

  // The "value" attribute is treated as user-facing only for text inputs that
  // are "readonly".
  if (element.HasTagName(html_names::kInputTag) &&
      element.hasAttribute(html_names::kReadonlyAttr)) {
    AddAtomicIfNotNullOrEmpty(element.getAttribute(html_names::kValueAttr),
                              text);
  }

  if (options) {
    for (Element* option : *options) {
      AddStringIfNotNullOrEmpty(option->GetInnerTextWithoutUpdate(), text);
    }
  }
}

void CollectNodeFeatures(const Element& element,
                         AutofillAssistantNodeFeatures* features) {
  features->html_tag = element.tagName();
  GetUserFacingText(element, ListOptions(element), &features->text);
  features->aria = GetAria(element);
  features->invisible_attributes = GetInvisibleAttributes(element);
  features->type = GetType(element);
}

// Add label name based on ancestors.
// Example: <label>Label <input></label>
WebString GetLabelRelatedChild(const Element& element) {
  Node* document = element.ownerDocument();
  Node* node = element.parentNode();
  while (node != document) {
    if (!node) {
      break;
    }
    if (node->IsElementNode() && node->HasTagName(html_names::kLabelTag)) {
      Element* label = DynamicTo<Element>(node);
      if (!IsVisible(*label)) {
        return WebString();
      } else {
        return label->GetInnerTextWithoutUpdate();
      }
    }
    node = node->parentNode();
  }
  return WebString();
}

// Add label based on attribute "for".
// Example: <label for="someid">Label</label><input id="someid">
WebString GetLabelRelatedAttribute(
    const Element& element,
    const HashMap<AtomicString, Member<Element>>& labels) {
  AtomicString id = element.getAttribute(html_names::kIdAttr);
  if (id.IsNull() || id.IsEmpty()) {
    return WebString();
  }

  const auto it = labels.find(id);
  if (it == labels.end() || !it->value) {
    return WebString();
  }
  return it->value->GetInnerTextWithoutUpdate();
}

// Get labels using the aria-labelledby attribute.
// Example: <div id="label">Label</div><input aria-labelledby="label">
void AddLabelRelatedAriaLabelledby(const Element& element,
                                   WebVector<WebString>* text) {
  // TODO(b/204839535): Find out if we need html_names::kAriaLabeledbyAttr.
  AtomicString labelledby =
      element.getAttribute(html_names::kAriaLabelledbyAttr);
  if (labelledby.IsNull() || labelledby.IsEmpty()) {
    return;
  }

  const Document& owner = element.GetDocument();

  SpaceSplitString split(labelledby);
  for (wtf_size_t i = 0u; i < split.size(); ++i) {
    Element* label = owner.getElementById(split[i]);
    if (!label || !IsVisible(*label)) {
      continue;
    }
    AddStringIfNotNullOrEmpty(label->GetInnerTextWithoutUpdate(), text);
  }
}

// Add label based on geometric position.
// Example: <label>my_label</label><input>
WebString GetLabelFromGeometry(const Element& element) {
  // TODO(b/204839535): Implement.
  return WebString();
}

void CollectLabelFeatures(const Element& element,
                          const HashMap<AtomicString, Member<Element>>& labels,
                          AutofillAssistantLabelFeatures* features) {
  AddStringIfNotNullOrEmpty(GetLabelRelatedChild(element), &features->text);
  AddStringIfNotNullOrEmpty(GetLabelRelatedAttribute(element, labels),
                            &features->text);
  AddLabelRelatedAriaLabelledby(element, &features->text);
  AddStringIfNotNullOrEmpty(GetLabelFromGeometry(element), &features->text);
}

void CollectContextFeatures(const Element& element,
                            AutofillAssistantContextFeatures* features) {
  // TODO(b/204839535): Implement
}

void CollectSignalsForNode(
    const Node& node,
    WebVector<AutofillAssistantNodeSignals>* node_signals) {
  // Pre-process elements for this Node.
  HashMap<AtomicString, Member<Element>> labels;
  for (Element& element : ElementTraversal::DescendantsOf(node)) {
    if (element.HasTagName(html_names::kLabelTag) &&
        element.hasAttribute(html_names::kForAttr)) {
      labels.insert(element.getAttribute(html_names::kForAttr), &element);
    }
  }

  // Note: This does not pierce documents like ShadowDom.
  for (Element& element : ElementTraversal::DescendantsOf(node)) {
    ShadowRoot* shadow_root = element.GetShadowRoot();
    if (shadow_root && shadow_root->GetType() != ShadowRootType::kUserAgent) {
      CollectSignalsForNode(*shadow_root, node_signals);
    }

    if (!element.HasTagName(html_names::kInputTag) &&
        !element.HasTagName(html_names::kTextareaTag) &&
        !element.HasTagName(html_names::kSelectTag)) {
      continue;
    }
    if (!IsVisible(element)) {
      continue;
    }
    AutofillAssistantNodeSignals signals;
    signals.backend_node_id = static_cast<int>(DOMNodeIds::IdForNode(&element));
    CollectNodeFeatures(element, &signals.node_features);
    CollectLabelFeatures(element, labels, &signals.label_features);
    CollectContextFeatures(element, &signals.context_features);
    node_signals->push_back(std::move(signals));
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
