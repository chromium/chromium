// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/availability_callback_wrapper.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

namespace blink {

AvailabilityCallbackWrapper::AvailabilityCallbackWrapper(
    V8RemotePlaybackAvailabilityCallback* callback)
    : bindings_cb_(callback) {}

AvailabilityCallbackWrapper::AvailabilityCallbackWrapper(
    base::RepeatingClosure callback)
    : internal_cb_(std::move(callback)) {}

void AvailabilityCallbackWrapper::Run(RemotePlayback* remote_playback,
                                      bool new_availability) {
  if (internal_cb_) {
    DCHECK(!bindings_cb_);
    internal_cb_.Run();
    return;
  }

  bindings_cb_->InvokeAndReportException(remote_playback, new_availability);
}

void AvailabilityCallbackWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(bindings_cb_);
}

}  // namespace blink
