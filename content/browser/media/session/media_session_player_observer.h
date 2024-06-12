// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_PLAYER_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_PLAYER_OBSERVER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace media {
enum class MediaContentType;
}

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace content {

class RenderFrameHost;

class MediaSessionPlayerObserver {
 public:
  MediaSessionPlayerObserver() = default;
  virtual ~MediaSessionPlayerObserver() = default;

  // The given |player_id| has been suspended by the MediaSession.
  virtual void OnSuspend(int player_id) = 0;

  // The given |player_id| has been resumed by the MediaSession.
  virtual void OnResume(int player_id) = 0;

  // The given |player_id| has been seeked forward by the MediaSession.
  virtual void OnSeekForward(int player_id, base::TimeDelta seek_time) = 0;

  // The given |player_id| has been seeked backward by the MediaSession.
  virtual void OnSeekBackward(int player_id, base::TimeDelta seek_time) = 0;

  // The given |player_id| has been seeked to by the MediaSession.
  virtual void OnSeekTo(int player_id, base::TimeDelta seek_time) = 0;

  // The given |player_id| has been set a new volume multiplier by
  // the MediaSession.
  virtual void OnSetVolumeMultiplier(int player_id,
                                     double volume_multiplier) = 0;

  // The given |player_id| has been requested picture-in-picture.
  virtual void OnEnterPictureInPicture(int player_id) = 0;

  // The given |player_id| has been requested to route audio output to the
  // specified audio device.
  virtual void OnSetAudioSinkId(int player_id,
                                const std::string& raw_device_id) = 0;

  // The given |player_id| has been requested to mute or unmute.
  virtual void OnSetMute(int player_id, bool mute) = 0;

  // The given |player_id| has been requested to start Media Remoting.
  virtual void OnRequestMediaRemoting(int player_id) = 0;

  // `RequestVisibilityCallback` is used to enable computing video visibility
  // on-demand. The callback is passed to the MediaVideoVisibilityTracker, where
  // the on-demand visibility computation will take place.
  //
  // The boolean parameter represents whether a video element meets a given
  // visibility threshold. This threshold (`kVisibilityThreshold`) is defined by
  // the HTMLVideoElement.
  using RequestVisibilityCallback = base::OnceCallback<void(bool)>;

  // The given |player_id| has been requested to report its video visibility.
  virtual void OnRequestVisibility(
      int player_id,
      RequestVisibilityCallback request_visibility_callback) = 0;

  // Returns the position for |player_id|.
  virtual std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const = 0;

  // Returns if picture-in-picture is available for |player_id|.
  virtual bool IsPictureInPictureAvailable(int player_id) const = 0;

  // Returns if player's |player_id| video is sufficiently visible.
  virtual bool HasSufficientlyVisibleVideo(int player_id) const = 0;

  // Returns true if the |player_id| has audio tracks.
  virtual bool HasAudio(int player_id) const = 0;

  // Returns true if the |player_id| has video tracks.
  virtual bool HasVideo(int player_id) const = 0;

  // Returns true if `player_id` is paused.
  virtual bool IsPaused(int player_id) const = 0;

  // Returns the id of the audio output device used by |player_id|. Returns the
  // empty string if unavailable.
  virtual std::string GetAudioOutputSinkId(int player_id) const = 0;

  // Returns true if the |player_id| supports audio output device switching.
  virtual bool SupportsAudioOutputDeviceSwitching(int player_id) const = 0;

  virtual media::MediaContentType GetMediaContentType() const = 0;

  // Returns the RenderFrameHost this player observer belongs to. Returns
  // nullptr if unavailable.
  virtual RenderFrameHost* render_frame_host() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_PLAYER_OBSERVER_H_
