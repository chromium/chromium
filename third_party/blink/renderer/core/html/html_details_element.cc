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
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_summary_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

HTMLDetailsElement::HTMLDetailsElement(Document& document)
    : HTMLElement(html_names::kDetailsTag, document) {
  UseCounter::Count(document, WebFeature::kDetailsElement);
  EnsureUserAgentShadowRoot().SetSlotAssignmentMode(
      SlotAssignmentMode::kManual);
}

HTMLDetailsElement::~HTMLDetailsElement() = default;

void HTMLDetailsElement::DispatchPendingEvent(
    const AttributeModificationReason reason) {
  if (reason == AttributeModificationReason::kByParser)
    GetDocument().SetToggleDuringParsing(true);
  DispatchEvent(*Event::Create(event_type_names::kToggle));
  if (reason == AttributeModificationReason::kByParser)
    GetDocument().SetToggleDuringParsing(false);
}

LayoutObject* HTMLDetailsElement::CreateLayoutObject(
    const ComputedStyle& style) {
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
  content_slot_->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                        CSSValueID::kHidden);
  content_slot_->EnsureDisplayLockContext().SetIsDetailsSlotElement(true);
  content_slot_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                        CSSValueID::kBlock);
  root.AppendChild(content_slot_);

  auto* default_summary_style = MakeGarbageCollected<HTMLStyleElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
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

  auto* element = UserAgentShadowRoot()->firstChild();
  CHECK(!element || IsA<HTMLSlotElement>(element));
  HTMLSlotElement* slot = To<HTMLSlotElement>(element);
  DCHECK(slot->firstChild());
  CHECK(IsA<HTMLSummaryElement>(*slot->firstChild()));
  return To<Element>(slot->firstChild());
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
  visitor->Trace(summary_slot_);
  visitor->Trace(content_slot_);
  HTMLElement::Trace(visitor);
}

void HTMLDetailsElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kOpenAttr) {
    bool old_value = is_open_;
    is_open_ = !params.new_value.IsNull();
    if (is_open_ == old_value)
      return;

    // Dispatch toggle event asynchronously.
    pending_event_ = PostCancellableTask(
        *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
        WTF::BindOnce(&HTMLDetailsElement::DispatchPendingEvent,
                      WrapPersistent(this), params.reason));

    Element* content = EnsureUserAgentShadowRoot().getElementById(
        shadow_element_names::kIdDetailsContent);
    DCHECK(content);

    if (is_open_) {
      content->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
      content->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);

      // The name attribute links multiple details elements into an
      // exclusive accordion.  So if this one has a name, close the
      // other ones with the same name.
      CHECK_NE(params.reason,
               AttributeModificationReason::kBySynchronizationOfLazyAttribute);
      if (RuntimeEnabledFeatures::AccordionPatternEnabled() &&
          !GetName().empty() &&
          params.reason == AttributeModificationReason::kDirectly) {
        // It's important that we have a copy of the set of details
        // elements, because the setAttribute call can trigger mutation
        // events that change the set.
        HeapVector<Member<HTMLDetailsElement>> details_with_name(
            OtherElementsInNameGroup());
        for (HTMLDetailsElement* other_details : details_with_name) {
          CHECK_NE(other_details, this);
          other_details->setAttribute(html_names::kOpenAttr, g_null_atom);
        }
      }
    } else {
      content->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                      CSSValueID::kBlock);
      content->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                      CSSValueID::kHidden);
      content->EnsureDisplayLockContext().SetIsDetailsSlotElement(true);
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLDetailsElement::ToggleOpen() {
  setAttribute(html_names::kOpenAttr, is_open_ ? g_null_atom : g_empty_atom);
}

HeapVector<Member<HTMLDetailsElement>>
HTMLDetailsElement::OtherElementsInNameGroup() {
  CHECK(RuntimeEnabledFeatures::AccordionPatternEnabled());
  HeapVector<Member<HTMLDetailsElement>> result;
  const AtomicString& name = GetName();
  if (name.empty() || !IsInTreeScope()) {
    return result;
  }
  HTMLDetailsElement* details =
      Traversal<HTMLDetailsElement>::Next(GetTreeScope().RootNode());
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

}  // namespace blink
