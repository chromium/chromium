// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

namespace blink {

namespace {

const char kNoDownload[] = "nodownload";
const char kNoFullscreen[] = "nofullscreen";
const char kNoPlaybackRate[] = "noplaybackrate";
const char kNoRemotePlayback[] = "noremoteplayback";

const char* const kSupportedTokens[] = {kNoDownload, kNoFullscreen,
                                        kNoPlaybackRate, kNoRemotePlayback};

}  // namespace

HTMLMediaElementControlsList::HTMLMediaElementControlsList(
    HTMLMediaElement* element)
    : DOMTokenList(*element, html_names::kControlslistAttr) {}

bool HTMLMediaElementControlsList::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  for (const char* supported_token : kSupportedTokens) {
    if (token_value == supported_token)
      return true;
  }
  return false;
}

bool HTMLMediaElementControlsList::ShouldHideDownload() const {
  return contains(kNoDownload);
}

bool HTMLMediaElementControlsList::ShouldHideFullscreen() const {
  return contains(kNoFullscreen);
}

bool HTMLMediaElementControlsList::ShouldHidePlaybackRate() const {
  return contains(kNoPlaybackRate);
}

bool HTMLMediaElementControlsList::ShouldHideRemotePlayback() const {
  return contains(kNoRemotePlayback);
}

bool HTMLMediaElementControlsList::CanShowAllControls() const {
  return ShouldHideDownload() || ShouldHideFullscreen() ||
         ShouldHidePlaybackRate() || ShouldHideRemotePlayback();
}

}  // namespace blink
