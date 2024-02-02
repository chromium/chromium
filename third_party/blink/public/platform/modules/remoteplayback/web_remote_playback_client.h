// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_WEB_REMOTE_PLAYBACK_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_WEB_REMOTE_PLAYBACK_CLIENT_H_

#include <optional>

namespace media {
enum class VideoCodec;
enum class AudioCodec;
}  // namespace media

namespace blink {

class WebURL;
class WebString;

// The interface between the HTMLMediaElement and its
// HTMLMediaElementRemotePlayback supplement.
class WebRemotePlaybackClient {
 public:
  virtual ~WebRemotePlaybackClient() = default;

  // Returns if the remote playback available for this media element.
  virtual bool RemotePlaybackAvailable() const = 0;

  // Notifies the client that the source of the HTMLMediaElement has changed as
  // well as if the new source is supported for remote playback.
  virtual void SourceChanged(const WebURL&, bool is_source_supported) = 0;

  virtual void MediaMetadataChanged(
      std::optional<media::VideoCodec> video_codec,
      std::optional<media::AudioCodec> audio_codec) = 0;

  // Gets the presentation ID associated with the client. The presentation ID
  // may be null, empty or stale.
  virtual WebString GetPresentationId() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_REMOTEPLAYBACK_WEB_REMOTE_PLAYBACK_CLIENT_H_
