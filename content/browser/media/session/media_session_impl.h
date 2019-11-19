// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_

#include <stddef.h>

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_uma_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // defined(OS_ANDROID)

class MediaSessionImplBrowserTest;

namespace media {
enum class MediaContentType;
}  // namespace media

namespace media_session {
struct MediaMetadata;
}  // namespace media_session

namespace content {

class AudioFocusManagerTest;
class MediaSessionImplServiceRoutingTest;
class MediaSessionImplStateObserver;
class MediaSessionImplVisibilityBrowserTest;
class MediaSessionPlayerObserver;
class MediaSessionServiceImpl;
class MediaSessionServiceImplBrowserTest;

#if defined(OS_ANDROID)
class MediaSessionAndroid;
#endif  // defined(OS_ANDROID)

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
                         public WebContentsUserData<MediaSessionImpl> {
 public:
  enum class State { ACTIVE, SUSPENDED, INACTIVE };

  // Returns the MediaSessionImpl associated to this WebContents. Creates one if
  // none is currently available.
  CONTENT_EXPORT static MediaSessionImpl* Get(WebContents* web_contents);

  ~MediaSessionImpl() override;

#if defined(OS_ANDROID)
  static MediaSession* FromJavaMediaSession(
      const base::android::JavaRef<jobject>& j_media_session);
  MediaSessionAndroid* session_android() const {
    return session_android_.get();
  }
#endif  // defined(OS_ANDROID)

  void NotifyMediaSessionMetadataChange();

  // Adds the given player to the current media session. Returns whether the
  // player was successfully added. If it returns false, AddPlayer() should be
  // called again later.
  CONTENT_EXPORT bool AddPlayer(MediaSessionPlayerObserver* observer,
                                int player_id,
                                media::MediaContentType media_content_type);

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
  void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) override;

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

  // Seek the media session to a specific time.
  void SeekTo(base::TimeDelta seek_time) override;

  // Scrub ("fast seek") the media session to a specific time.
  void ScrubTo(base::TimeDelta seek_time) override;

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

  // Returns whether the action should be routed to |routed_service_|.
  bool ShouldRouteAction(media_session::mojom::MediaSessionAction action) const;

  // Returns the source ID which links media sessions on the same browser
  // context together.
  CONTENT_EXPORT const base::UnguessableToken& GetSourceId() const;

  // Returns the Audio Focus request ID associated with this media session.
  const base::UnguessableToken& GetRequestId() const;

 private:
  friend class content::WebContentsUserData<MediaSessionImpl>;
  friend class ::MediaSessionImplBrowserTest;
  friend class content::MediaSessionImplVisibilityBrowserTest;
  friend class content::AudioFocusManagerTest;
  friend class content::MediaSessionImplServiceRoutingTest;
  friend class content::MediaSessionImplStateObserver;
  friend class content::MediaSessionServiceImplBrowserTest;
  friend class MediaSessionImplTest;
  friend class MediaInternalsAudioFocusTest;

  CONTENT_EXPORT void SetDelegateForTests(
      std::unique_ptr<AudioFocusDelegate> delegate);
  CONTENT_EXPORT void RemoveAllPlayersForTest();
  CONTENT_EXPORT MediaSessionUmaHelper* uma_helper_for_test();

  // Representation of a player for the MediaSessionImpl.
  struct PlayerIdentifier {
    PlayerIdentifier(MediaSessionPlayerObserver* observer, int player_id);
    PlayerIdentifier(const PlayerIdentifier&) = default;

    void operator=(const PlayerIdentifier&) = delete;
    bool operator==(const PlayerIdentifier& player_identifier) const;
    bool operator<(const PlayerIdentifier&) const;

    // Hash operator for std::unordered_map<>.
    struct Hash {
      size_t operator()(const PlayerIdentifier& player_identifier) const;
    };

    MediaSessionPlayerObserver* observer;
    int player_id;
  };
  using PlayersMap =
      std::unordered_set<PlayerIdentifier, PlayerIdentifier::Hash>;
  using StateChangedCallback = base::Callback<void(State)>;

  CONTENT_EXPORT explicit MediaSessionImpl(WebContents* web_contents);

  void Initialize();

  // Called when we have finished downloading an image.
  void OnImageDownloadComplete(GetMediaImageBitmapCallback callback,
                               int minimum_size_px,
                               int desired_size_px,
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

  // Called when a MediaSessionAction is received. The action will be forwarded
  // to blink::MediaSession corresponding to the current routed service.
  void DidReceiveAction(media_session::mojom::MediaSessionAction action,
                        blink::mojom::MediaSessionActionDetailsPtr details);

  // A set of actions supported by |routed_service_| and the current media
  // session.
  std::set<media_session::mojom::MediaSessionAction> actions_;

  std::unique_ptr<AudioFocusDelegate> delegate_;
  std::map<PlayerIdentifier, media_session::mojom::AudioFocusType>
      normal_players_;
  PlayersMap pepper_players_;

  // Players that are playing in the web contents but we cannot control (e.g.
  // WebAudio or MediaStream).
  PlayersMap one_shot_players_;

  State audio_focus_state_ = State::INACTIVE;
  MediaSession::SuspendType suspend_type_;

  // The |desired_audio_focus_type_| is the AudioFocusType we will request when
  // we request system audio focus.
  media_session::mojom::AudioFocusType desired_audio_focus_type_;

  // The last updated |MediaSessionInfo| that was sent to |observers_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // The last updated |MediaPosition| that was sent to |observers_|.
  base::Optional<media_session::MediaPosition> position_;

  MediaSessionUmaHelper uma_helper_;

  // The ducking state of this media session. The initial value is |false|, and
  // is set to |true| after StartDucking(), and will be set to |false| after
  // StopDucking().
  bool is_ducking_;

  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  double ducking_volume_multiplier_;

  // True if the WebContents associated with this MediaSessionImpl is focused.
  bool focused_ = false;

#if defined(OS_ANDROID)
  std::unique_ptr<MediaSessionAndroid> session_android_;
#endif  // defined(OS_ANDROID)

  // MediaSessionService-related fields
  using ServicesMap = std::map<RenderFrameHost*, MediaSessionServiceImpl*>;

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
  MediaSessionServiceImpl* routed_service_;

  // Bindings for Mojo pointers to |this| held by media route providers.
  mojo::ReceiverSet<media_session::mojom::MediaSession> receivers_;

  mojo::RemoteSet<media_session::mojom::MediaSessionObserver> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_IMPL_H_
