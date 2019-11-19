// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace {

// The during element has extra '/ ' in the text which takes approximately
// 9 pixels.
constexpr int kTimeDisplayExtraCharacterWidth = 9;

}  // namespace

namespace blink {

MediaControlRemainingTimeDisplayElement::
    MediaControlRemainingTimeDisplayElement(MediaControlsImpl& media_controls)
    : MediaControlTimeDisplayElement(media_controls,
                                     IDS_AX_MEDIA_TIME_REMAINING_DISPLAY) {
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-time-remaining-display"));
}

int MediaControlRemainingTimeDisplayElement::EstimateElementWidth() const {
  // Add extra pixel width for during display since we have an extra  "/ ".
  return kTimeDisplayExtraCharacterWidth +
         MediaControlTimeDisplayElement::EstimateElementWidth();
}

String MediaControlRemainingTimeDisplayElement::FormatTime() const {
  // For the duration display, we prepend a "/ " to deliminate the current time
  // from the duration, e.g. "0:12 / 3:45".
  return "/ " + MediaControlTimeDisplayElement::FormatTime();
}

}  // namespace blink
