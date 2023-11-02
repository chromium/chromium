// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"

namespace blink {

// static
const char RemotePlaybackController::kSupplementName[] =
    "RemotePlaybackController";

// static
RemotePlaybackController* RemotePlaybackController::From(
    HTMLMediaElement& element) {
  return Supplement<HTMLMediaElement>::From<RemotePlaybackController>(element);
}

void RemotePlaybackController::Trace(Visitor* visitor) const {
  Supplement<HTMLMediaElement>::Trace(visitor);
}

RemotePlaybackController::RemotePlaybackController(HTMLMediaElement& element)
    : Supplement<HTMLMediaElement>(element) {}

// static
void RemotePlaybackController::ProvideTo(HTMLMediaElement& element,
                                         RemotePlaybackController* controller) {
  Supplement<HTMLMediaElement>::ProvideTo(element, controller);
}

}  // namespace blink
