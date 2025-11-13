// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"

namespace blink {

// static
RemotePlaybackController* RemotePlaybackController::From(
    HTMLMediaElement& element) {
  return element.GetRemotePlaybackController();
}

void RemotePlaybackController::Trace(Visitor* visitor) const {}

// static
void RemotePlaybackController::ProvideTo(HTMLMediaElement& element,
                                         RemotePlaybackController* controller) {
  element.SetRemotePlaybackController(controller);
}

}  // namespace blink
