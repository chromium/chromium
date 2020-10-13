// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/browser/media/media_power_experiment_manager.h"
#include "content/browser/media/session/media_session_controllers_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/use_after_free_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace blink {
enum class WebFullscreenVideoStatus;
}  // namespace blink

namespace media {
enum class MediaContentType;
}  // namespace media

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

class AudibleMetrics;

// This class manages all RenderFrame based media related managers at the
// browser side. It receives IPC messages from media RenderFrameObservers and
// forwards them to the corresponding managers. The managers are responsible
// for sending IPCs back to the RenderFrameObservers at the render side.
class CONTENT_EXPORT MediaWebContentsObserver : public WebContentsObserver {
 public:
  explicit MediaWebContentsObserver(WebContents* web_contents);
  ~MediaWebContentsObserver() override;

  // Called by WebContentsImpl when the audible state may have changed.
  void MaybeUpdateAudibleState();

  // Called by WebContentsImpl to know if an active player is effectively
  // fullscreen. That means that the video is either fullscreen or it is the
  // content of a fullscreen page (in other words, a fullscreen video with
  // custom controls).
  // It should only be called while the WebContents is fullscreen.
  bool HasActiveEffectivelyFullscreenVideo() const;

  // Called by WebContentsImpl to know if Picture-in-Picture can be triggered
  // for the current active effectively fullscreen player.
  // It should only be called while the WebContents is fullscreen.
  bool IsPictureInPictureAllowedForFullscreenVideo() const;

  // Gets the MediaPlayerId of the fullscreen video if it exists.
  const base::Optional<MediaPlayerId>& GetFullscreenVideoMediaPlayerId() const;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;
  void DidUpdateAudioMutingState(bool muted) override;

  // TODO(zqzhang): this method is temporarily in MediaWebContentsObserver as
  // the effectively fullscreen video code is also here. We need to consider
  // merging the logic of effectively fullscreen, hiding media controls and
  // fullscreening video element to the same place.
  void RequestPersistentVideo(bool value);

  // Returns whether or not the given player id is active.
  bool IsPlayerActive(const MediaPlayerId& player_id) const;

  bool has_audio_wake_lock_for_testing() const {
    return has_audio_wake_lock_for_testing_;
  }

  void SetAudibleMetricsForTest(AudibleMetrics* audible_metrics) {
    audible_metrics_ = audible_metrics;
  }

  void OnReceivedTranslatedDeviceId(RenderFrameHost* render_frame_host,
                                    int delegate_id,
                                    const std::string& raw_device_id);

#if defined(OS_ANDROID)
  // Called by the WebContents when a tab has been closed but may still be
  // available for "undo" -- indicates that all media players (even audio only
  // players typically allowed background audio) bound to this WebContents must
  // be suspended.
  void SuspendAllMediaPlayers();
#endif  // defined(OS_ANDROID)
 protected:
  MediaSessionControllersManager* session_controllers_manager() {
    return &session_controllers_manager_;
  }

 private:
  class PlayerInfo;
  friend class PlayerInfo;

  using PlayerInfoMap =
      base::flat_map<MediaPlayerId, std::unique_ptr<PlayerInfo>>;

  // Returns the PlayerInfo associated with |id|, or nullptr if no such
  // PlayerInfo exists.
  PlayerInfo* GetPlayerInfo(const MediaPlayerId& id) const;

  void OnMediaDestroyed(RenderFrameHost* render_frame_host, int delegate_id);
  void OnMediaPaused(RenderFrameHost* render_frame_host,
                     int delegate_id,
                     bool reached_end_of_stream);
  void OnMediaMetadataChanged(RenderFrameHost* render_frame_host,
                              int delegate_id,
                              bool has_video,
                              bool has_audio,
                              media::MediaContentType media_content_type);
  void OnMediaPlaying(RenderFrameHost* render_frame_host, int delegate_id);
  void OnMediaEffectivelyFullscreenChanged(
      RenderFrameHost* render_frame_host,
      int delegate_id,
      blink::WebFullscreenVideoStatus fullscreen_status);
  void OnMediaSizeChanged(RenderFrameHost* render_frame_host,
                          int delegate_id,
                          const gfx::Size& size);
  void OnMediaMutedStatusChanged(RenderFrameHost* render_frame_host,
                                 int delegate_id,
                                 bool muted);
  void OnMediaPositionStateChanged(
      RenderFrameHost* render_frame_host,
      int delegate_id,
      const media_session::MediaPosition& position);
  void OnPictureInPictureAvailabilityChanged(RenderFrameHost* render_frame_host,
                                             int delegate_id,
                                             bool available);
  void OnAudioOutputSinkChanged(RenderFrameHost* render_frame_host,
                                int delegate_id,
                                std::string hashed_device_id);
  void OnAudioOutputSinkChangingDisabled(RenderFrameHost* render_frame_host,
                                         int delegate_id);
  void OnBufferUnderflow(RenderFrameHost* render_frame_host, int delegate_id);

  device::mojom::WakeLock* GetAudioWakeLock();

  // WakeLock related methods for audio and video.
  void LockAudio();
  void CancelAudioLock();
  void UpdateVideoLock();

  // Convenience method that casts web_contents() to a WebContentsImpl*.
  WebContentsImpl* web_contents_impl() const;

  // Notify |id| about |is_starting|.  Note that |id| might no longer be in the
  // active players list, which is fine.
  void OnExperimentStateChanged(MediaPlayerId id, bool is_starting);

  // Return a weak pointer to |this| that's local to |render_frame_host|, in the
  // sense that we can cancel all of the ptrs to one frame without cancelling
  // pointers for any of the others.
  base::WeakPtr<MediaWebContentsObserver> GetWeakPtrForFrame(
      RenderFrameHost* render_frame_host);

  // Helper class for recording audible metrics.
  AudibleMetrics* audible_metrics_;

  // Tracking variables and associated wake locks for media playback.
  PlayerInfoMap player_info_map_;
  mojo::Remote<device::mojom::WakeLock> audio_wake_lock_;
  base::Optional<MediaPlayerId> fullscreen_player_;
  base::Optional<bool> picture_in_picture_allowed_in_fullscreen_;
  bool has_audio_wake_lock_for_testing_ = false;

  MediaSessionControllersManager session_controllers_manager_;
  MediaPowerExperimentManager* power_experiment_manager_ = nullptr;

  std::map<RenderFrameHost*,
           std::unique_ptr<base::WeakPtrFactory<MediaWebContentsObserver>>>
      per_frame_factory_;

  media::UseAfterFreeChecker use_after_free_checker_;

  base::WeakPtrFactory<MediaWebContentsObserver> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
