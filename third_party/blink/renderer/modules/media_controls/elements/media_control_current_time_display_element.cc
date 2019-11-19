// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_current_time_display_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlCurrentTimeDisplayElement::MediaControlCurrentTimeDisplayElement(
    MediaControlsImpl& media_controls)
    : MediaControlTimeDisplayElement(media_controls,
                                     IDS_AX_MEDIA_CURRENT_TIME_DISPLAY) {
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-current-time-display"));
}

}  // namespace blink
