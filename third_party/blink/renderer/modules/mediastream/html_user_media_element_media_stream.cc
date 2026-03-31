// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"

namespace blink {

// static
const char HTMLUserMediaElementMediaStream::kSupplementName[] =
    "HTMLUserMediaElementMediaStream";

// static
HTMLUserMediaElementMediaStream& HTMLUserMediaElementMediaStream::From(
    HTMLUserMediaElement& element) {
  HTMLUserMediaElementMediaStream* supplement =
      Supplement<HTMLUserMediaElement>::From<HTMLUserMediaElementMediaStream>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLUserMediaElementMediaStream>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

// static
MediaStream* HTMLUserMediaElementMediaStream::stream(
    HTMLUserMediaElement& element) {
  return From(element).GetMediaStream();
}

// static
const V8UnionDOMExceptionOrOverconstrainedError*
HTMLUserMediaElementMediaStream::error(HTMLUserMediaElement& element) {
  return From(element).error_.Get();
}

HTMLUserMediaElementMediaStream::HTMLUserMediaElementMediaStream(
    HTMLUserMediaElement& element)
    : Supplement<HTMLUserMediaElement>(element) {}

void HTMLUserMediaElementMediaStream::Trace(Visitor* visitor) const {
  visitor->Trace(media_stream_);
  visitor->Trace(error_);
  Supplement<HTMLUserMediaElement>::Trace(visitor);
}

}  // namespace blink
