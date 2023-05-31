// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/keywords.h"

namespace blink {

HTMLMediaElementControlsList::HTMLMediaElementControlsList(
    HTMLMediaElement* element)
    : DOMTokenList(*element, html_names::kControlslistAttr) {}

bool HTMLMediaElementControlsList::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  return token_value == keywords::kNodownload ||
         token_value == keywords::kNofullscreen ||
         token_value == keywords::kNoplaybackrate ||
         token_value == keywords::kNoremoteplayback;
}

bool HTMLMediaElementControlsList::ShouldHideDownload() const {
  return contains(keywords::kNodownload);
}

bool HTMLMediaElementControlsList::ShouldHideFullscreen() const {
  return contains(keywords::kNofullscreen);
}

bool HTMLMediaElementControlsList::ShouldHidePlaybackRate() const {
  return contains(keywords::kNoplaybackrate);
}

bool HTMLMediaElementControlsList::ShouldHideRemotePlayback() const {
  return contains(keywords::kNoremoteplayback);
}

bool HTMLMediaElementControlsList::CanShowAllControls() const {
  return ShouldHideDownload() || ShouldHideFullscreen() ||
         ShouldHidePlaybackRate() || ShouldHideRemotePlayback();
}

}  // namespace blink
