// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_input_element.h"

#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

// The default size of an overflow button in pixels.
constexpr int kDefaultButtonSize = 48;

const char kOverflowContainerWithSubtitleCSSClass[] = "with-subtitle";
const char kOverflowSubtitleCSSClass[] = "subtitle";

}  // namespace

namespace blink {

// static
bool MediaControlInputElement::ShouldRecordDisplayStates(
    const HTMLMediaElement& media_element) {
  // Only record when the metadat are available so that the display state of the
  // buttons are fairly stable. For example, before metadata are available, the
  // size of the element might differ, it's unknown if the file has an audio
  // track, etc.
  if (media_element.getReadyState() >= HTMLMediaElement::kHaveMetadata)
    return true;

  // When metadata are not available, only record the display state if the
  // element will require a user gesture in order to load.
  if (media_element.EffectivePreloadType() ==
      WebMediaPlayer::Preload::kPreloadNone) {
    return true;
  }

  return false;
}

HTMLElement* MediaControlInputElement::CreateOverflowElement(
    MediaControlInputElement* button) {
  if (!button)
    return nullptr;

  // We don't want the button visible within the overflow menu.
  button->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);

  overflow_menu_text_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  overflow_menu_text_->setInnerText(button->GetOverflowMenuString(),
                                    ASSERT_NO_EXCEPTION);

  overflow_label_element_ =
      MakeGarbageCollected<HTMLLabelElement>(GetDocument());
  overflow_label_element_->SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list-item"));
  overflow_label_element_->setAttribute(html_names::kRoleAttr, "menuitem");
  // Appending a button to a label element ensures that clicks on the label
  // are passed down to the button, performing the action we'd expect.
  overflow_label_element_->ParserAppendChild(button);

  // Allows to focus the list entry instead of the button.
  overflow_label_element_->setTabIndex(0);
  button->setTabIndex(-1);

  overflow_menu_container_ =
      MakeGarbageCollected<HTMLDivElement>(GetDocument());
  overflow_menu_container_->ParserAppendChild(overflow_menu_text_);
  overflow_menu_container_->setAttribute(html_names::kAriaHiddenAttr, "true");
  aria_label_ = button->FastGetAttribute(html_names::kAriaLabelAttr) + " " +
                button->GetOverflowMenuString();
  UpdateOverflowSubtitleElement(button->GetOverflowMenuSubtitleString());
  overflow_label_element_->ParserAppendChild(overflow_menu_container_);

  // Initialize the internal states of the main element and the overflow one.
  button->is_overflow_element_ = true;
  overflow_element_ = button;

  // Keeping the element hidden by default. This is setting the style in
  // addition of calling ShouldShowButtonInOverflowMenu() to guarantee that the
  // internal state matches the CSS state.
  overflow_label_element_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                  CSSValueID::kNone);
  SetOverflowElementIsWanted(false);

  return overflow_label_element_;
}

void MediaControlInputElement::UpdateOverflowSubtitleElement(String text) {
  DCHECK(overflow_menu_container_);

  if (!text) {
    // If setting the text to null, we want to remove the element.
    RemoveOverflowSubtitleElement();
    UpdateOverflowLabelAriaLabel("");
    return;
  }

  if (overflow_menu_subtitle_) {
    // If element exists, just update the text.
    overflow_menu_subtitle_->setInnerText(text, ASSERT_NO_EXCEPTION);
  } else {
    // Otherwise, create a new element.
    overflow_menu_subtitle_ =
        MakeGarbageCollected<HTMLSpanElement>(GetDocument());
    overflow_menu_subtitle_->setInnerText(text, ASSERT_NO_EXCEPTION);
    overflow_menu_subtitle_->setAttribute("class", kOverflowSubtitleCSSClass);

    overflow_menu_container_->ParserAppendChild(overflow_menu_subtitle_);
    overflow_menu_container_->setAttribute(
        "class", kOverflowContainerWithSubtitleCSSClass);
  }
  UpdateOverflowLabelAriaLabel(text);
}

void MediaControlInputElement::RemoveOverflowSubtitleElement() {
  if (!overflow_menu_subtitle_)
    return;

  overflow_menu_container_->RemoveChild(overflow_menu_subtitle_);
  overflow_menu_container_->removeAttribute("class");
  overflow_menu_subtitle_ = nullptr;
}

bool MediaControlInputElement::OverflowElementIsWanted() {
  return overflow_element_ && overflow_element_->IsWanted();
}

void MediaControlInputElement::SetOverflowElementIsWanted(bool wanted) {
  if (!overflow_element_)
    return;
  overflow_element_->SetIsWanted(wanted);
}

void MediaControlInputElement::UpdateOverflowLabelAriaLabel(String subtitle) {
  String full_aria_label = aria_label_ + " " + subtitle;
  overflow_label_element_->setAttribute(html_names::kAriaLabelAttr,
                                        WTF::AtomicString(full_aria_label));
}

void MediaControlInputElement::MaybeRecordDisplayed() {
  // Display is defined as wanted and fitting. Overflow elements will only be
  // displayed if their inline counterpart isn't displayed.
  if (!IsWanted() || !DoesFit()) {
    if (IsWanted() && overflow_element_)
      overflow_element_->MaybeRecordDisplayed();
    return;
  }

  // Keep this check after the block above because `display_recorded_` might be
  // true for the inline element but not for the overflow one.
  if (display_recorded_)
    return;

  RecordCTREvent(CTREvent::kDisplayed);
  display_recorded_ = true;
}

void MediaControlInputElement::UpdateOverflowString() {
  if (!overflow_menu_text_)
    return;

  DCHECK(overflow_element_);
  overflow_menu_text_->setInnerText(GetOverflowMenuString(),
                                    ASSERT_NO_EXCEPTION);

  UpdateOverflowSubtitleElement(GetOverflowMenuSubtitleString());
}

MediaControlInputElement::MediaControlInputElement(
    MediaControlsImpl& media_controls)
    : HTMLInputElement(media_controls.GetDocument(), CreateElementFlags()),
      MediaControlElementBase(media_controls, this) {}

int MediaControlInputElement::GetOverflowStringId() const {
  NOTREACHED();
  return IDS_AX_AM_PM_FIELD_TEXT;
}

void MediaControlInputElement::UpdateShownState() {
  if (is_overflow_element_) {
    Element* parent = parentElement();
    DCHECK(parent);
    DCHECK(IsA<HTMLLabelElement>(parent));

    if (IsWanted() && DoesFit()) {
      parent->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    } else {
      parent->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                     CSSValueID::kNone);
    }

    // Don't update the shown state of the element if we want to hide
    // icons on the overflow menu.
    if (!RuntimeEnabledFeatures::OverflowIconsForMediaControlsEnabled())
      return;
  }

  MediaControlElementBase::UpdateShownState();
}

void MediaControlInputElement::DefaultEventHandler(Event& event) {
  if (!IsDisabled() && (event.type() == event_type_names::kClick ||
                        event.type() == event_type_names::kGesturetap)) {
    MaybeRecordInteracted();
  }

  // Unhover the element if the hover is triggered by a tap on
  // a touch screen device to avoid showing hover circle indefinitely.
  if (event.IsGestureEvent() && IsHovered())
    SetHovered(false);

  HTMLInputElement::DefaultEventHandler(event);
}

void MediaControlInputElement::MaybeRecordInteracted() {
  if (interaction_recorded_)
    return;

  if (!display_recorded_) {
    // The only valid reason to not have the display recorded at this point is
    // if it wasn't allowed. Regardless, the display will now be recorded.
    DCHECK(!ShouldRecordDisplayStates(MediaElement()));
    RecordCTREvent(CTREvent::kDisplayed);
  }

  RecordCTREvent(CTREvent::kInteracted);
  interaction_recorded_ = true;
}

bool MediaControlInputElement::IsOverflowElement() const {
  return is_overflow_element_;
}

bool MediaControlInputElement::IsMouseFocusable() const {
  return false;
}

bool MediaControlInputElement::IsMediaControlElement() const {
  return true;
}

String MediaControlInputElement::GetOverflowMenuString() const {
  return MediaElement().GetLocale().QueryString(GetOverflowStringId());
}

String MediaControlInputElement::GetOverflowMenuSubtitleString() const {
  return String();
}

void MediaControlInputElement::RecordCTREvent(CTREvent event) {
  String histogram_name =
      StringView("Media.Controls.CTR.") + GetNameForHistograms();
  EnumerationHistogram ctr_histogram(histogram_name.Ascii().c_str(),
                                     static_cast<int>(CTREvent::kCount));
  ctr_histogram.Count(static_cast<int>(event));
}

void MediaControlInputElement::SetClass(const AtomicString& class_name,
                                        bool should_have_class) {
  if (should_have_class)
    classList().Add(class_name);
  else
    classList().Remove(class_name);
}

void MediaControlInputElement::UpdateDisplayType() {
  if (overflow_element_)
    overflow_element_->UpdateDisplayType();
}

WebSize MediaControlInputElement::GetSizeOrDefault() const {
  if (IsControlPanelButton()) {
    return MediaControlElementsHelper::GetSizeOrDefault(
        *this, WebSize(kDefaultButtonSize, kDefaultButtonSize));
  }
  return MediaControlElementsHelper::GetSizeOrDefault(*this, WebSize(0, 0));
}

bool MediaControlInputElement::IsDisabled() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

void MediaControlInputElement::Trace(blink::Visitor* visitor) {
  HTMLInputElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
  visitor->Trace(overflow_element_);
  visitor->Trace(overflow_menu_container_);
  visitor->Trace(overflow_menu_text_);
  visitor->Trace(overflow_menu_subtitle_);
  visitor->Trace(overflow_label_element_);
}

}  // namespace blink
