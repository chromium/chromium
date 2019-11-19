// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_time_display_element.h"

#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

// These constants are used to estimate the size of time display element
// when the time display is hidden.
constexpr int kDefaultTimeDisplayDigitWidth = 8;
constexpr int kDefaultTimeDisplayColonWidth = 3;
constexpr int kDefaultTimeDisplayHeight = 48;

}  // namespace

namespace blink {

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(
    MediaControlsImpl& media_controls,
    int localized_resource_id)
    : MediaControlDivElement(media_controls),
      localized_resource_id_(localized_resource_id) {
  SetAriaLabel();
}

void MediaControlTimeDisplayElement::SetCurrentValue(double time) {
  current_value_ = time;
  SetAriaLabel();
  setInnerText(FormatTime(), ASSERT_NO_EXCEPTION);
}

double MediaControlTimeDisplayElement::CurrentValue() const {
  return current_value_;
}

WebSize MediaControlTimeDisplayElement::GetSizeOrDefault() const {
  return MediaControlElementsHelper::GetSizeOrDefault(
      *this, WebSize(EstimateElementWidth(), kDefaultTimeDisplayHeight));
}

int MediaControlTimeDisplayElement::EstimateElementWidth() const {
  String formatted_time = MediaControlTimeDisplayElement::FormatTime();
  int colons = formatted_time.length() > 5 ? 2 : 1;
  return kDefaultTimeDisplayColonWidth * colons +
         kDefaultTimeDisplayDigitWidth * (formatted_time.length() - colons);
}

String MediaControlTimeDisplayElement::FormatTime() const {
  return MediaControlsSharedHelpers::FormatTime(current_value_);
}

void MediaControlTimeDisplayElement::SetAriaLabel() {
  String aria_label =
      GetLocale().QueryString(localized_resource_id_, FormatTime());
  setAttribute(html_names::kAriaLabelAttr, AtomicString(aria_label));
}

}  // namespace blink
