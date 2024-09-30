// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_uma_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/presentation_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media_session {
struct MediaMetadata;
}  // namespace media_session

namespace content {

class AudioFocusManagerTest;
class MediaSessionImplServiceRoutingTest;
class MediaSessionImplServiceRoutingThrottleTest;
class MediaSessionImplStateObserver;
class MediaSessionImplVisibilityBrowserTest;
class MediaSessionPlayerObserver;
class MediaSessionServiceImpl;
class MediaSessionServiceImplBrowserTest;

#if BUILDFLAG(IS_ANDROID)
class MediaSessionAndroid;
#endif  // BUILDFLAG(IS_ANDROID)

// MediaSessionImpl is the implementation of MediaSession. It manages the media
// session and audio focus for a given WebContents. It is requesting the audio
// focus, pausing when requested by the system and dropping it on demand. The
// audio focus can be of two types: Transient or Content. A Transient audio
// focus will allow other players to duck instead of pausing and will be
// declared as temporary to the system. A Content audio focus will not be
// declared as temporary and will not allow other players to duck. If a given
// WebContents can only have one audio focus at a time, it will be Content in
// case of Transient and Content audio focus are both requested.
// TODO(thakis,mlamouri): MediaSessionImpl isn't CONTENT_EXPORT'd because it
// creates complicated build issues with WebContentsUserData being a
// non-exported template, see https://crbug.com/589840. As a result, the class
// uses CONTENT_EXPORT for methods that are being used from tests.
// CONTENT_EXPORT should be moved back to the class when the Windows build will
// work with it.
class MediaSessionImpl : public MediaSession,
                         public WebContentsObserver,
                         public WebContentsUserData<MediaSessionImpl>,
                         public PresentationObserver {
 public:
  enum class State { ACTIVE, SUSPENDED, INACTIVE };

  // Returns the MediaSessionImpl associated to this WebContents. Creates one if
  // none is currently available.
  CONTENT_EXPORT static MediaSessionImpl* Get(WebContents* web_contents);

  MediaSessionImpl(const MediaSessionImpl&) = delete;
  MediaSessionImpl& operator=(const MediaSessionImpl&) = delete;

  ~MediaSessionImpl() override;

  CONTENT_EXPORT void SetDelegateForTests(
      std::unique_ptr<AudioFocusDelegate> delegate);

#if BUILDFLAG(IS_ANDROID)
  void ClearMediaSessionAndroid();
  MediaSessionAndroid* GetMediaSessionAndroid();
#endif  // BUILDFLAG(IS_ANDROID)

  void NotifyMediaSessionMetadataChange();

  // Adds the given player to the current media session. Returns whether the
  // player was successfully added. If it returns false, AddPlayer() should be
  // called again later.
  CONTENT_EXPORT bool AddPlayer(MediaSessionPlayerObserver* observer,
                                int player_id);

  // Removes the given player from the current media session. Abandons audio
  // focus if that was the last player in the session.
  CONTENT_EXPORT void RemovePlayer(MediaSessionPlayerObserver* observer,
                                   int player_id);

  // Removes all the players associated with |observer|. Abandons audio focus if
  // these were the last players in the session.
  CONTENT_EXPORT void RemovePlayers(MediaSessionPlayerObserver* observer);

  // Record that the session was ducked.
  void RecordSessionDuck();

  // Called when a player is paused in the content.
  // If the paused player is the last player, we suspend the MediaSession.
  // Otherwise, the paused player will be removed from the MediaSession.
  CONTENT_EXPORT void OnPlayerPaused(MediaSessionPlayerObserver* observer,
                                     int player_id);

  // Called when the position state of the session might have changed.
  CONTENT_EXPORT void RebuildAndNotifyMediaPositionChanged();

  // Returns if the session is currently active.
  CONTENT_EXPORT bool IsActive() const;

  // Returns if the session is currently suspended.
  CONTENT_EXPORT bool IsSuspended() const;

  // Returns whether the session has Pepper instances.
  CONTENT_EXPORT bool HasPepper() const;

  // WebContentsObserver implementation
  void WebContentsDestroyed() override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnWebContentsFocused(RenderWidgetHost*) override;
  void OnWebContentsLostFocus(RenderWidgetHost*) override;
  void TitleWasSet(NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;

  // MediaSessionService-related methods

  // Called when a MediaSessionService is created, which registers itself to
  // this session.
  void OnServiceCreated(MediaSessionServiceImpl* service);
  // Called when a MediaSessionService is destroyed, which unregisters itself
  // from this session.
  void OnServiceDestroyed(MediaSessionServiceImpl* service);

  // Called when the playback state of a MediaSessionService has
  // changed. Will notify observers of media session state change.
  void OnMediaSessionPlaybackStateChanged(MediaSessionServiceImpl* service);

  // Called when the metadata of a MediaSessionService has changed. Will notify
  // observers if the service is currently routed.
  void OnMediaSessionMetadataChanged(MediaSessionServiceImpl* service);

  // Called when the actions of a MediaSessionService has changed. Will notify
  // observers if the service is currently routed.
  void OnMediaSessionActionsChanged(MediaSessionServiceImpl* service);

  // Called when the info of a MediaSessionService has changed. Will notify
  // observers if the service is currently routed.
  void OnMediaSessionInfoChanged(MediaSessionServiceImpl* service);

  // Requests audio focus to the AudioFocusDelegate.
  // Returns whether the request was granted.
  CONTENT_EXPORT AudioFocusDelegate::AudioFocusResult RequestSystemAudioFocus(
      media_session::mojom::AudioFocusType audio_focus_type);

  // Creates a binding between |this| and |request|.
  mojo::PendingRemote<media_session::mojom::MediaSession> AddRemote();

  // Returns information about the MediaSession.
  CONTENT_EXPORT media_session::mojom::MediaSessionInfoPtr
  GetMediaSessionInfoSync();

  // Returns if the session can be controlled by the user.
  CONTENT_EXPORT bool IsControllable() const;

  // MediaSession overrides ---------------------------------------------------

  // Resume the media session.
  // |type| represents the origin of the request.
  CONTENT_EXPORT void Resume(MediaSession::SuspendType suspend_type) override;

  // Stop the media session.
  // |type| represents the origin of the request.
  CONTENT_EXPORT void Stop(MediaSession::SuspendType suspend_type) override;

  // Seek the media session.
  CONTENT_EXPORT void Seek(base::TimeDelta seek_time) override;

  // Called when a MediaSessionAction is received. The action will be forwarded
  // to blink::MediaSession corresponding to the current routed service.
  void DidReceiveAction(
      media_session::mojom::MediaSessionAction action) override;

  // Set the volume multiplier applied during ducking.
  CONTENT_EXPORT void SetDuckingVolumeMultiplier(double multiplier) override;

  // Set the audio focus group id for this media session. Sessions in the same
  // group can share audio focus. Setting this to null will use the browser
  // default value.
  CONTENT_EXPORT void SetAudioFocusGroupId(
      const base::UnguessableToken& group_id) override;

  // Suspend the media session.
  // |type| represents the origin of the request.
  CONTENT_EXPORT void Suspend(MediaSession::SuspendType suspend_type) override;

  // Let the media session start ducking such that the volume multiplier is
  // reduced.
  CONTENT_EXPORT void StartDucking() override;

  // Let the media session stop ducking such that the volume multiplier is
  // recovered.
  CONTENT_EXPORT void StopDucking() override;

  // Returns information about the MediaSession. The sync method is not actually
  // slower and should be used over the async one which is available over mojo.
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;

  // Returns debugging information to be displayed on chrome://media-internals.
  void GetDebugInfo(GetDebugInfoCallback) override;

  // Adds a mojo based observer to listen to events related to this session.
  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer)
      override;

  // Called by |AudioFocusDelegate| when an async audio focus request is
  // completed.
  CONTENT_EXPORT void FinishSystemAudioFocusRequest(
      media_session::mojom::AudioFocusType type,
      bool result);

  // Skip to the previous track.
  CONTENT_EXPORT void PreviousTrack() override;

  // Skip to the next track.
  CONTENT_EXPORT void NextTrack() override;

  // Skip ad.
  CONTENT_EXPORT void SkipAd() override;

  // Go back to previous slide.
  CONTENT_EXPORT void PreviousSlide() override;

  // Go to next slide.
  CONTENT_EXPORT void NextSlide() override;

  // Seek the media session to a specific time.
  void SeekTo(base::TimeDelta seek_time) override;

  // Scrub ("fast seek") the media session to a specific time.
  void ScrubTo(base::TimeDelta seek_time) override;

  // Enter picture-in-picture.
  void EnterPictureInPicture() override;

  // Exit picture-in-picture.
  void ExitPictureInPicture() override;

  // Automatically enter picture-in-picture from a non-user source (e.g. in
  // reaction to content being hidden).
  void EnterAutoPictureInPicture() override;

  // Routes the audio from this Media Session to the given output device. If
  // |id| is null, we will route to the default output device.
  // Players created after this setting has been set will also have their audio
  // rerouted. This setting persists until cross-origin navigation occurs, the
  // renderer reports an audio sink change to a device different from |id|, or
  // this method is called again.
  void SetAudioSinkId(const std::optional<std::string>& id) override;

  // Mute/Unmute the microphone for a WebRTC session.
  void ToggleMicrophone() override;

  // Turn on or off the camera for a WebRTC session.
  void ToggleCamera() override;

  // Hang up a WebRTC session.
  void HangUp() override;

  // Brings the associated tab into focus.
  void Raise() override;

  // Mute or unmute the media player.
  void SetMute(bool mute) override;

  // Request the media player to start Media Remoting once there are available
  // sinks.
  void RequestMediaRemoting() override;

  // PresentationObserver:
  void OnPresentationsChanged(bool has_presentation) override;

  // Downloads the bitmap version of a MediaImage at least |minimum_size_px|
  // and closest to |desired_size_px|. If the download failed, was too small or
  // the image did not come from the media session then returns a null image.
  CONTENT_EXPORT void GetMediaImageBitmap(
      const media_session::MediaImage& image,
      int minimum_size_px,
      int desired_size_px,
      GetMediaImageBitmapCallback callback) override;

  const base::UnguessableToken& audio_focus_group_id() const {
    return audio_focus_group_id_;
  }

  void OnMediaMutedStatusChanged(bool mute);

  void OnPictureInPictureAvailabilityChanged();

  // Called when any of the normal players have switched to a different audio
  // output device.
  void OnAudioOutputSinkIdChanged();

  // Called when any of the normal players can no longer support audio output
  // device switching.
  void OnAudioOutputSinkChangingDisabled();

  // Called when any of the normal players video visibility changes.
  CONTENT_EXPORT void OnVideoVisibilityChanged();

  // Update the value of `remote_playback_metadata_`.
  CONTENT_EXPORT void SetRemotePlaybackMetadata(
      media_session::mojom::RemotePlaybackMetadataPtr metadata);

  // Returns whether the action should be routed to |routed_service_|.
  bool ShouldRouteAction(media_session::mojom::MediaSessionAction action) const;

  // Returns the source ID which links media sessions on the same browser
  // context together.
  CONTENT_EXPORT const base::UnguessableToken& GetSourceId() const;

  // Returns the Audio Focus request ID associated with this media session.
  const base::UnguessableToken& GetRequestId() const;

  // Returns a WeakPtr to `this`.
  base::WeakPtr<MediaSessionImpl> GetWeakPtr();

  CONTENT_EXPORT bool HasImageCacheForTest(const GURL& image_url) const;

  // Make sure that all observers have received any pending callbacks from us,
  // that might otherwise be sitting in a message pipe somewhere.
  void flush_observers_for_testing() { observers_.FlushForTesting(); }

 private:
  friend class content::WebContentsUserData<MediaSessionImpl>;
  friend class MediaSessionImplBrowserTest;
  friend class content::MediaSessionImplVisibilityBrowserTest;
  friend class content::AudioFocusManagerTest;
  friend class content::MediaSessionImplServiceRoutingTest;
  friend class content::MediaSessionImplServiceRoutingThrottleTest;
  friend class content::MediaSessionImplStateObserver;
  friend class content::MediaSessionServiceImplBrowserTest;
  friend class MediaSessionImplTest;
  friend class MediaSessionImplDurationThrottleTest;
  friend class MediaInternalsAudioFocusTest;
  friend class WebAppSystemMediaControlsBrowserTest;

  CONTENT_EXPORT void RemoveAllPlayersForTest();
  CONTENT_EXPORT MediaSessionUmaHelper* uma_helper_for_test();

  // Representation of a player for the MediaSessionImpl.
  struct PlayerIdentifier {
    PlayerIdentifier(MediaSessionPlayerObserver* observer, int player_id);

    PlayerIdentifier(const PlayerIdentifier&) = default;
    PlayerIdentifier(PlayerIdentifier&&) = default;

    PlayerIdentifier& operator=(const PlayerIdentifier&) = default;
    PlayerIdentifier& operator=(PlayerIdentifier&&) = default;

    bool operator==(const PlayerIdentifier& other) const;
    bool operator!=(const PlayerIdentifier& other) const;
    bool operator<(const PlayerIdentifier& other) const;
    // RAW_PTR_EXCLUSION: #union
    RAW_PTR_EXCLUSION MediaSessionPlayerObserver* observer;
    int player_id;
  };

  CONTENT_EXPORT explicit MediaSessionImpl(WebContents* web_contents);

  void Initialize();

  // Called when we have finished downloading an image.
  void OnImageDownloadComplete(GetMediaImageBitmapCallback callback,
                               int minimum_size_px,
                               int desired_size_px,
                               bool source_icon,
                               int id,
                               int http_status_code,
                               const GURL& image_url,
                               const std::vector<SkBitmap>& bitmaps,
                               const std::vector<gfx::Size>& sizes);

  // Called when system audio focus has been requested and whether the request
  // was granted.
  void OnSystemAudioFocusRequested(bool result);

  CONTENT_EXPORT void OnSuspendInternal(MediaSession::SuspendType suspend_type,
                                        State new_state);
  CONTENT_EXPORT void OnResumeInternal(MediaSession::SuspendType suspend_type);

  // To be called after a call to AbandonAudioFocus() in order request the
  // delegate to abandon the audio focus.
  CONTENT_EXPORT void AbandonSystemAudioFocusIfNeeded();

  // Internal method that should be used instead of setting audio_focus_state_.
  // It sets audio_focus_state_ and notifies observers about the state change.
  void SetAudioFocusState(State audio_focus_state);

  // Flushes any mojo bindings for testing.
  CONTENT_EXPORT void FlushForTesting();

  // Notifies |observers_| and |delegate_| that |MediaSessionInfo| has changed.
  void RebuildAndNotifyMediaSessionInfoChanged();

  // Update the volume multiplier when ducking state changes.
  void UpdateVolumeMultiplier();

  // Get the volume multiplier, which depends on whether the media session is
  // ducking.
  double GetVolumeMultiplier() const;

  CONTENT_EXPORT bool AddPepperPlayer(MediaSessionPlayerObserver* observer,
                                      int player_id);

  CONTENT_EXPORT bool AddOneShotPlayer(MediaSessionPlayerObserver* observer,
                                       int player_id);

  // Returns true if there is at least one player and all the players are
  // one-shot.
  bool HasOnlyOneShotPlayers() const;

  // MediaSessionService-related methods

  // Called when the routed service may have changed.
  void UpdateRoutedService();

  // Returns whether the frame |rfh| uses MediaSession API.
  bool IsServiceActiveForRenderFrameHost(RenderFrameHost* rfh);

  // Compute the MediaSessionService that should be routed, which will be used
  // to update |routed_service_|.
  CONTENT_EXPORT MediaSessionServiceImpl* ComputeServiceForRouting();

  // Rebuilds |actions_| and notifies observers if they have changed.
  void RebuildAndNotifyActionsChanged();

  // Rebuilds |metadata_| and |images_| and notifies observers if they have
  // changed.
  void RebuildAndNotifyMetadataChanged();

#if BUILDFLAG(IS_CHROMEOS)
  void BuildPlaceholderMetadata(
      media_session::MediaMetadata& metadata,
      std::vector<media_session::MediaImage>& artwork);
#endif

  void BuildMetadata(media_session::MediaMetadata& metadata,
                     std::vector<media_session::MediaImage>& artwork);

  bool IsPictureInPictureAvailable() const;

  // Iterates over all |normal_players_| and returns true if any of the players'
  // videos is sufficiently visible, false otherwise.
  CONTENT_EXPORT bool HasSufficientlyVisibleVideo() const;

  // Iterates over all |normal_players_| and returns true if any of the players'
  // videos is sufficiently visible, false otherwise.
  //
  // This is very similar to `HasSufficientlyVisibleVideo`, however this method
  // is used to get notifications on demand, while `HasSufficientlyVisibleVideo`
  // is constantly reporting visibility.
  void GetVisibility(GetVisibilityCallback get_visibility_callback) override;

  // Returns the device ID for the audio output device being used by all of the
  // normal players. If the players are not all using the same audio output
  // device, the id of the default device will be returned.
  std::string GetSharedAudioOutputDeviceId() const;

  bool IsAudioOutputDeviceSwitchingSupported() const;

  // Called when a MediaSessionAction is received. The action will be forwarded
  // to blink::MediaSession corresponding to the current routed service.
  void DidReceiveAction(media_session::mojom::MediaSessionAction action,
                        blink::mojom::MediaSessionActionDetailsPtr details);

  // Returns the media audio video state for each player. This is whether the
  // players associated with the media session are audio-only, video-only, or
  // have both audio and video. If we have a |routed_service_| then we limit to
  // players on that frame because this should align with the metadata.
  std::vector<media_session::mojom::MediaAudioVideoState>
  GetMediaAudioVideoStates();

  // Calls the callback with each |PlayerIdentifier| for every player associated
  // with this media session.
  void ForAllPlayers(base::RepeatingCallback<void(const PlayerIdentifier&)>);

  // Restrict duration update under certain frequency.
  std::optional<media_session::MediaPosition> MaybeGuardDurationUpdate(
      std::optional<media_session::MediaPosition> position);

  void IncreaseDurationUpdateAllowance();

  void ResetDurationUpdateGuard();

  CONTENT_EXPORT void SetShouldThrottleDurationUpdateForTest(
      bool should_throttle);

  // Duration update allowance is inscreasing by 1 every 20 seconds, and
  // capped at 3. Every duration updates will consume 1 allowance, and
  // if updates happen when we have 0 allowance, we consider the media as
  // a livestream and stop instreasing allowance until the time difference
  // between two updates is greater than 20 seconds.
  CONTENT_EXPORT static constexpr int kDurationUpdateMaxAllowance = 3;
  CONTENT_EXPORT static constexpr base::TimeDelta
      kDurationUpdateAllowanceIncreaseInterval = base::Seconds(20);

  // A set of actions supported by |routed_service_| and the current media
  // session.
  std::set<media_session::mojom::MediaSessionAction> actions_;

  std::unique_ptr<AudioFocusDelegate> delegate_;
  std::map<PlayerIdentifier, media_session::mojom::AudioFocusType>
      normal_players_;
  base::flat_set<PlayerIdentifier> pepper_players_;

  // Players that are playing in the web contents but we cannot control (e.g.
  // WebAudio or MediaStream).
  base::flat_set<PlayerIdentifier> one_shot_players_;

  // Players that are removed from |normal_players_| temporarily when the page
  // goes to back-forward cache. When the page is restored from the cache, these
  // players are also restored to |normal_players_|.
  base::flat_set<PlayerIdentifier> hidden_players_;

  State audio_focus_state_ = State::INACTIVE;
  MediaSession::SuspendType suspend_type_;

  // The |desired_audio_focus_type_| is the AudioFocusType we will request when
  // we request system audio focus.
  media_session::mojom::AudioFocusType desired_audio_focus_type_;

  // The last updated |MediaSessionInfo| that was sent to |observers_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // The last updated |MediaPosition| that was sent to |observers_|.
  std::optional<media_session::MediaPosition> position_;

  MediaSessionUmaHelper uma_helper_;

  // The ducking state of this media session. The initial value is |false|, and
  // is set to |true| after StartDucking(), and will be set to |false| after
  // StopDucking().
  bool is_ducking_;

  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  double ducking_volume_multiplier_;

  // True if the WebContents associated with this MediaSessionImpl is focused.
  bool focused_ = false;

  bool is_muted_ = false;

  // Used to persist audio device selection between navigations on the same
  // origin.
  url::Origin origin_;
  std::optional<std::string> audio_device_id_for_origin_;

  class PageData : public content::PageUserData<PageData> {
   public:
    explicit PageData(content::Page& page);

    PageData(const PageData&) = delete;
    PageData& operator=(const PageData&) = delete;

    ~PageData() override;

    void AddImageCache(const GURL& image_url, const SkBitmap& bitmap) {
      image_cache_.emplace(image_url, bitmap);
    }

    const SkBitmap* GetImageCache(const GURL& image_url) const {
      auto it = image_cache_.find(image_url);
      if (it == image_cache_.end())
        return nullptr;

      return &it->second;
    }

    PAGE_USER_DATA_KEY_DECL();

   private:
    // Cache of images that have been requested by clients.
    base::flat_map<GURL, SkBitmap> image_cache_;
  };

  // Returns the PageData for the specified |page|.
  PageData& GetPageData(content::Page& page) const;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MediaSessionAndroid> session_android_;
#endif  // BUILDFLAG(IS_ANDROID)

  // MediaSessionService-related fields
  using ServicesMap =
      std::map<GlobalRenderFrameHostId,
               raw_ptr<MediaSessionServiceImpl, CtnExperimental>>;

  // The current metadata and images associated with the current media session.
  media_session::MediaMetadata metadata_;
  base::flat_map<media_session::mojom::MediaSessionImageType,
                 std::vector<media_session::MediaImage>>
      images_;

  // The collection of all managed services (non-owned pointers). The services
  // are owned by RenderFrameHost and should be registered on creation and
  // unregistered on destroy.
  ServicesMap services_;
  // The currently routed service (non-owned pointer).
  raw_ptr<MediaSessionServiceImpl> routed_service_;

  // Bindings for Mojo pointers to |this| held by media route providers.
  mojo::ReceiverSet<media_session::mojom::MediaSession> receivers_;

  mojo::RemoteSet<media_session::mojom::MediaSessionObserver> observers_;

  base::RepeatingTimer duration_update_allowance_timer_;

  bool is_throttling_ = false;

  // This is guaranteed to be reset to |kDurationUpdateMaxAllowance| at
  // first update because |guarding_player_id_| is always a mismatch
  // at first, and will trigger a reset.
  int duration_update_allowance_ = 0;

  bool should_throttle_duration_update_ = false;

  // Whether the associated WebContents is connected to a presentation.
  bool has_presentation_ = false;

  std::optional<PlayerIdentifier> guarding_player_id_;

  media_session::mojom::RemotePlaybackMetadataPtr remote_playback_metadata_;

  // Used by tests to force media sessions to be ignored when finding a new
  // active session.
  bool always_ignore_for_active_session_for_testing_ = false;

  // True if the given media has infinite duration OR has a duration that
  // changes often enough to be considered live. See
  // `MaybeGuardDurationUpdate()` for details on duration changes.
  bool is_considered_live_ = false;

  base::WeakPtrFactory<MediaSessionImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_
