// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/select_mutation_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_no_script_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SelectMutationObserver::SelectMutationObserver(HTMLSelectElement& select)
    : select_(select), observer_(MutationObserver::Create(this)) {
  DCHECK(select_->IsAppearanceBase());

  MutationObserverInit* init = MutationObserverInit::Create();
  init->setChildList(true);
  init->setSubtree(true);
  init->setAttributes(true);
  observer_->observe(select_, init, ASSERT_NO_EXCEPTION);
  // Traverse descendants that have been added to the select so far.
  TraverseNodeDescendants(select_);
}

ExecutionContext* SelectMutationObserver::GetExecutionContext() const {
  return select_->GetExecutionContext();
}

void SelectMutationObserver::Deliver(const MutationRecordVector& records,
                                     MutationObserver&) {
  for (const auto& record : records) {
    if (record->type() == "childList") {
      CheckAddedNodes(record);
      CheckRemovedNodes(record);
    } else if (record->type() == "attributes") {
      if (record->attributeName() == html_names::kTabindexAttr ||
          record->attributeName() == html_names::kContenteditableAttr) {
        AddDescendantDisallowedErrorToNode(*record->target());
      }
    }
  }
}

void SelectMutationObserver::Disconnect() {
  observer_->disconnect();
}

void SelectMutationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(select_);
  visitor->Trace(observer_);
  MutationObserver::Delegate::Trace(visitor);
}

void SelectMutationObserver::CheckAddedNodes(MutationRecord* record) {
  DCHECK(record);
  auto* added_nodes = record->addedNodes();
  for (unsigned i = 0; i < added_nodes->length(); ++i) {
    auto* descendant = added_nodes->item(i);
    DCHECK(descendant);
    if (IsWhitespaceOrEmpty(*descendant)) {
      continue;
    }
    AddDescendantDisallowedErrorToNode(*descendant);
    // Check the added node's descendants, if any.
    TraverseNodeDescendants(descendant);
  }
}

void SelectMutationObserver::CheckRemovedNodes(MutationRecord* record) {
  DCHECK(record);
  auto* removed_nodes = record->removedNodes();
  DCHECK(removed_nodes);
  for (unsigned i = 0; i < removed_nodes->length(); ++i) {
    auto* descendant = removed_nodes->item(i);
    DCHECK(descendant);
    if (IsWhitespaceOrEmpty(*descendant)) {
      continue;
    }
    if (!IsAllowedInteractiveElement(*descendant)) {
      select_->DecreaseContentModelViolationCount();
    }
    // Check the removed node's descendants, if any.
    for (Node* nested_descendant = NodeTraversal::FirstWithin(*descendant);
         nested_descendant; nested_descendant = NodeTraversal::Next(
                                *nested_descendant, descendant)) {
      if (!IsWhitespaceOrEmpty(*nested_descendant) &&
          !IsAllowedInteractiveElement(*nested_descendant)) {
        select_->DecreaseContentModelViolationCount();
      }
    }
  }
}

void SelectMutationObserver::TraverseNodeDescendants(const Node* node) {
  for (Node* descendant = NodeTraversal::FirstWithin(*node); descendant;
       descendant = NodeTraversal::Next(*descendant, node)) {
    if (!IsWhitespaceOrEmpty(*descendant)) {
      AddDescendantDisallowedErrorToNode(*descendant);
    }
  }
}

void SelectMutationObserver::AddDescendantDisallowedErrorToNode(Node& node) {
  ElementAccessibilityIssueReason issue_reason = CheckForIssue(node);
  if (issue_reason != ElementAccessibilityIssueReason::kValidChild) {
    if (!IsAllowedInteractiveElement(node)) {
      select_->IncreaseContentModelViolationCount();
    }
    Document& document = select_->GetDocument();
    AuditsIssue::ReportElementAccessibilityIssue(
        &document, node.GetDomNodeId(), issue_reason,
        /* has_disallowed_attributes = */ HasTabIndexAttribute(node) ||
            IsContenteditable(node));
    RecordIssueByType(issue_reason);
  }
}

bool SelectMutationObserver::IsAllowedInteractiveElement(Node& node) {
  if (IsA<HTMLButtonElement>(node)) {
    // The <button> must have a parent (not being inserted as a child of
    // `HTMLSelectedContentElement`) and must be the first child of the
    // <select>.
    const Node* parent = node.parentNode();
    return parent && IsA<HTMLSelectElement>(*parent) &&
           !ElementTraversal::PreviousSibling(node);
  }
  // If the node isn't a <button> but it is an interactive element, we return
  // false as interactive elements are disallowed.
  return !IsInteractiveElement(node);
}

bool SelectMutationObserver::IsInteractiveElement(const Node& node) {
  if (HasTabIndexAttribute(node)) {
    return true;
  }
  if (auto* html_element = DynamicTo<HTMLElement>(node)) {
    return IsContenteditable(node) || html_element->IsInteractiveContent();
  }
  return false;
}

void SelectMutationObserver::RecordIssueByType(
    ElementAccessibilityIssueReason issue_reason) {
  switch (issue_reason) {
    case ElementAccessibilityIssueReason::kDisallowedSelectChild:
      UseCounter::Count(select_->GetDocument(),
                        WebFeature::kDisallowedSelectChild);
      break;
    case ElementAccessibilityIssueReason::kDisallowedOptGroupChild:
      UseCounter::Count(select_->GetDocument(),
                        WebFeature::kDisallowedOptGroupChild);
      break;
    case ElementAccessibilityIssueReason::kNonPhrasingContentOptionChild:
      UseCounter::Count(select_->GetDocument(),
                        WebFeature::kNonPhrasingContentOptionChild);
      break;
    case ElementAccessibilityIssueReason::kInteractiveContentOptionChild:
      UseCounter::Count(select_->GetDocument(),
                        WebFeature::kInteractiveContentOptionChild);
      break;
    case ElementAccessibilityIssueReason::kInteractiveContentLegendChild:
      UseCounter::Count(select_->GetDocument(),
                        WebFeature::kInteractiveContentLegendChild);
      break;
    case ElementAccessibilityIssueReason::kValidChild:
    default:
      NOTREACHED();
  }
}

ElementAccessibilityIssueReason SelectMutationObserver::CheckForIssue(
    const Node& descendant) {
  if (descendant.getNodeType() == Node::kCommentNode ||
      IsAutonomousCustomElement(descendant)) {
    return ElementAccessibilityIssueReason::kValidChild;
  }
  // Get the parent of the descendant.
  const Node* parent = descendant.parentNode();
  // If the node has no parent, assume it is being appended to a
  // `HTMLSelectedContentElement`.
  if (!parent) {
    return CheckDescedantOfOption(descendant);
  }
  if (!IsA<HTMLElement>(*parent)) {
    if (parent->IsSVGElement()) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    return ElementAccessibilityIssueReason::kDisallowedSelectChild;
  }
  if (IsA<HTMLSelectElement>(*parent)) {
    if (IsAllowedDescendantOfSelect(descendant, *parent)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    return ElementAccessibilityIssueReason::kDisallowedSelectChild;
  }
  if (IsA<HTMLOptGroupElement>(*parent)) {
    if (IsAllowedDescendantOfOptgroup(descendant, *parent)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    return ElementAccessibilityIssueReason::kDisallowedOptGroupChild;
  }
  if (IsA<HTMLOptionElement>(*parent) ||
      IsA<HTMLSelectedContentElement>(*parent) ||
      (IsAllowedPhrasingContent(*parent) && !IsA<HTMLSpanElement>(*parent))) {
    return CheckDescedantOfOption(descendant);
  }
  if (IsA<HTMLDivElement>(*parent) || IsA<HTMLSpanElement>(*parent) ||
      IsAutonomousCustomElement(*parent)) {
    return TraverseAncestorsAndCheckDescendant(descendant);
  }
  if ((IsA<HTMLNoScriptElement>(*parent) || IsA<HTMLScriptElement>(*parent) ||
       IsA<HTMLTemplateElement>(*parent)) &&
      !descendant.IsTextNode()) {
    return TraverseAncestorsAndCheckDescendant(descendant);
  }
  if (IsA<HTMLButtonElement>(*parent)) {
    if (IsAllowedDescendantOfButton(descendant)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    return ElementAccessibilityIssueReason::kDisallowedSelectChild;
  }
  if (IsA<HTMLLegendElement>(*parent)) {
    if (IsAllowedPhrasingContent(descendant) &&
        !HasTabIndexAttribute(descendant) && !IsContenteditable(descendant)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    return ElementAccessibilityIssueReason::kInteractiveContentLegendChild;
  }
  return ElementAccessibilityIssueReason::kDisallowedSelectChild;
}

bool SelectMutationObserver::IsAllowedDescendantOfSelect(const Node& descendant,
                                                         const Node& parent) {
  // <button> has to be the first direct descendant of the <select>.
  return (IsA<HTMLButtonElement>(descendant) &&
          IsA<HTMLSelectElement>(parent) &&
          !ElementTraversal::PreviousSibling(descendant)) ||
         IsA<HTMLOptionElement>(descendant) ||
         IsA<HTMLOptGroupElement>(descendant) ||
         IsA<HTMLHRElement>(descendant) || IsA<HTMLDivElement>(descendant) ||
         IsA<HTMLSpanElement>(descendant) ||
         IsA<HTMLNoScriptElement>(descendant) ||
         IsA<HTMLScriptElement>(descendant) ||
         IsA<HTMLTemplateElement>(descendant);
}

bool SelectMutationObserver::IsAllowedDescendantOfOptgroup(
    const Node& descendant,
    const Node& parent) {
  // <legend> has to be the first direct descendant of the <optgroup>.
  return (IsA<HTMLLegendElement>(descendant) &&
          IsA<HTMLOptGroupElement>(parent) &&
          !ElementTraversal::PreviousSibling(descendant)) ||
         IsA<HTMLOptionElement>(descendant) ||
         IsA<HTMLDivElement>(descendant) || IsA<HTMLSpanElement>(descendant) ||
         IsA<HTMLNoScriptElement>(descendant) ||
         IsA<HTMLScriptElement>(descendant) ||
         IsA<HTMLTemplateElement>(descendant);
}

bool SelectMutationObserver::IsAllowedDescendantOfButton(
    const Node& descendant) {
  return IsA<HTMLSelectedContentElement>(descendant) ||
         CheckDescedantOfOption(descendant) ==
             ElementAccessibilityIssueReason::kValidChild;
}

ElementAccessibilityIssueReason SelectMutationObserver::CheckDescedantOfOption(
    const Node& descendant) {
  if (!IsA<HTMLDivElement>(descendant) &&
      !IsAllowedPhrasingContent(descendant) &&
      !IsAutonomousCustomElement(descendant)) {
    return ElementAccessibilityIssueReason::kNonPhrasingContentOptionChild;
  }
  // Check tabindex and contenteditable attributes of the descendant as well.
  if (!HasTabIndexAttribute(descendant) && !IsContenteditable(descendant)) {
    return ElementAccessibilityIssueReason::kValidChild;
  }
  return ElementAccessibilityIssueReason::kInteractiveContentOptionChild;
}

bool SelectMutationObserver::HasTabIndexAttribute(const Node& node) {
  if (auto* element = DynamicTo<Element>(node)) {
    return element->FastHasAttribute(html_names::kTabindexAttr);
  }
  return false;
}

bool SelectMutationObserver::IsContenteditable(const Node& node) {
  if (auto* html_element = DynamicTo<HTMLElement>(node)) {
    ContentEditableType normalized_value =
        html_element->contentEditableNormalized();
    return normalized_value == ContentEditableType::kContentEditable ||
           normalized_value == ContentEditableType::kPlaintextOnly;
  }
  return false;
}

ElementAccessibilityIssueReason
SelectMutationObserver::TraverseAncestorsAndCheckDescendant(
    const Node& descendant) {
  // As we've already checked the descendant's parent, we can directly look at
  // the grandparent.
  const Node* parent = descendant.parentNode();
  for (const Node* ancestor = parent->parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    if (IsA<HTMLOptionElement>(*ancestor) ||
        IsA<HTMLSelectedContentElement>(*ancestor)) {
      return CheckDescedantOfOption(descendant);
    }
    if (IsA<HTMLOptGroupElement>(*ancestor)) {
      if (IsAllowedDescendantOfOptgroup(descendant, *parent)) {
        return ElementAccessibilityIssueReason::kValidChild;
      }
      return ElementAccessibilityIssueReason::kDisallowedOptGroupChild;
    }
    if (IsA<HTMLSelectElement>(*ancestor) &&
        IsAllowedDescendantOfSelect(descendant, *parent)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
    if (IsA<HTMLButtonElement>(*ancestor) &&
        IsAllowedDescendantOfButton(descendant)) {
      return ElementAccessibilityIssueReason::kValidChild;
    }
  }
  return ElementAccessibilityIssueReason::kDisallowedSelectChild;
}

bool SelectMutationObserver::IsWhitespaceOrEmpty(const Node& node) {
  return node.IsTextNode() &&
         node.textContent().ContainsOnlyWhitespaceOrEmpty();
}

// Phrasing content that isn't Interactive content. <datalist>, <object>
// elements are excluded as well.
bool SelectMutationObserver::IsAllowedPhrasingContent(const Node& node) {
  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, phrasing_content_names,
                      ({
                          html_names::kATag,        html_names::kAbbrTag,
                          html_names::kAreaTag,     html_names::kAudioTag,
                          html_names::kBTag,        html_names::kBdiTag,
                          html_names::kBdoTag,      html_names::kBrTag,
                          html_names::kButtonTag,   html_names::kCanvasTag,
                          html_names::kCiteTag,     html_names::kCodeTag,
                          html_names::kDataTag,     html_names::kDatalistTag,
                          html_names::kDelTag,      html_names::kDfnTag,
                          html_names::kEmTag,       html_names::kEmbedTag,
                          html_names::kITag,        html_names::kIFrameTag,
                          html_names::kImgTag,      html_names::kInputTag,
                          html_names::kInsTag,      html_names::kKbdTag,
                          html_names::kLabelTag,    html_names::kLinkTag,
                          html_names::kMapTag,      html_names::kMarkTag,
                          mathml_names::kMathTag,   html_names::kMetaTag,
                          html_names::kMeterTag,    html_names::kNoscriptTag,
                          html_names::kObjectTag,   html_names::kOutputTag,
                          html_names::kPictureTag,  html_names::kProgressTag,
                          html_names::kQTag,        html_names::kRubyTag,
                          html_names::kSTag,        html_names::kSampTag,
                          html_names::kScriptTag,   html_names::kSelectTag,
                          html_names::kSlotTag,     html_names::kSmallTag,
                          html_names::kSpanTag,     html_names::kStrongTag,
                          html_names::kSubTag,      html_names::kSupTag,
                          svg_names::kSVGTag,       html_names::kTemplateTag,
                          html_names::kTextareaTag, html_names::kTimeTag,
                          html_names::kUTag,        html_names::kVarTag,
                          html_names::kVideoTag,    html_names::kWbrTag,
                      }));
  if (node.IsTextNode()) {
    return true;
  }
  if (IsA<HTMLDataListElement>(node) || IsA<HTMLObjectElement>(node)) {
    return false;
  }
  if (const auto* element = DynamicTo<Element>(node)) {
    if (phrasing_content_names.Contains(element->TagQName())) {
      if (auto* html_element = DynamicTo<HTMLElement>(element)) {
        return !html_element->IsInteractiveContent();
      }
      return element->IsSVGElement();
    }
  }
  return false;
}

bool SelectMutationObserver::IsAutonomousCustomElement(const Node& node) {
  if (node.IsCustomElement()) {
    if (auto* element = DynamicTo<Element>(node)) {
      if (CustomElement::IsValidName(element->localName())) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace blink
