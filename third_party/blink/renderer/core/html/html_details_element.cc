/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/html/html_details_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_summary_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

HTMLDetailsElement::HTMLDetailsElement(Document& document)
    : HTMLElement(html_names::kDetailsTag, document) {
  UseCounter::Count(document, WebFeature::kDetailsElement);
  EnsureUserAgentShadowRoot(SlotAssignmentMode::kManual);
}

HTMLDetailsElement::~HTMLDetailsElement() = default;

void HTMLDetailsElement::DispatchPendingEvent(
    const AttributeModificationReason reason) {
  Event* toggle_event = nullptr;
  CHECK(pending_toggle_event_);
  toggle_event = pending_toggle_event_.Get();
  pending_toggle_event_ = nullptr;

  if (reason == AttributeModificationReason::kByParser)
    GetDocument().SetToggleDuringParsing(true);
  DispatchEvent(*toggle_event);
  if (reason == AttributeModificationReason::kByParser)
    GetDocument().SetToggleDuringParsing(false);
}

LayoutObject* HTMLDetailsElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (RuntimeEnabledFeatures::DetailsStylingEnabled()) {
    return HTMLElement::CreateLayoutObject(style);
  }

  return LayoutObject::CreateBlockFlowOrListItem(this, style);
}

// Creates shadow DOM:
// #shadowroot
//   <SLOT id="details-summary">
//     <SUMMARY>Details</SUMMARY>
//   </SLOT>
//   <SLOT id="details-content"
//         style="content-visibility: hidden; display:block;"></SLOT>
//   <STYLE>...
void HTMLDetailsElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  auto* default_summary =
      MakeGarbageCollected<HTMLSummaryElement>(GetDocument());
  default_summary->AppendChild(
      Text::Create(GetDocument(),
                   GetLocale().QueryString(IDS_DETAILS_WITHOUT_SUMMARY_LABEL)));

  summary_slot_ = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
  summary_slot_->SetIdAttribute(shadow_element_names::kIdDetailsSummary);
  summary_slot_->AppendChild(default_summary);
  root.AppendChild(summary_slot_);

  content_slot_ = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
  content_slot_->SetIdAttribute(shadow_element_names::kIdDetailsContent);
  if (RuntimeEnabledFeatures::DetailsStylingEnabled()) {
    content_slot_->SetShadowPseudoId(shadow_element_names::kIdDetailsContent);
  }
  content_slot_->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                        CSSValueID::kHidden);
  content_slot_->EnsureDisplayLockContext().SetIsDetailsSlotElement(true);
  content_slot_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                        CSSValueID::kBlock);
  root.AppendChild(content_slot_);

  auto* default_summary_style =
      MakeGarbageCollected<HTMLStyleElement>(GetDocument());
  // This style is required only if this <details> shows the UA-provided
  // <summary>, not a light child <summary>.
  default_summary_style->setTextContent(R"CSS(
:host summary {
  display: list-item;
  counter-increment: list-item 0;
  list-style: disclosure-closed inside;
}
:host([open]) summary {
  list-style-type: disclosure-open;
}
)CSS");
  root.AppendChild(default_summary_style);
}

Element* HTMLDetailsElement::FindMainSummary() const {
  if (HTMLSummaryElement* summary =
          Traversal<HTMLSummaryElement>::FirstChild(*this))
    return summary;
  HTMLSlotElement& slot =
      To<HTMLSlotElement>(*UserAgentShadowRoot()->firstChild());
  CHECK(IsA<HTMLSummaryElement>(*slot.firstChild()));
  return To<Element>(slot.firstChild());
}

void HTMLDetailsElement::ManuallyAssignSlots() {
  HeapVector<Member<Node>> summary_nodes;
  HeapVector<Member<Node>> content_nodes;
  for (Node& child : NodeTraversal::ChildrenOf(*this)) {
    if (!child.IsSlotable()) {
      CHECK(!IsA<HTMLSummaryElement>(child));
      continue;
    }
    bool is_first_summary =
        summary_nodes.empty() && IsA<HTMLSummaryElement>(child);
    if (is_first_summary) {
      summary_nodes.push_back(child);
    } else {
      content_nodes.push_back(child);
    }
  }
  summary_slot_->Assign(summary_nodes);
  content_slot_->Assign(content_nodes);
}

void HTMLDetailsElement::Trace(Visitor* visitor) const {
  visitor->Trace(pending_toggle_event_);
  visitor->Trace(summary_slot_);
  visitor->Trace(content_slot_);
  HTMLElement::Trace(visitor);
}

void HTMLDetailsElement::ParseAttribute(
    const AttributeModificationParams& params) {
  CHECK(params.reason != AttributeModificationReason::kByParser ||
        !parentNode())
      << "This code depends on the parser setting attributes before inserting "
         "into the document; if that were not the case we would need to "
         "handle setting of either open or name by the parser, and "
         "potentially change the open attribute during that handling.";

  if (params.name == html_names::kOpenAttr) {
    bool old_value = is_open_;
    is_open_ = !params.new_value.IsNull();
    if (is_open_ == old_value)
      return;

    // Dispatch toggle event asynchronously.
    String old_state = is_open_ ? "closed" : "open";
    String new_state = is_open_ ? "open" : "closed";
    if (pending_toggle_event_) {
      old_state = pending_toggle_event_->oldState();
    }
    pending_toggle_event_ =
        ToggleEvent::Create(event_type_names::kToggle, Event::Cancelable::kNo,
                            old_state, new_state);
    pending_event_task_ = PostCancellableTask(
        *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
        WTF::BindOnce(&HTMLDetailsElement::DispatchPendingEvent,
                      WrapPersistent(this), params.reason));

    Element* content =
        EnsureUserAgentShadowRoot(SlotAssignmentMode::kManual)
            .getElementById(shadow_element_names::kIdDetailsContent);
    DCHECK(content);

    if (is_open_) {
      content->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
      if (!RuntimeEnabledFeatures::DetailsStylingEnabled()) {
        content->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
      }

      // https://html.spec.whatwg.org/multipage/interactive-elements.html#ensure-details-exclusivity-by-closing-other-elements-if-needed
      //
      // The name attribute links multiple details elements into an
      // exclusive accordion.  So if this one has a name, close the
      // other ones with the same name.
      CHECK_NE(params.reason,
               AttributeModificationReason::kBySynchronizationOfLazyAttribute);
      // TODO(https://crbug.com/1444057): Should this be in
      // AttributeChanged instead?
      if (!GetName().empty() &&
          params.reason == AttributeModificationReason::kDirectly) {
        // Don't fire mutation events for any changes to the open attribute
        // that this causes.
        MutationEventSuppressionScope scope(GetDocument());

        HeapVector<Member<HTMLDetailsElement>> details_with_name(
            OtherElementsInNameGroup());
        for (HTMLDetailsElement* other_details : details_with_name) {
          CHECK_NE(other_details, this);
          UseCounter::Count(
              GetDocument(),
              WebFeature::kHTMLDetailsElementNameAttributeClosesOther);
          other_details->setAttribute(html_names::kOpenAttr, g_null_atom);
        }
      }
    } else {
      if (!RuntimeEnabledFeatures::DetailsStylingEnabled()) {
        content->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                        CSSValueID::kBlock);
      }
      content->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                      CSSValueID::kHidden);
      content->EnsureDisplayLockContext().SetIsDetailsSlotElement(true);
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLDetailsElement::AttributeChanged(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kNameAttr) {
    if (!params.new_value.empty()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLDetailsElementNameAttribute);
    }
    MaybeCloseForExclusivity();
  }

  HTMLElement::AttributeChanged(params);
}

Node::InsertionNotificationRequest HTMLDetailsElement::InsertedInto(
    ContainerNode& insertion_point) {
  Node::InsertionNotificationRequest result =
      HTMLElement::InsertedInto(insertion_point);

  MaybeCloseForExclusivity();

  return result;
}

// https://html.spec.whatwg.org/multipage/C#ensure-details-exclusivity-by-closing-the-given-element-if-needed
void HTMLDetailsElement::MaybeCloseForExclusivity() {
  if (GetName().empty() || !is_open_) {
    return;
  }

  // Don't fire mutation events for any changes to the open attribute
  // that this causes.
  MutationEventSuppressionScope scope(GetDocument());

  HeapVector<Member<HTMLDetailsElement>> details_with_name(
      OtherElementsInNameGroup());
  for (HTMLDetailsElement* other_details : details_with_name) {
    CHECK_NE(other_details, this);
    if (other_details->is_open_) {
      // close this details element
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLDetailsElementNameAttributeClosesSelf);
      ToggleOpen();
      break;
    }
  }
}

void HTMLDetailsElement::ToggleOpen() {
  setAttribute(html_names::kOpenAttr, is_open_ ? g_null_atom : g_empty_atom);
}

HeapVector<Member<HTMLDetailsElement>>
HTMLDetailsElement::OtherElementsInNameGroup() {
  HeapVector<Member<HTMLDetailsElement>> result;
  const AtomicString& name = GetName();
  if (name.empty()) {
    return result;
  }
  HTMLDetailsElement* details = Traversal<HTMLDetailsElement>::Next(TreeRoot());
  while (details) {
    if (details != this && details->GetName() == name) {
      result.push_back(details);
    }
    details = Traversal<HTMLDetailsElement>::Next(*details);
  }
  return result;
}

bool HTMLDetailsElement::IsInteractiveContent() const {
  return true;
}

// static
bool HTMLDetailsElement::ExpandDetailsAncestors(const Node& node) {
  CHECK(&node);
  // Since setting the open attribute fires mutation events which could mess
  // with the FlatTreeTraversal iterator, we should first iterate details
  // elements to open and then open them all.
  HeapVector<Member<HTMLDetailsElement>> details_to_open;

  for (Node& parent : FlatTreeTraversal::AncestorsOf(node)) {
    if (HTMLDetailsElement* details = DynamicTo<HTMLDetailsElement>(parent)) {
      // If the active match is inside the <summary> of a <details>, then we
      // shouldn't expand the <details> because the active match is already
      // visible.
      bool inside_summary = false;
      Element& summary = *details->FindMainSummary();
      for (Node& ancestor : FlatTreeTraversal::AncestorsOf(node)) {
        if (&ancestor == &summary) {
          inside_summary = true;
          break;
        }
      }

      if (!inside_summary &&
          !details->FastHasAttribute(html_names::kOpenAttr)) {
        details_to_open.push_back(details);
      }
    }
  }

  for (HTMLDetailsElement* details : details_to_open) {
    details->setAttribute(html_names::kOpenAttr, g_empty_atom);
  }

  return details_to_open.size();
}

bool HTMLDetailsElement::IsValidCommand(HTMLElement& invoker,
                                        CommandEventType command) {
  bool parent_is_valid = HTMLElement::IsValidCommand(invoker, command);
  if (!RuntimeEnabledFeatures::HTMLInvokeActionsV2Enabled()) {
    return parent_is_valid;
  }
  return parent_is_valid || command == CommandEventType::kToggle ||
         command == CommandEventType::kOpen ||
         command == CommandEventType::kClose;
}

bool HTMLDetailsElement::HandleCommandInternal(HTMLElement& invoker,
                                               CommandEventType command) {
  CHECK(IsValidCommand(invoker, command));

  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }

  if (command == CommandEventType::kToggle) {
    ToggleOpen();
    return true;
  } else if (command == CommandEventType::kClose) {
    if (is_open_) {
      setAttribute(html_names::kOpenAttr, g_null_atom);
    }
    return true;
  } else if (command == CommandEventType::kOpen) {
    if (!is_open_) {
      setAttribute(html_names::kOpenAttr, g_empty_atom);
    }
    return true;
  }

  return false;
}

}  // namespace blink
