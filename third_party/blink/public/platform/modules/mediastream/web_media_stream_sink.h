// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_

#include <optional>

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// WebMediaStreamSink is the base interface for WebMediaStreamAudioSink and
// WebMediaStreamVideoSink. It allows an implementation to receive notifications
// about state changes on a WebMediaStreamSource object or such an
// object underlying a WebMediaStreamTrack.
class BLINK_PLATFORM_EXPORT WebMediaStreamSink {
 public:
  virtual void OnReadyStateChanged(WebMediaStreamSource::ReadyState state) {}
  virtual void OnEnabledChanged(bool enabled) {}
  virtual void OnContentHintChanged(
      WebMediaStreamTrack::ContentHintType content_hint) {}

  // OnVideoConstraintsChanged is called when constraints set on the source
  // MediaStreamVideoTrack change. Never called in case the sink isn't connected
  // to a video track.
  virtual void OnVideoConstraintsChanged(std::optional<double> min_fps,
                                         std::optional<double> max_fps) {}

  // IsSecure indicates if this sink is secure (i.e. meets
  // output protection requirement). Generally, this should be kNo unless you
  // know what you are doing. Encoded sinks are never secure.
  enum class IsSecure { kNo, kYes };

  // UsesAlpha indicates if this sink might use its source's
  // alpha channel (if the source has one). This should be kDefault unless it is
  // guaranteed that the alpha channel of |track| will be ignored. If
  // kDependsOnOtherSinks is used, the sink will not receive alpha if all other
  // sinks do not use alpha.
  enum class UsesAlpha { kDefault, kDependsOnOtherSinks, kNo };

 protected:
  virtual ~WebMediaStreamSink() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_SINK_H_
