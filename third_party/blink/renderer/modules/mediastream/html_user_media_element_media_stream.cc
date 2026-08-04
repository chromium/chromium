// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"

namespace blink {

// static
const char HTMLUserMediaElementMediaStream::kSupplementName[] =
    "HTMLUserMediaElementMediaStream";

// static
HTMLUserMediaElementMediaStream& HTMLUserMediaElementMediaStream::From(
    HTMLUserMediaElement& element) {
  HTMLUserMediaElementMediaStream* supplement =
      Supplement<HTMLMediaCaptureElementBase>::From<
          HTMLUserMediaElementMediaStream>(element);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLUserMediaElementMediaStream>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

// static
MediaStream* HTMLUserMediaElementMediaStream::stream(
    HTMLUserMediaElement& element) {
  if (element.IsLegacyMode()) {
    return nullptr;
  }
  return From(element).GetMediaStream();
}

HTMLUserMediaElementMediaStream::HTMLUserMediaElementMediaStream(
    HTMLUserMediaElement& element)
    : Supplement<HTMLMediaCaptureElementBase>(element) {}

void HTMLUserMediaElementMediaStream::Trace(Visitor* visitor) const {
  visitor->Trace(media_stream_);
  Supplement<HTMLMediaCaptureElementBase>::Trace(visitor);
}

}  // namespace blink
