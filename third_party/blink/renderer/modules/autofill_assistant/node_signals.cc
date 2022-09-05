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
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

constexpr int kMaxAncestorLevelForGeometry = 5;
constexpr double kToleranceForGeometry = 1e-5;
constexpr int kMaxCSSPixelDistance = 30;

constexpr char kShippingFormType[] = "SHIPPING";
constexpr char kBillingFormType[] = "BILLING";
constexpr char kAfterLabel[] = "AFTRLBL";

WebString JoinStrings(const WebVector<WebString>& strings,
                      const char* separator) {
  StringBuilder builder;
  for (wtf_size_t i = 0; i < strings.size(); ++i) {
    if (i > 0) {
      builder.Append(separator);
    }
    builder.Append(String::FromUTF8(strings[i].Utf8()));
  }
  return WebString::FromUTF8(builder.ToString().StripWhiteSpace().Utf8());
}

bool IsVisible(const Element& element) {
  const ComputedStyle* computed_style = element.GetComputedStyle();
  if (computed_style && computed_style->Visibility() != EVisibility::kVisible) {
    return false;
  }

  return !element.BoundingBox().IsEmpty();
}

WebString GetAttributes(const Element& element,
                        const WebVector<QualifiedName>& attributes) {
  WebVector<WebString> values;
  for (const QualifiedName& attribute : attributes) {
    const auto& attribute_value = element.getAttribute(attribute);
    if (!attribute_value.IsNull() && !attribute_value.IsEmpty()) {
      values.push_back(attribute_value);
    }
  }
  return JoinStrings(values, " ");
}

bool AddAtomicIfNotNullOrEmpty(const AtomicString& atomic,
                               WebVector<WebString>* text) {
  if (atomic.IsNull() || atomic.IsEmpty()) {
    return false;
  }

  text->emplace_back(WebString(atomic));
  return true;
}

bool AddStringIfNotNullOrEmpty(const WebString& web_string,
                               WebVector<WebString>* text) {
  if (web_string.IsNull() || web_string.IsEmpty()) {
    return false;
  }

  text->emplace_back(web_string);
  return true;
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

bool IsSupportedByClient(const Element& element) {
  if (element.HasTagName(html_names::kInputTag)) {
    const String input_type = String{GetType(element)}.LowerASCII();
    return input_type != "checkbox" && input_type != "radio" &&
           input_type != "submit" && input_type != "button" &&
           input_type != "hidden";
  }
  return element.HasTagName(html_names::kTextareaTag) ||
         element.HasTagName(html_names::kSelectTag);
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
      AddStringIfNotNullOrEmpty(option->innerText(), text);
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
  Node* node = element.parentNode();
  while (node) {
    if (node->HasTagName(html_names::kLabelTag)) {
      Element* label = DynamicTo<Element>(node);
      if (!IsVisible(*label)) {
        return WebString();
      } else {
        return label->innerText();
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
  return it->value->innerText();
}

// Get labels using the aria-labelledby attribute.
// Example: <div id="label">Label</div><input aria-labelledby="label">
bool AddLabelRelatedAriaLabelledby(const Element& element,
                                   WebVector<WebString>* text) {
  // TODO(b/204839535): Find out if we need html_names::kAriaLabeledbyAttr.
  AtomicString labelledby =
      element.getAttribute(html_names::kAriaLabelledbyAttr);
  if (labelledby.IsNull() || labelledby.IsEmpty()) {
    return false;
  }

  bool added_label = false;
  const Document& owner = element.GetDocument();

  SpaceSplitString split(labelledby);
  for (wtf_size_t i = 0u; i < split.size(); ++i) {
    Element* label = owner.getElementById(split[i]);
    if (!label || !IsVisible(*label)) {
      continue;
    }
    added_label =
        AddStringIfNotNullOrEmpty(label->innerText(), text) || added_label;
  }
  return added_label;
}

double Distance(const PhysicalRect& a, const PhysicalRect& b) {
  // There are only three ways rectangles can be positioned on each axis. For
  // the y-axis we have:
  //
  // 1. b is above a then the y-axis distance is the y coordinate of
  //    b.y() - a.bottom()
  // 2. b is below a then the y-axis distance is the y coordinate of
  //    a.y() - b.bottom()
  // 3. b and a intersect in the y-axis (i.e. their projection on the y-axis
  //    intersect), then the distance on the y-axis is zero.
  //
  // The max defines what is the current situation.
  double delta_y = std::max(
      {(b.Y() - a.Bottom()).ToDouble(), (a.Y() - b.Bottom()).ToDouble(), 0.0});
  double delta_x = std::max(
      {(b.X() - a.Right()).ToDouble(), (a.X() - b.Right()).ToDouble(), 0.0});
  return std::sqrt(delta_x * delta_x + delta_y * delta_y);
}

// Add label based on geometric position.
// Example: <label>my_label</label><input>
WebString GetLabelFromGeometry(const Element& element) {
  const PhysicalRect& element_rect = element.BoundingBox();

  const Document* root = element.ownerDocument();
  int step = 0;
  const Node* ancestor = &element;
  while (ancestor != root && step < kMaxAncestorLevelForGeometry) {
    ancestor = ancestor->parentNode();
    if (!ancestor) {
      return WebString();
    }
    ++step;
  }

  WebVector<std::pair<double, WebString>> distance_and_text;
  double closest_distance = std::numeric_limits<double>::max();
  for (Node& node : NodeTraversal::DescendantsOf(*ancestor)) {
    if (!node.IsTextNode()) {
      continue;
    }
    const PhysicalRect& rect = node.BoundingBox();
    if (rect.Y() > 0.5 * (element_rect.Y() + element_rect.Bottom()) &&
        !element_rect.Contains(rect)) {
      continue;
    }
    double distance = Distance(rect, element_rect);
    if (distance > kMaxCSSPixelDistance ||
        distance > closest_distance + kToleranceForGeometry) {
      // No need to check text if element is far away.
      continue;
    }

    WebString text(DynamicTo<Text>(node)->wholeText());
    if (!text.IsNull() && !text.IsEmpty()) {
      distance_and_text.push_back({distance, text});
      if (distance < closest_distance) {
        closest_distance = distance;
      }
    }
  }

  if (closest_distance > kMaxCSSPixelDistance) {
    return WebString();
  }
  WebVector<WebString> best_text;
  for (const auto& entry : distance_and_text) {
    if (entry.first <= closest_distance + kToleranceForGeometry) {
      best_text.push_back(entry.second);
    }
  }
  return JoinStrings(best_text, " ");
}

void CollectLabelFeatures(Element& element,
                          const HashMap<AtomicString, Member<Element>>& labels,
                          AutofillAssistantLabelFeatures* features) {
  if (AddStringIfNotNullOrEmpty(GetLabelRelatedChild(element),
                                &features->text)) {
    return;
  }
  if (AddStringIfNotNullOrEmpty(GetLabelRelatedAttribute(element, labels),
                                &features->text)) {
    return;
  }
  if (AddLabelRelatedAriaLabelledby(element, &features->text)) {
    return;
  }
  AddStringIfNotNullOrEmpty(GetLabelFromGeometry(element), &features->text);
}

void AddFirstLegendOfFieldset(Node* node, WebVector<WebString>* header_text) {
  Element* fieldset = DynamicTo<Element>(node);
  if (!IsVisible(*fieldset)) {
    return;
  }
  for (Element& legend : ElementTraversal::DescendantsOf(*fieldset)) {
    // Add the first legend only.
    if (legend.HasTagName(html_names::kLegendTag) &&
        AddStringIfNotNullOrEmpty(legend.innerText(), header_text)) {
      return;
    }
  }
}

// Add text signals when the |element| is a descendant of one or more
// fieldset(s).
// Example: <fieldset><legend>Legend</legend><input/><fieldset>
void GetFieldsetLegends(const Element& element,
                        AutofillAssistantContextFeatures* features) {
  Node* node = element.parentNode();
  while (node) {
    if (node->HasTagName(html_names::kFormTag)) {
      break;
    }
    if (node->HasTagName(html_names::kFieldsetTag)) {
      AddFirstLegendOfFieldset(node, &features->header_text);
    }
    node = node->parentNode();
  }
}

bool IsHeader(const Element& element) {
  return element.HasTagName(html_names::kH1Tag) ||
         element.HasTagName(html_names::kH2Tag) ||
         element.HasTagName(html_names::kH3Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH5Tag);
}

void AddHeaders(const Element& element,
                AutofillAssistantContextFeatures* features) {
  WebVector<Element*> headers_before;
  for (Element& node :
       ElementTraversal::DescendantsOf(*element.ownerDocument())) {
    if (&node == &element) {
      break;
    }
    if (IsHeader(node) && IsVisible(node)) {
      headers_before.emplace_back(&node);
    }
  }
  for (int i = static_cast<int>(headers_before.size()) - 1; i >= 0; --i) {
    Element* header = headers_before[i];
    // Demand header to be above the element or to have an overlap in
    // y-coordinates. I.e. top (Y) of header must be higher (<) than bottom
    // of element.
    if (header->BoundingBox().Y() >= element.BoundingBox().Bottom()) {
      continue;
    }
    // Only use the header which is the lowest in hierarchy.
    if (AddStringIfNotNullOrEmpty(header->innerText(),
                                  &features->header_text)) {
      break;
    }
  }
}

bool HasVisibleParent(const Node& node) {
  if (!node.parentNode() || !node.parentNode()->IsElementNode()) {
    return false;
  }

  return IsVisible(*DynamicTo<Element>(*node.parentNode()));
}

bool IsShippingFormObjective(const AtomicString& text) {
  return text.Contains("ship", kTextCaseUnicodeInsensitive) ||
         text.Contains("deliver", kTextCaseUnicodeInsensitive);
}

bool IsBillingFormObjective(const AtomicString& text) {
  return text.Contains("billing", kTextCaseUnicodeInsensitive) ||
         text.Contains("payment", kTextCaseUnicodeInsensitive);
}

WebVector<WebString> GetObjectivesFromAtomicString(const AtomicString& text) {
  WebVector<WebString> objectives;
  if (IsShippingFormObjective(text)) {
    objectives.push_back(WebString::FromUTF8(kShippingFormType));
  }
  if (IsBillingFormObjective(text)) {
    objectives.push_back(WebString::FromUTF8(kBillingFormType));
  }
  return objectives;
}

WebVector<WebString> GetObjectivesFromTextNode(const Node& node) {
  return GetObjectivesFromAtomicString(
      AtomicString(DynamicTo<Text>(node)->wholeText()));
}

WebVector<WebString> GetObjectivesFromElementNode(const Node& node) {
  const Element* element = DynamicTo<Element>(node);
  return GetObjectivesFromAtomicString(AtomicString(
      String::FromUTF8(element->getAttribute(html_names::kIdAttr).Utf8() + " " +
                       element->getAttribute(html_names::kNameAttr).Utf8())));
}

bool HasLabelAncestor(const Node& node) {
  const Node* iter = &node;
  while (iter) {
    if (iter->HasTagName(html_names::kLabelTag)) {
      return true;
    }
    iter = iter->parentNode();
  }
  return false;
}

void AddFormTypeForContext(const Element& element,
                           AutofillAssistantContextFeatures* features) {
  WebVector<WebString> best_objectives;
  // First attempt: Look at text nodes.
  // |after_label| captures the presence of a label between the header and the
  // input, typically a checkbox "billing same as shipping".
  bool after_label = false;
  for (Node& node : NodeTraversal::DescendantsOf(*element.ownerDocument())) {
    if (&node == &element) {
      break;
    }
    if (!node.IsTextNode() || !HasVisibleParent(node)) {
      continue;
    }

    WebVector<WebString> objectives = GetObjectivesFromTextNode(node);
    if (objectives.empty()) {
      continue;
    }

    if (HasLabelAncestor(node)) {
      after_label = true;
    } else {
      after_label = false;
      best_objectives = objectives;
    }
  }

  if (best_objectives.empty()) {
    // Second attempt: Try with id and names of ancestor nodes.
    after_label = false;
    Node* iter = element.parentNode();
    while (iter && !iter->IsDocumentNode()) {
      if (iter->IsElementNode()) {
        WebVector<WebString> objectives = GetObjectivesFromElementNode(*iter);
        if (!objectives.empty()) {
          best_objectives = objectives;
        }
      }
      iter = iter->parentNode();
    }
  }

  if (best_objectives.empty()) {
    return;
  }

  WebVector<WebString> form_type;
  if (after_label) {
    form_type.push_back(kAfterLabel);
  }
  for (const WebString& objective : best_objectives) {
    form_type.push_back(objective);
  }
  features->form_type = JoinStrings(form_type, " ");
}

void CollectContextFeatures(const Element& element,
                            AutofillAssistantContextFeatures* features) {
  GetFieldsetLegends(element, features);
  AddHeaders(element, features);
  AddFormTypeForContext(element, features);
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

    if (!IsSupportedByClient(element) || !IsVisible(element)) {
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
