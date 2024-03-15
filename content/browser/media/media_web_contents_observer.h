// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/media/media_power_experiment_manager.h"
#include "content/browser/media/session/media_session_controllers_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/use_after_free_checker.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

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
class WebContentsImpl;

// This class manages all RenderFrame based media related managers at the
// browser side. It receives IPC messages from media RenderFrameObservers and
// forwards them to the corresponding managers. The managers are responsible
// for sending IPCs back to the RenderFrameObservers at the render side.
class CONTENT_EXPORT MediaWebContentsObserver
    : public WebContentsObserver,
      public media::mojom::MediaPlayerObserverClient {
 public:
  explicit MediaWebContentsObserver(WebContentsImpl* web_contents);

  MediaWebContentsObserver(const MediaWebContentsObserver&) = delete;
  MediaWebContentsObserver& operator=(const MediaWebContentsObserver&) = delete;

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
  const std::optional<MediaPlayerId>& GetFullscreenVideoMediaPlayerId() const;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;
  void DidUpdateAudioMutingState(bool muted) override;

  // MediaPlayerObserverClient implementation.
  void GetHasPlayedBefore(GetHasPlayedBeforeCallback callback) override;

  void BindMediaPlayerObserverClient(
      mojo::PendingReceiver<media::mojom::MediaPlayerObserverClient>
          pending_receiver);

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

  // Returns whether or not to be able to use the MediaPlayer mojo interface.
  bool IsMediaPlayerRemoteAvailable(const MediaPlayerId& player_id);

  // Return an already bound mojo Remote for the MediaPlayer mojo interface. It
  // is an error to call this method if no MediaPlayer with |player_id| exists.
  mojo::AssociatedRemote<media::mojom::MediaPlayer>& GetMediaPlayerRemote(
      const MediaPlayerId& player_id);

  // Creates a new MediaPlayerObserverHostImpl associated to |player_id| if
  // needed, and then passes |player_receiver| to it to establish a
  // communication channel.
  void BindMediaPlayerHost(
      GlobalRenderFrameHostId frame_routing_id,
      mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
          player_receiver);

  // Called by the WebContents when a tab has been closed but may still be
  // available for "undo" -- indicates that all media players (even audio only
  // players typically allowed background audio) bound to this WebContents must
  // be suspended.
  void SuspendAllMediaPlayers();

 protected:
  MediaSessionControllersManager* session_controllers_manager() {
    return session_controllers_manager_.get();
  }

 private:
  class PlayerInfo;
  using PlayerInfoMap =
      base::flat_map<MediaPlayerId, std::unique_ptr<PlayerInfo>>;

  // Helper class providing a per-RenderFrame object implementing the only
  // method of the media::mojom::MediaPlayerHost mojo interface, to provide the
  // renderer process with a way to notify the browser when a new MediaPlayer
  // has been created, so that a communication channel can be established.
  class MediaPlayerHostImpl : public media::mojom::MediaPlayerHost {
   public:
    MediaPlayerHostImpl(GlobalRenderFrameHostId frame_routing_id,
                        MediaWebContentsObserver* media_web_contents_observer);
    ~MediaPlayerHostImpl() override;

    // Used to bind receivers via the BrowserInterfaceBroker.
    void AddMediaPlayerHostReceiver(
        mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost>
            receiver);

    // media::mojom::MediaPlayerHost implementation.
    void OnMediaPlayerAdded(
        mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> media_player,
        mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
            media_player_observer,
        int32_t player_id) override;

   private:
    GlobalRenderFrameHostId frame_routing_id_;
    raw_ptr<MediaWebContentsObserver> media_web_contents_observer_;
    mojo::AssociatedReceiverSet<media::mojom::MediaPlayerHost> receivers_;
  };

  // Helper class providing a per-MediaPlayerId object implementing the
  // media::mojom::MediaPlayerObserver mojo interface.
  class MediaPlayerObserverHostImpl : public media::mojom::MediaPlayerObserver {
   public:
    MediaPlayerObserverHostImpl(
        const MediaPlayerId& media_player_id,
        MediaWebContentsObserver* media_web_contents_observer);
    ~MediaPlayerObserverHostImpl() override;

    // Used to bind the receiver via the BrowserInterfaceBroker.
    void BindMediaPlayerObserverReceiver(
        mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
            media_player_observer);

    // media::mojom::MediaPlayerObserver implementation.
    void OnMediaPlaying() override;
    void OnMediaPaused(bool stream_ended) override;
    void OnMutedStatusChanged(bool muted) override;
    void OnMediaMetadataChanged(
        bool has_audio,
        bool has_video,
        media::MediaContentType media_content_type) override;
    void OnMediaPositionStateChanged(
        const media_session::MediaPosition& media_position) override;
    void OnMediaEffectivelyFullscreenChanged(
        blink::WebFullscreenVideoStatus status) override;
    void OnMediaSizeChanged(const ::gfx::Size& size) override;
    void OnPictureInPictureAvailabilityChanged(bool available) override;
    void OnAudioOutputSinkChanged(const std::string& hashed_device_id) override;
    void OnUseAudioServiceChanged(bool uses_audio_service) override;
    void OnAudioOutputSinkChangingDisabled() override;
    void OnRemotePlaybackMetadataChange(
        media_session::mojom::RemotePlaybackMetadataPtr
            remote_playback_metadata) override;
    void OnVideoVisibilityChanged(bool meets_visibility_threshold) override;

   private:
    PlayerInfo* GetPlayerInfo();
    void NotifyAudioStreamMonitorIfNeeded();

    void OnReceivedMediaDeviceSalt(
        const std::string& hashed_device_id,
        const content::MediaDeviceSaltAndOrigin& salt_and_origin);
    void OnReceivedTranslatedDeviceId(
        const std::optional<std::string>& translated_id);

    const MediaPlayerId media_player_id_;
    const raw_ptr<MediaWebContentsObserver> media_web_contents_observer_;

    mojo::AssociatedReceiver<media::mojom::MediaPlayerObserver>
        media_player_observer_receiver_{this};

    // Helps monitor audio stream when not using AudioService.
    bool uses_audio_service_ = true;
    std::unique_ptr<AudioStreamMonitor::AudibleClientRegistration>
        audio_client_registration_;

    base::WeakPtrFactory<MediaPlayerObserverHostImpl> weak_factory_{this};
  };

  using MediaPlayerHostImplMap =
      base::flat_map<GlobalRenderFrameHostId,
                     std::unique_ptr<MediaPlayerHostImpl>>;
  using MediaPlayerObserverHostImplMap =
      base::flat_map<MediaPlayerId,
                     std::unique_ptr<MediaPlayerObserverHostImpl>>;
  using MediaPlayerRemotesMap =
      base::flat_map<MediaPlayerId,
                     mojo::AssociatedRemote<media::mojom::MediaPlayer>>;

  // Communicates with the MediaSessionControllersManager to find or create (if
  // needed) a MediaSessionController identified by |player_id|, in order to
  // bind its mojo remote for media::mojom::MediaPlayer.
  void OnMediaPlayerAdded(
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
      mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerObserver>
          media_player_observer,
      MediaPlayerId player_id);

  // Returns the PlayerInfo associated with |id|, or nullptr if no such
  // PlayerInfo exists.
  PlayerInfo* GetPlayerInfo(const MediaPlayerId& id) const;

  void OnMediaMetadataChanged(const MediaPlayerId& player_id,
                              bool has_video,
                              bool has_audio,
                              media::MediaContentType media_content_type);

  void OnMediaEffectivelyFullscreenChanged(
      const MediaPlayerId& player_id,
      blink::WebFullscreenVideoStatus fullscreen_status);
  void OnMediaPlaying();
  void OnAudioOutputSinkChangedWithRawDeviceId(
      const MediaPlayerId& player_id,
      const std::string& raw_device_id);
  void OnRemotePlaybackMetadataChange(
      const MediaPlayerId& player_id,
      media_session::mojom::RemotePlaybackMetadataPtr remote_playback_metadata);

  // Used to notify when the renderer -> browser mojo connection via the
  // interface media::mojom::MediaPlayerObserver gets disconnected.
  void OnMediaPlayerObserverDisconnected(const MediaPlayerId& player_id);

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
  raw_ptr<AudibleMetrics> audible_metrics_;

  // A boolean indicating whether media has played before.
  bool has_played_before_ = false;

  mojo::ReceiverSet<media::mojom::MediaPlayerObserverClient> receivers_;

  // Tracking variables and associated wake locks for media playback.
  PlayerInfoMap player_info_map_;
  mojo::Remote<device::mojom::WakeLock> audio_wake_lock_;
  std::optional<MediaPlayerId> fullscreen_player_;
  std::optional<bool> picture_in_picture_allowed_in_fullscreen_;
  bool has_audio_wake_lock_for_testing_ = false;

  std::unique_ptr<MediaSessionControllersManager> session_controllers_manager_;
  raw_ptr<MediaPowerExperimentManager> power_experiment_manager_ = nullptr;

  std::map<RenderFrameHost*,
           std::unique_ptr<base::WeakPtrFactory<MediaWebContentsObserver>>>
      per_frame_factory_;

  media::UseAfterFreeChecker use_after_free_checker_;

  MediaPlayerHostImplMap media_player_hosts_;
  MediaPlayerObserverHostImplMap media_player_observer_hosts_;

  // Map of remote endpoints for the media::mojom::MediaPlayer mojo interface,
  // indexed by MediaPlayerId.
  MediaPlayerRemotesMap media_player_remotes_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_WEB_CONTENTS_OBSERVER_H_
