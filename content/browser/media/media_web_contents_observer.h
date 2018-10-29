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
#include "content/browser/media/session/media_session_controllers_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace blink {
enum class WebFullscreenVideoStatus;
struct PictureInPictureControlInfo;
}  // namespace blink

namespace media {
enum class MediaContentType;
}  // namespace media

namespace gfx {
class Size;
}  // namespace size

namespace viz {
class SurfaceId;
}  // namespace viz

namespace content {

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

  // Gets the MediaPlayerId of the picture in picture video if it exists.
  const base::Optional<MediaPlayerId>& GetPictureInPictureVideoMediaPlayerId()
      const;

  // Reset the MediaPlayerId of the picture in picture video when user closes
  // Picture-in-Picture window manually.
  void ResetPictureInPictureVideoMediaPlayerId();

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidUpdateAudioMutingState(bool muted) override;

  // TODO(zqzhang): this method is temporarily in MediaWebContentsObserver as
  // the effectively fullscreen video code is also here. We need to consider
  // merging the logic of effectively fullscreen, hiding media controls and
  // fullscreening video element to the same place.
  void RequestPersistentVideo(bool value);

  // Returns whether or not the given player id is active.
  bool IsPlayerActive(const MediaPlayerId& player_id) const;

  // Called by the Picture-in-Picture controller when the associated window is
  // resized. |window_size| represents the new size of the window. It MUST be
  // called when there is a player in Picture-in-Picture.
  void OnPictureInPictureWindowResize(const gfx::Size& window_size);

  bool has_audio_wake_lock_for_testing() const {
    return has_audio_wake_lock_for_testing_;
  }

  bool has_video_wake_lock_for_testing() const { return has_video_wake_lock_; }

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
  void OnPictureInPictureModeStarted(RenderFrameHost* render_frame_host,
                                     int delegate_id,
                                     const viz::SurfaceId&,
                                     const gfx::Size& natural_size,
                                     int request_id,
                                     bool show_play_pause_button);
  void OnPictureInPictureModeEnded(RenderFrameHost* render_frame_host,
                                   int delegate_id,
                                   int request_id);
  void OnSetPictureInPictureCustomControls(
      RenderFrameHost* render_frame_host,
      int delegate_id,
      const std::vector<blink::PictureInPictureControlInfo>& controls);
  void OnPictureInPictureSurfaceChanged(RenderFrameHost*,
                                        int delegate_id,
                                        const viz::SurfaceId&,
                                        const gfx::Size&,
                                        bool show_play_pause_button);

  // Clear |render_frame_host|'s tracking entry for its WakeLocks.
  void ClearWakeLocks(RenderFrameHost* render_frame_host);

  device::mojom::WakeLock* GetAudioWakeLock();
  device::mojom::WakeLock* GetVideoWakeLock();

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

  // Internal method to exit Picture-in-Picture from an event received from the
  // renderer process.
  void ExitPictureInPictureInternal();

  // Convenience method that casts web_contents() to a WebContentsImpl*.
  WebContentsImpl* web_contents_impl() const;

  // Tracking variables and associated wake locks for media playback.
  ActiveMediaPlayerMap active_audio_players_;
  ActiveMediaPlayerMap active_video_players_;
  device::mojom::WakeLockPtr audio_wake_lock_;
  device::mojom::WakeLockPtr video_wake_lock_;
  base::Optional<MediaPlayerId> fullscreen_player_;
  base::Optional<MediaPlayerId> pip_player_;
  base::Optional<bool> picture_in_picture_allowed_in_fullscreen_;
  bool has_audio_wake_lock_for_testing_ = false;
  bool has_video_wake_lock_ = false;

  MediaSessionControllersManager session_controllers_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
