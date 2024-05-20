// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_time_display_element.h"

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/gfx/geometry/size.h"

namespace {

// These constants are used to estimate the size of time display element
// when the time display is hidden.
constexpr int kDefaultTimeDisplayDigitWidth = 8;
constexpr int kDefaultTimeDisplayColonWidth = 3;
constexpr int kDefaultTimeDisplayHeight = 48;

}  // namespace

namespace blink {

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  // Will hide from accessibility tree, because the information is redundant
  // with the info provided on the media scrubber.
  setAttribute(html_names::kAriaHiddenAttr, AtomicString("true"));
}

void MediaControlTimeDisplayElement::SetCurrentValue(double time) {
  if (current_value_ == time) {
    return;
  }
  current_value_ = time;
  String formatted_time = FormatTime();
  setInnerText(formatted_time);
}

double MediaControlTimeDisplayElement::CurrentValue() const {
  return current_value_.value_or(0);
}

gfx::Size MediaControlTimeDisplayElement::GetSizeOrDefault() const {
  return MediaControlElementsHelper::GetSizeOrDefault(
      *this, gfx::Size(EstimateElementWidth(), kDefaultTimeDisplayHeight));
}

int MediaControlTimeDisplayElement::EstimateElementWidth() const {
  String formatted_time = MediaControlTimeDisplayElement::FormatTime();
  int colons = formatted_time.length() > 5 ? 2 : 1;
  return kDefaultTimeDisplayColonWidth * colons +
         kDefaultTimeDisplayDigitWidth * (formatted_time.length() - colons);
}

String MediaControlTimeDisplayElement::FormatTime() const {
  return MediaControlsSharedHelpers::FormatTime(CurrentValue());
}

}  // namespace blink
