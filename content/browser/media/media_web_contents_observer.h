// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/media/media_power_experiment_manager.h"
#include "content/browser/media/session/media_session_controllers_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
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
}  // namespace size

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

  using PlayerSet = std::set<int>;
  using ActiveMediaPlayerMap = std::map<RenderFrameHost*, PlayerSet>;

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
  void OnMediaDestroyed(RenderFrameHost* render_frame_host, int delegate_id);
  void OnMediaPaused(RenderFrameHost* render_frame_host,
                     int delegate_id,
                     bool reached_end_of_stream);
  void OnMediaPlaying(RenderFrameHost* render_frame_host,
                      int delegate_id,
                      bool has_video,
                      bool has_audio,
                      bool is_remote,
                      media::MediaContentType media_content_type);
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

  // Clear |render_frame_host|'s tracking entry for its WakeLocks.
  void ClearWakeLocks(RenderFrameHost* render_frame_host);

  device::mojom::WakeLock* GetAudioWakeLock();

  // WakeLock related methods for audio and video.
  void LockAudio();
  void CancelAudioLock();
  void UpdateVideoLock();

  // Helper methods for adding or removing player entries in |player_map|.
  void AddMediaPlayerEntry(const MediaPlayerId& id,
                           ActiveMediaPlayerMap* player_map);
  // Returns true if an entry is actually removed.
  bool RemoveMediaPlayerEntry(const MediaPlayerId& id,
                              ActiveMediaPlayerMap* player_map);
  // Removes all entries from |player_map| for |render_frame_host|. Removed
  // entries are added to |removed_players|.
  void RemoveAllMediaPlayerEntries(RenderFrameHost* render_frame_host,
                                   ActiveMediaPlayerMap* player_map,
                                   std::set<MediaPlayerId>* removed_players);

  // Convenience method that casts web_contents() to a WebContentsImpl*.
  WebContentsImpl* web_contents_impl() const;

  // Notify |id| about |is_starting|.  Note that |id| might no longer be in the
  // active players list, which is fine.
  void OnExperimentStateChanged(MediaPlayerId id, bool is_starting);

  // Remove all players from |player_map|.
  void RemoveAllPlayers(ActiveMediaPlayerMap* player_map);

  // Remove all players.
  void RemoveAllPlayers();

  // Return a weak pointer to |this| that's local to |render_frame_host|, in the
  // sense that we can cancel all of the ptrs to one frame without cancelling
  // pointers for any of the others.
  base::WeakPtr<MediaWebContentsObserver> GetWeakPtrForFrame(
      RenderFrameHost* render_frame_host);

  // Helper class for recording audible metrics.
  AudibleMetrics* audible_metrics_;

  // Tracking variables and associated wake locks for media playback.
  ActiveMediaPlayerMap active_audio_players_;
  ActiveMediaPlayerMap active_video_players_;
  mojo::Remote<device::mojom::WakeLock> audio_wake_lock_;
  base::Optional<MediaPlayerId> fullscreen_player_;
  base::Optional<bool> picture_in_picture_allowed_in_fullscreen_;
  bool has_audio_wake_lock_for_testing_ = false;

  MediaSessionControllersManager session_controllers_manager_;
  MediaPowerExperimentManager* power_experiment_manager_ = nullptr;

  std::map<RenderFrameHost*,
           std::unique_ptr<base::WeakPtrFactory<MediaWebContentsObserver>>>
      per_frame_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
