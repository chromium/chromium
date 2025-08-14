/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/html_summary_element.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// SummaryDescendantsObserver observes mutations to the descendants of a
// <summary> in order to give warnings when interactive content (which is
// problematic for accessibility, although still not forbidden (see
// https://github.com/whatwg/html/issues/2272) is used inside of <summary>.
//
// This is similar to SelectDescendantsObserver which fills a similar purpose
// for <select>.  They could in theory share a small amount of common code,
// but such a refactoring would probably harm code readability too much.
class SummaryDescendantsObserver : public MutationObserver::Delegate {
 public:
  explicit SummaryDescendantsObserver(HTMLSummaryElement& summary)
      : summary_(summary), observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setChildList(true);
    init->setSubtree(true);
    init->setAttributes(true);
    observer_->observe(summary_, init, ASSERT_NO_EXCEPTION);
    // Traverse descendants that have been added to the summary so far.
    TraverseNodeDescendants(summary_);
  }

  ExecutionContext* GetExecutionContext() const override {
    return summary_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    for (const auto& record : records) {
      if (record->type() == "childList") {
        CheckAddedNodes(record);
      } else if ((record->type() == "attributes") &&
                 (record->attributeName() == html_names::kTabindexAttr ||
                  record->attributeName() ==
                      html_names::kContenteditableAttr)) {
        AddDescendantDisallowedErrorToNode(*To<Element>(record->target()));
      }
    }
  }

  void Disconnect() { observer_->disconnect(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(summary_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  void CheckAddedNodes(MutationRecord* record) {
    DCHECK(record);
    auto* added_nodes = record->addedNodes();
    for (unsigned i = 0; i < added_nodes->length(); ++i) {
      auto* descendant = added_nodes->item(i);
      DCHECK(descendant);
      Element* element = DynamicTo<Element>(descendant);
      if (!element) {
        continue;
      }
      AddDescendantDisallowedErrorToNode(*element);
      // Check the added node's descendants, if any.
      TraverseNodeDescendants(element);
    }
  }

  void TraverseNodeDescendants(const Element* element) {
    for (Element* descendant = ElementTraversal::FirstWithin(*element);
         descendant;
         descendant = ElementTraversal::Next(*descendant, element)) {
      AddDescendantDisallowedErrorToNode(*descendant);
    }
  }

  void AddDescendantDisallowedErrorToNode(Element& element) {
    if (IsInteractiveElement(element)) {
      Document& document = summary_->GetDocument();
      bool has_disallowed_attributes =
          HasTabIndexAttribute(element) || IsContenteditable(element);
      AuditsIssue::ReportElementAccessibilityIssue(
          &document, element.GetDomNodeId(),
          ElementAccessibilityIssueReason::kInteractiveContentSummaryDescendant,
          has_disallowed_attributes);
      UseCounter::Count(summary_->GetDocument(),
                        WebFeature::kInteractiveContentSummaryDescendant);
    }
  }

  bool IsInteractiveElement(const Element& element) {
    if (HasTabIndexAttribute(element)) {
      return true;
    }
    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      return IsContenteditable(element) || html_element->IsInteractiveContent();
    }
    return false;
  }

  bool HasTabIndexAttribute(const Element& element) {
    return element.FastHasAttribute(html_names::kTabindexAttr);
  }

  bool IsContenteditable(const Element& element) {
    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      ContentEditableType normalized_value =
          html_element->contentEditableNormalized();
      return normalized_value == ContentEditableType::kContentEditable ||
             normalized_value == ContentEditableType::kPlaintextOnly;
    }
    return false;
  }

  Member<HTMLSummaryElement> summary_;
  Member<MutationObserver> observer_;
};

HTMLSummaryElement::HTMLSummaryElement(Document& document)
    : HTMLElement(html_names::kSummaryTag, document) {
}

void HTMLSummaryElement::Trace(Visitor* visitor) const {
  visitor->Trace(descendants_observer_);
  HTMLElement::Trace(visitor);
}

HTMLDetailsElement* HTMLSummaryElement::DetailsElement() const {
  if (auto* details = DynamicTo<HTMLDetailsElement>(parentNode()))
    return details;
  if (auto* details = DynamicTo<HTMLDetailsElement>(OwnerShadowHost()))
    return details;
  return nullptr;
}

bool HTMLSummaryElement::IsMainSummary() const {
  if (HTMLDetailsElement* details = DetailsElement())
    return &details->MainSummary() == this;

  return false;
}

FocusableState HTMLSummaryElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsMainSummary()) {
    return FocusableState::kFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

int HTMLSummaryElement::DefaultTabIndex() const {
  return IsMainSummary() ? 0 : -1;
}

void HTMLSummaryElement::DefaultEventHandler(Event& event) {
  if (IsMainSummary()) {
    if (event.type() == event_type_names::kDOMActivate &&
        !IsClickableControl(event.RawTarget()->ToNode())) {
      if (HTMLDetailsElement* details = DetailsElement())
        details->ToggleOpen();
      event.SetDefaultHandled();
      return;
    }

    if (HandleKeyboardActivation(event)) {
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLSummaryElement::HasActivationBehavior() const {
  return true;
}

bool HTMLSummaryElement::WillRespondToMouseClickEvents() {
  return IsMainSummary() || HTMLElement::WillRespondToMouseClickEvents();
}

Node::InsertionNotificationRequest HTMLSummaryElement::InsertedInto(
    ContainerNode& insertion_point) {
  Node::InsertionNotificationRequest result =
      HTMLElement::InsertedInto(insertion_point);

  if (isConnected()) {
    CHECK(!descendants_observer_);
    descendants_observer_ =
        MakeGarbageCollected<SummaryDescendantsObserver>(*this);
  }

  return result;
}

void HTMLSummaryElement::RemovedFrom(ContainerNode& insertion_point) {
  if (insertion_point.isConnected()) {
    CHECK(descendants_observer_);
    descendants_observer_->Disconnect();
    descendants_observer_ = nullptr;
  }
  HTMLElement::RemovedFrom(insertion_point);
}

}  // namespace blink
