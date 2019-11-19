// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace blink {

// MediaStreamSink is the base interface for MediaStreamAudioSink and
// MediaStreamVideoSink. It allows an implementation to receive notifications
// about state changes on a WebMediaStreamSource object or such an
// object underlying a WebMediaStreamTrack.
class BLINK_PLATFORM_EXPORT WebMediaStreamSink {
 public:
  virtual void OnReadyStateChanged(WebMediaStreamSource::ReadyState state) {}
  virtual void OnEnabledChanged(bool enabled) {}
  virtual void OnContentHintChanged(
      WebMediaStreamTrack::ContentHintType content_hint) {}

 protected:
  virtual ~WebMediaStreamSink() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_
