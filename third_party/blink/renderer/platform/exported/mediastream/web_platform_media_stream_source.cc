// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

const char WebPlatformMediaStreamSource::kSourceId[] = "sourceId";

WebPlatformMediaStreamSource::WebPlatformMediaStreamSource() {}

WebPlatformMediaStreamSource::~WebPlatformMediaStreamSource() {
  DCHECK(stop_callback_.is_null());
  owner_ = nullptr;
}

void WebPlatformMediaStreamSource::StopSource() {
  DoStopSource();
  FinalizeStopSource();
}

void WebPlatformMediaStreamSource::FinalizeStopSource() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run(Owner());
  if (Owner())
    Owner().SetReadyState(WebMediaStreamSource::kReadyStateEnded);
}

void WebPlatformMediaStreamSource::SetSourceMuted(bool is_muted) {
  // Although this change is valid only if the ready state isn't already Ended,
  // there's code further along (like in MediaStreamTrack) which filters
  // that out already.
  if (!Owner())
    return;
  Owner().SetReadyState(is_muted ? WebMediaStreamSource::kReadyStateMuted
                                 : WebMediaStreamSource::kReadyStateLive);
}

void WebPlatformMediaStreamSource::SetDevice(const MediaStreamDevice& device) {
  device_ = device;
}

void WebPlatformMediaStreamSource::SetStopCallback(
    SourceStoppedCallback stop_callback) {
  DCHECK(stop_callback_.is_null());
  stop_callback_ = std::move(stop_callback);
}

void WebPlatformMediaStreamSource::ResetSourceStoppedCallback() {
  DCHECK(!stop_callback_.is_null());
  stop_callback_.Reset();
}

void WebPlatformMediaStreamSource::ChangeSource(
    const MediaStreamDevice& new_device) {
  DoChangeSource(new_device);
}

WebMediaStreamSource WebPlatformMediaStreamSource::Owner() {
  DCHECK(owner_);
  return WebMediaStreamSource(owner_.Get());
}

#if INSIDE_BLINK
void WebPlatformMediaStreamSource::SetOwner(MediaStreamSource* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}
#endif

}  // namespace blink
