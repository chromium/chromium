// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_controls.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

namespace blink {

namespace {

// The sizing class thresholds in pixels.
constexpr int kMediaControlsSizingMediumThreshold = 741;
constexpr int kMediaControlsSizingLargeThreshold = 1441;

}  // namespace

// static
MediaControlsSizingClass MediaControls::GetSizingClass(int width) {
  if (width < kMediaControlsSizingMediumThreshold)
    return MediaControlsSizingClass::kSmall;
  if (width < kMediaControlsSizingLargeThreshold)
    return MediaControlsSizingClass::kMedium;

  return MediaControlsSizingClass::kLarge;
}

// static
AtomicString MediaControls::GetSizingCSSClass(
    MediaControlsSizingClass sizing_class) {
  switch (sizing_class) {
    case MediaControlsSizingClass::kSmall:
      return AtomicString(kMediaControlsSizingSmallCSSClass);
    case MediaControlsSizingClass::kMedium:
      return AtomicString(kMediaControlsSizingMediumCSSClass);
    case MediaControlsSizingClass::kLarge:
      return AtomicString(kMediaControlsSizingLargeCSSClass);
  }

  NOTREACHED_IN_MIGRATION();
}

MediaControls::MediaControls(HTMLMediaElement& media_element)
    : media_element_(&media_element) {}

HTMLMediaElement& MediaControls::MediaElement() const {
  return *media_element_;
}

void MediaControls::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
}

}  // namespace blink
