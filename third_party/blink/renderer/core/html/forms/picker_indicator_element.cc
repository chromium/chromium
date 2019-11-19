/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/picker_indicator_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/layout_details_marker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

PickerIndicatorElement::PickerIndicatorElement(
    Document& document,
    PickerIndicatorOwner& picker_indicator_owner)
    : HTMLDivElement(document),
      picker_indicator_owner_(&picker_indicator_owner) {
  SetShadowPseudoId(AtomicString("-webkit-calendar-picker-indicator"));
  setAttribute(html_names::kIdAttr, shadow_element_names::PickerIndicator());
}

PickerIndicatorElement::~PickerIndicatorElement() {
  DCHECK(!chooser_);
}

LayoutObject* PickerIndicatorElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (RuntimeEnabledFeatures::FormControlsRefreshEnabled())
    return HTMLDivElement::CreateLayoutObject(style, legacy);

  return new LayoutDetailsMarker(this);
}

void PickerIndicatorElement::DefaultEventHandler(Event& event) {
  if (!GetLayoutObject())
    return;
  if (!picker_indicator_owner_ ||
      picker_indicator_owner_->IsPickerIndicatorOwnerDisabledOrReadOnly())
    return;

  if (event.type() == event_type_names::kClick) {
    OpenPopup();
    event.SetDefaultHandled();
  } else if (event.type() == event_type_names::kKeypress &&
             event.IsKeyboardEvent()) {
    int char_code = ToKeyboardEvent(event).charCode();
    if (char_code == ' ' || char_code == '\r') {
      OpenPopup();
      event.SetDefaultHandled();
    }
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool PickerIndicatorElement::WillRespondToMouseClickEvents() {
  if (GetLayoutObject() && picker_indicator_owner_ &&
      !picker_indicator_owner_->IsPickerIndicatorOwnerDisabledOrReadOnly())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

void PickerIndicatorElement::DidChooseValue(const String& value) {
  if (!picker_indicator_owner_)
    return;
  picker_indicator_owner_->PickerIndicatorChooseValue(value);
}

void PickerIndicatorElement::DidChooseValue(double value) {
  if (picker_indicator_owner_)
    picker_indicator_owner_->PickerIndicatorChooseValue(value);
}

void PickerIndicatorElement::DidEndChooser() {
  chooser_.Clear();
}

void PickerIndicatorElement::OpenPopup() {
  if (chooser_)
    return;
  if (!GetDocument().GetPage())
    return;
  if (!picker_indicator_owner_)
    return;
  DateTimeChooserParameters parameters;
  if (!picker_indicator_owner_->SetupDateTimeChooserParameters(parameters))
    return;
  chooser_ = GetDocument().GetPage()->GetChromeClient().OpenDateTimeChooser(
      GetDocument().GetFrame(), this, parameters);
}

Element& PickerIndicatorElement::OwnerElement() const {
  DCHECK(picker_indicator_owner_);
  return picker_indicator_owner_->PickerOwnerElement();
}

void PickerIndicatorElement::ClosePopup() {
  if (!chooser_)
    return;
  chooser_->EndChooser();
}

void PickerIndicatorElement::DetachLayoutTree(bool performing_reattach) {
  ClosePopup();
  HTMLDivElement::DetachLayoutTree(performing_reattach);
}

AXObject* PickerIndicatorElement::PopupRootAXObject() const {
  return chooser_ ? chooser_->RootAXObject() : nullptr;
}

bool PickerIndicatorElement::IsPickerIndicatorElement() const {
  return true;
}

Node::InsertionNotificationRequest PickerIndicatorElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLDivElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void PickerIndicatorElement::DidNotifySubtreeInsertionsToDocument() {
  if (!GetDocument().ExistingAXObjectCache())
    return;
  // Don't make this focusable if we are in web tests in order to avoid
  // breaking existing tests.
  // FIXME: We should have a way to disable accessibility in web tests.
  if (WebTestSupport::IsRunningWebTest())
    return;
  setAttribute(html_names::kTabindexAttr, "0");
  setAttribute(html_names::kAriaHaspopupAttr, "menu");
  setAttribute(html_names::kRoleAttr, "button");
  setAttribute(
      html_names::kAriaLabelAttr,
      AtomicString(GetLocale().QueryString(IDS_AX_CALENDAR_SHOW_DATE_PICKER)));
}

void PickerIndicatorElement::Trace(Visitor* visitor) {
  visitor->Trace(picker_indicator_owner_);
  visitor->Trace(chooser_);
  HTMLDivElement::Trace(visitor);
  DateTimeChooserClient::Trace(visitor);
}

}  // namespace blink
