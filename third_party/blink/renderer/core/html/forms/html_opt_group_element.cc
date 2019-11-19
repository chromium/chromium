/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

HTMLOptGroupElement::HTMLOptGroupElement(Document& document)
    : HTMLElement(html_names::kOptgroupTag, document) {
  EnsureUserAgentShadowRoot();
}

// An explicit empty destructor should be in html_opt_group_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_opt_group_element.h, when including html_opt_group_element.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptGroupElement::~HTMLOptGroupElement() = default;

// static
bool HTMLOptGroupElement::CanAssignToOptGroupSlot(const Node& node) {
  return node.HasTagName(html_names::kOptionTag) ||
         node.HasTagName(html_names::kHrTag);
}

bool HTMLOptGroupElement::IsDisabledFormControl() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

void HTMLOptGroupElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);

  if (params.name == html_names::kDisabledAttr) {
    PseudoStateChanged(CSSSelector::kPseudoDisabled);
    PseudoStateChanged(CSSSelector::kPseudoEnabled);
  } else if (params.name == html_names::kLabelAttr) {
    UpdateGroupLabel();
  }
}

bool HTMLOptGroupElement::SupportsFocus() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList())
    return false;
  return HTMLElement::SupportsFocus();
}

bool HTMLOptGroupElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

Node::InsertionNotificationRequest HTMLOptGroupElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (&insertion_point == select)
      select->OptGroupInsertedOrRemoved(*this);
  }
  return kInsertionDone;
}

void HTMLOptGroupElement::RemovedFrom(ContainerNode& insertion_point) {
  if (auto* select = DynamicTo<HTMLSelectElement>(insertion_point)) {
    if (!parentNode())
      select->OptGroupInsertedOrRemoved(*this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

String HTMLOptGroupElement::GroupLabelText() const {
  String item_text = FastGetAttribute(html_names::kLabelAttr);

  // In WinIE, leading and trailing whitespace is ignored in options and
  // optgroups. We match this behavior.
  item_text = item_text.StripWhiteSpace();
  // We want to collapse our whitespace too.  This will match other browsers.
  item_text = item_text.SimplifyWhiteSpace();

  return item_text;
}

HTMLSelectElement* HTMLOptGroupElement::OwnerSelectElement() const {
  // TODO(tkent): We should return only the parent <select>.
  return Traversal<HTMLSelectElement>::FirstAncestor(*this);
}

String HTMLOptGroupElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

void HTMLOptGroupElement::AccessKeyAction(bool) {
  HTMLSelectElement* select = OwnerSelectElement();
  // send to the parent to bring focus to the list box
  if (select && !select->IsFocused())
    select->AccessKeyAction(false);
}

void HTMLOptGroupElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DEFINE_STATIC_LOCAL(AtomicString, label_padding, ("0 2px 1px 2px"));
  DEFINE_STATIC_LOCAL(AtomicString, label_min_height, ("1.2em"));
  auto* label = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  label->setAttribute(html_names::kRoleAttr, AtomicString("group"));
  label->setAttribute(html_names::kAriaLabelAttr, AtomicString());
  label->SetInlineStyleProperty(CSSPropertyID::kPadding, label_padding);
  label->SetInlineStyleProperty(CSSPropertyID::kMinHeight, label_min_height);
  label->SetIdAttribute(shadow_element_names::OptGroupLabel());
  root.AppendChild(label);

  root.AppendChild(
      HTMLSlotElement::CreateUserAgentCustomAssignSlot(GetDocument()));
}

void HTMLOptGroupElement::UpdateGroupLabel() {
  const String& label_text = GroupLabelText();
  HTMLDivElement& label = OptGroupLabelElement();
  label.setTextContent(label_text);
  label.setAttribute(html_names::kAriaLabelAttr, AtomicString(label_text));
}

HTMLDivElement& HTMLOptGroupElement::OptGroupLabelElement() const {
  auto* element = UserAgentShadowRoot()->getElementById(
      shadow_element_names::OptGroupLabel());
  CHECK(!element || IsA<HTMLDivElement>(element));
  return *To<HTMLDivElement>(element);
}

}  // namespace blink
