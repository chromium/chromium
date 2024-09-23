// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class MediaSessionImpl;
class WebContentsImpl;

// Helper class for controlling a single player's MediaSession instance.  Sends
// browser side MediaSession commands back to a player hosted in the renderer
// process.
// MediaSessionController registers itself with MediaSessionImpl as the
// MediaSessionPlayerObserver for the associated player, and for that player
// only.  Consequently, it expects all MediaSessionPlayerObserver calls to
// occur for that player only.
class CONTENT_EXPORT MediaSessionController
    : public MediaSessionPlayerObserver {
 public:
  MediaSessionController(const MediaPlayerId& id,
                         WebContentsImpl* web_contents);

  MediaSessionController(const MediaSessionController&) = delete;
  MediaSessionController& operator=(const MediaSessionController&) = delete;

  ~MediaSessionController() override;

  // Must be called when media player metadata changes.
  void SetMetadata(bool has_audio,
                   bool has_video,
                   media::MediaContentType media_content_type);

  // Must be called when playback starts.  Returns false if a media session
  // cannot be created.
  bool OnPlaybackStarted();

  // Must be called when a pause occurs on the renderer side media player; keeps
  // the MediaSession instance in sync with renderer side behavior.
  void OnPlaybackPaused(bool reached_end_of_stream);

  // MediaSessionPlayerObserver implementation.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override;
  void OnEnterPictureInPicture(int player_id) override;
  void OnSetAudioSinkId(int player_id,
                        const std::string& raw_device_id) override;
  void OnSetMute(int player_id, bool mute) override;
  void OnRequestMediaRemoting(int player_id) override;
  void OnRequestVisibility(
      int player_id,
      RequestVisibilityCallback request_visibility_callback) override;
  RenderFrameHost* render_frame_host() const override;
  std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;
  bool IsPictureInPictureAvailable(int player_id) const override;
  bool HasSufficientlyVisibleVideo(int player_id) const override;
  bool HasAudio(int player_id) const override;
  bool HasVideo(int player_id) const override;
  bool IsPaused(int player_id) const override;
  std::string GetAudioOutputSinkId(int player_id) const override;
  bool SupportsAudioOutputDeviceSwitching(int player_id) const override;
  media::MediaContentType GetMediaContentType() const override;

  // Test helpers.
  int get_player_id_for_testing() const { return player_id_; }

  // Called when entering/leaving Picture-in-Picture for the given media
  // player.
  void PictureInPictureStateChanged(bool is_picture_in_picture);

  // Called when the WebContents is either muted or unmuted.
  void WebContentsMutedStateChanged(bool muted);

  // Called when the media position state of the player has changed.
  void OnMediaPositionStateChanged(
      const media_session::MediaPosition& position);

  void OnMediaMutedStatusChanged(bool mute);

  // Called when the media picture-in-picture availability has changed.
  void OnPictureInPictureAvailabilityChanged(bool available);

  // Called when the audio output device has changed.
  void OnAudioOutputSinkChanged(const std::string& raw_device_id);

  // Called when the ability to switch audio output devices has been disabled.
  void OnAudioOutputSinkChangingDisabled();

  // Called when the RemotePlayback metadata has changed.
  void OnRemotePlaybackMetadataChanged(
      media_session::mojom::RemotePlaybackMetadataPtr metadata);

  // Called when video visibility changes for the given media player.
  void OnVideoVisibilityChanged(bool meets_visibility_threshold);

 private:
  bool IsMediaSessionNeeded() const;

  // Determines whether a session is needed and adds or removes the player
  // accordingly.
  bool AddOrRemovePlayer();

  void OnHashedSinkIdReceived(const std::string& hashed_sink_id);

  const MediaPlayerId id_;

  // Outlives |this|.
  const raw_ptr<WebContentsImpl> web_contents_;

  // Outlives |this|.
  const raw_ptr<MediaSessionImpl> media_session_;

  std::optional<media_session::MediaPosition> position_;

  // These objects are only created on the UI thread, so this is safe.
  static int player_count_;
  const int player_id_ = player_count_++;

  bool is_paused_ = true;
  // Playing or paused, but not ended.
  bool is_playback_in_progress_ = false;
  bool has_audio_ = false;
  bool has_video_ = false;
  bool is_picture_in_picture_available_ = false;
  bool has_sufficiently_visible_video_ = false;
  std::string audio_output_sink_id_ =
      media::AudioDeviceDescription::kDefaultDeviceId;
  bool supports_audio_output_device_switching_ = true;
  media::MediaContentType media_content_type_ =
      media::MediaContentType::kPersistent;

  base::WeakPtrFactory<MediaSessionController> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_
