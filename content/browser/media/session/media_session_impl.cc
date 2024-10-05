// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_players_callback_aggregator.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/gfx/favicon_size.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/media/session/media_session_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "content/public/common/content_features.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

using blink::mojom::MediaSessionPlaybackState;
using media_session::mojom::MediaAudioVideoState;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionImageType;
using media_session::mojom::MediaSessionInfo;

namespace {

const double kUnduckedVolumeMultiplier = 1.0;
const double kDefaultDuckingVolumeMultiplier = 0.2;

const char kDebugInfoOwnerSeparator[] = " - ";

using MapRenderFrameHostToDepth = std::map<RenderFrameHost*, size_t>;

using media_session::mojom::AudioFocusType;

using MediaSessionSuspendedSource =
    MediaSessionUmaHelper::MediaSessionSuspendedSource;

const char kMediaSessionDataName[] = "MediaSessionDataName";

class MediaSessionData : public base::SupportsUserData::Data {
 public:
  MediaSessionData() = default;

  MediaSessionData(const MediaSessionData&) = delete;
  MediaSessionData& operator=(const MediaSessionData&) = delete;

  static MediaSessionData* GetOrCreate(BrowserContext* context) {
    auto* data = static_cast<MediaSessionData*>(
        context->GetUserData(kMediaSessionDataName));

    if (!data) {
      auto new_data = std::make_unique<MediaSessionData>();
      data = new_data.get();
      context->SetUserData(kMediaSessionDataName, std::move(new_data));
    }

    return data;
  }

  const base::UnguessableToken& source_id() const { return source_id_; }

 private:
  base::UnguessableToken source_id_ = base::UnguessableToken::Create();
};

size_t ComputeFrameDepth(RenderFrameHost* rfh,
                         MapRenderFrameHostToDepth* map_rfh_to_depth) {
  DCHECK(rfh);
  size_t depth = 0;
  RenderFrameHost* current_frame = rfh;
  while (current_frame) {
    auto it = map_rfh_to_depth->find(current_frame);
    if (it != map_rfh_to_depth->end()) {
      depth += it->second;
      break;
    }
    ++depth;
    current_frame = current_frame->GetParentOrOuterDocument();
  }
  (*map_rfh_to_depth)[rfh] = depth;
  return depth;
}

// If the string is not empty then push it to the back of a vector.
void MaybePushBackString(std::vector<std::string>& vector,
                         const std::string& str) {
  if (!str.empty())
    vector.push_back(str);
}

bool IsSizeAtLeast(const gfx::Size& size, int min_size) {
  return size.width() >= min_size || size.height() >= min_size;
}

bool IsSizesAtLeast(const std::vector<gfx::Size>& sizes, int min_size) {
  // If we haven't found an image based on size then we should check if there
  // are any images that have no size data or have an "any" size which is
  // denoted by a single empty gfx::Size value.
  if (sizes.size() == 0 || (sizes.size() == 1 && sizes[0].IsEmpty()))
    return true;

  bool check_size = false;
  for (auto& size : sizes)
    check_size = check_size || IsSizeAtLeast(size, min_size);
  return check_size;
}

std::u16string SanitizeMediaTitle(const std::u16string& title) {
  std::u16string out;
  base::TrimString(title, u" ", &out);
  return out;
}

}  // anonymous namespace

constexpr int MediaSessionImpl::kDurationUpdateMaxAllowance;
constexpr base::TimeDelta
    MediaSessionImpl::kDurationUpdateAllowanceIncreaseInterval;

MediaSessionImpl::PlayerIdentifier::PlayerIdentifier(
    MediaSessionPlayerObserver* observer,
    int player_id)
    : observer(observer), player_id(player_id) {}

bool MediaSessionImpl::PlayerIdentifier::operator==(
    const PlayerIdentifier& other) const {
  return this->observer == other.observer && this->player_id == other.player_id;
}

bool MediaSessionImpl::PlayerIdentifier::operator!=(
    const PlayerIdentifier& other) const {
  return this->observer != other.observer || this->player_id != other.player_id;
}

bool MediaSessionImpl::PlayerIdentifier::operator<(
    const PlayerIdentifier& other) const {
  return observer != other.observer ? observer < other.observer
                                    : player_id < other.player_id;
}

// static
MediaSession* MediaSession::Get(WebContents* web_contents) {
  return MediaSessionImpl::Get(web_contents);
}

// static
MediaSession* MediaSession::GetIfExists(WebContents* contents) {
  return MediaSessionImpl::FromWebContents(contents);
}

// static
const base::UnguessableToken& MediaSession::GetSourceId(
    BrowserContext* browser_context) {
  return MediaSessionData::GetOrCreate(browser_context)->source_id();
}

// static
WebContents* MediaSession::GetWebContentsFromRequestId(
    const base::UnguessableToken& request_id) {
  DCHECK_NE(base::UnguessableToken::Null(), request_id);
  for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
    MediaSessionImpl* session = MediaSessionImpl::FromWebContents(web_contents);
    if (!session)
      continue;
    if (session->GetRequestId() == request_id)
      return web_contents;
  }
  return nullptr;
}

// static
WebContents* MediaSession::GetWebContentsFromRequestId(
    const std::string& request_id) {
  for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
    MediaSessionImpl* session = MediaSessionImpl::FromWebContents(web_contents);
    if (!session)
      continue;
    if (session->GetRequestId().ToString() == request_id)
      return web_contents;
  }
  return nullptr;
}

// static
const base::UnguessableToken& MediaSession::GetRequestIdFromWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  MediaSessionImpl* session = MediaSessionImpl::FromWebContents(web_contents);
  return session ? session->GetRequestId() : base::UnguessableToken::Null();
}

// static
void MediaSession::FlushObserversForTesting(WebContents* web_contents) {
  DCHECK(web_contents);
  MediaSessionImpl* session = MediaSessionImpl::FromWebContents(web_contents);
  session->flush_observers_for_testing();  // IN-TEST
}

// static
MediaSessionImpl* MediaSessionImpl::Get(WebContents* web_contents) {
  MediaSessionImpl* session = FromWebContents(web_contents);
  if (!session) {
    CreateForWebContents(web_contents);
    session = FromWebContents(web_contents);
    session->Initialize();
    static_cast<WebContentsImpl*>(web_contents)->MediaSessionCreated(session);
  }
  return session;
}

MediaSessionImpl::~MediaSessionImpl() {
  DCHECK(normal_players_.empty());
  DCHECK(pepper_players_.empty());
  DCHECK(one_shot_players_.empty());
  DCHECK(audio_focus_state_ == State::INACTIVE);
}

#if BUILDFLAG(IS_ANDROID)
void MediaSessionImpl::ClearMediaSessionAndroid() {
  session_android_.reset();
}

MediaSessionAndroid* MediaSessionImpl::GetMediaSessionAndroid() {
  return session_android_.get();
}
#endif

void MediaSessionImpl::WebContentsDestroyed() {
  delegate_->ReleaseRequestId();

  // This should only work for tests. In production, all the players should have
  // already been removed before WebContents is destroyed.

  // TODO(zqzhang): refactor MediaSessionImpl, maybe move the interface used to
  // talk with AudioFocusManager out to a seperate class. The AudioFocusManager
  // unit tests then could mock the interface and abandon audio focus when
  // WebContents is destroyed. See https://crbug.com/651069
  normal_players_.clear();
  pepper_players_.clear();
  one_shot_players_.clear();

  AbandonSystemAudioFocusIfNeeded();

  GetContentClient()->browser()->RemovePresentationObserver(this,
                                                            web_contents());
}

void MediaSessionImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  const auto rfh_id = rfh->GetGlobalId();
  if (services_.count(rfh_id))
    OnServiceDestroyed(services_[rfh_id]);
}

void MediaSessionImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  auto new_origin = url::Origin::Create(navigation_handle->GetURL());
  if (navigation_handle->IsInPrimaryMainFrame() &&
      !new_origin.IsSameOriginWith(origin_)) {
    audio_device_id_for_origin_.reset();
    origin_ = new_origin;
  }

  const auto rfh_id = navigation_handle->GetRenderFrameHost()->GetGlobalId();
  if (services_.count(rfh_id))
    services_[rfh_id]->DidFinishNavigation();

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::OnWebContentsFocused(RenderWidgetHost*) {
  focused_ = true;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  // If we have just gained focus and we have audio focus we should re-request
  // system audio focus. This will ensure this media session is towards the top
  // of the stack if we have multiple sessions active at the same time.
  if (audio_focus_state_ == State::ACTIVE)
    RequestSystemAudioFocus(desired_audio_focus_type_);
#endif
}

void MediaSessionImpl::OnWebContentsLostFocus(RenderWidgetHost*) {
  focused_ = false;
}

void MediaSessionImpl::TitleWasSet(NavigationEntry* entry) {
  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::DidUpdateFaviconURL(
    RenderFrameHost* rfh,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  std::vector<media_session::MediaImage> icons;

  for (auto& icon : candidates) {
    // We only want either favicons or the touch icons. There is another type of
    // touch icon which is "precomposed". This means it might have rounded
    // corners, etc. but it is not predictable so we cannot show them in the UI.
    if (icon->icon_type != blink::mojom::FaviconIconType::kFavicon &&
        icon->icon_type != blink::mojom::FaviconIconType::kTouchIcon) {
      continue;
    }

    std::vector<gfx::Size> sizes = icon->icon_sizes;

    // If we are a favicon and we do not have a size then we should assume the
    // default size for favicons.
    if (icon->icon_type == blink::mojom::FaviconIconType::kFavicon &&
        sizes.empty())
      sizes.push_back(gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));

    if (sizes.empty() || !icon->icon_url.is_valid())
      continue;

    media_session::MediaImage image;
    image.src = icon->icon_url;
    image.sizes = sizes;
    icons.push_back(image);
  }

  auto it = images_.find(MediaSessionImageType::kSourceIcon);
  if (it != images_.end() && it->second == icons)
    return;

  images_.insert_or_assign(MediaSessionImageType::kSourceIcon, icons);

  for (auto& observer : observers_)
    observer->MediaSessionImagesChanged(this->images_);
}

void MediaSessionImpl::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::RenderFrameHostStateChanged(
    RenderFrameHost* host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  // If the page goes to back-forward cache, hide the players.
  if (new_state == RenderFrameHost::LifecycleState::kInBackForwardCache) {
    // Checking the normal players is enough. One shot players and pepper
    // players are not related to media control UIs.
    auto players = normal_players_;
    for (auto player : players) {
      if (player.first.observer->render_frame_host() != host) {
        continue;
      }
      // RemovePlayer removes the player from not only |normal_players_| but
      // also |hidden_players_|. Call RemovePlayer first.
      RemovePlayer(player.first.observer, player.first.player_id);
      hidden_players_.insert(player.first);
    }
    return;
  }

  // If the page is restored from back-forward cache, show the players.
  if (new_state == RenderFrameHost::LifecycleState::kActive) {
    auto players = hidden_players_;
    bool added_players = false;
    for (auto player : players) {
      if (player.observer->render_frame_host() != host)
        continue;
      hidden_players_.erase(player);
      AddPlayer(player.observer, player.player_id);
      added_players = true;
    }

    // Just after adding a player, the state might be 'play'. Make sure that the
    // state is 'pause'.
    if (added_players)
      OnSuspendInternal(SuspendType::kSystem, State::SUSPENDED);

    return;
  }
}

bool MediaSessionImpl::AddPlayer(MediaSessionPlayerObserver* observer,
                                 int player_id) {
  media::MediaContentType media_content_type = observer->GetMediaContentType();

  if (media_content_type == media::MediaContentType::kOneShot)
    return AddOneShotPlayer(observer, player_id);
  if (media_content_type == media::MediaContentType::kPepper)
    return AddPepperPlayer(observer, player_id);

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());
  if (audio_device_id_for_origin_)
    observer->OnSetAudioSinkId(player_id, audio_device_id_for_origin_.value());

  AudioFocusType required_audio_focus_type;
  if (media_content_type == media::MediaContentType::kPersistent)
    required_audio_focus_type = AudioFocusType::kGain;
  else
    required_audio_focus_type = AudioFocusType::kGainTransientMayDuck;

  PlayerIdentifier key(observer, player_id);

  // If the audio focus is already granted and is of type Content, there is
  // nothing to do. If it is granted of type Transient the requested type is
  // also transient, there is also nothing to do. Otherwise, the session needs
  // to request audio focus again.
  if (audio_focus_state_ == State::ACTIVE) {
    std::optional<AudioFocusType> current_focus_type =
        delegate_->GetCurrentFocusType();
    if (current_focus_type == AudioFocusType::kGain ||
        current_focus_type == required_audio_focus_type) {
      auto iter = normal_players_.find(key);
      if (iter == normal_players_.end())
        normal_players_.emplace(std::move(key), required_audio_focus_type);
      else
        iter->second = required_audio_focus_type;

      UpdateRoutedService();
      RebuildAndNotifyMediaSessionInfoChanged();
      RebuildAndNotifyActionsChanged();
      RebuildAndNotifyMediaPositionChanged();
      return true;
    }
  }

  // If this player is paused, then don't actually request audio focus on its
  // behalf.  Otherwise, we might take focus away from something else, even
  // though we don't really need it right now.  Note that we don't abandon focus
  // when playback is paused; we will continue to hold it.  However, if we have
  // given it up, e.g. when playback is suspended, or lost it to some other
  // request, then we don't want to take it back for a paused player.
  // Otherwise, any random update to the player (e.g., metadata as in
  // b/40946745) will re-request focus even while paused.
  if (!observer->IsPaused(player_id)) {
    State old_audio_focus_state = audio_focus_state_;
    RequestSystemAudioFocus(required_audio_focus_type);

    if (audio_focus_state_ != State::ACTIVE) {
      return false;
    }

    // The session should be reset if a player is starting while all players
    // are suspended.
    if (old_audio_focus_state != State::ACTIVE) {
      normal_players_.clear();
    }
  } else if (audio_focus_state_ == State::INACTIVE) {
    // We switch from `INACTIVE` to `SUSPENDED` to indicate that we want to have
    // the focus, but don't right now.  This makes the session controllable.
    audio_focus_state_ = State::SUSPENDED;
  }

  auto iter = normal_players_.find(key);
  if (iter == normal_players_.end())
    normal_players_.emplace(std::move(key), required_audio_focus_type);
  else
    iter->second = required_audio_focus_type;

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();

  return true;
}

void MediaSessionImpl::RemovePlayer(MediaSessionPlayerObserver* observer,
                                    int player_id) {
  const PlayerIdentifier identifier(observer, player_id);
  normal_players_.erase(identifier);
  pepper_players_.erase(identifier);
  one_shot_players_.erase(identifier);
  hidden_players_.erase(identifier);

  if (guarding_player_id_ && *guarding_player_id_ == identifier)
    ResetDurationUpdateGuard();

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::RemovePlayers(MediaSessionPlayerObserver* observer) {
  for (auto it = normal_players_.begin(); it != normal_players_.end();) {
    if (it->first.observer == observer)
      normal_players_.erase(it++);
    else
      ++it;
  }

  for (auto it = pepper_players_.begin(); it != pepper_players_.end();) {
    if (it->observer == observer)
      pepper_players_.erase(it++);
    else
      ++it;
  }

  for (auto it = one_shot_players_.begin(); it != one_shot_players_.end();) {
    if (it->observer == observer)
      one_shot_players_.erase(it++);
    else
      ++it;
  }

  if (guarding_player_id_ && guarding_player_id_->observer == observer)
    ResetDurationUpdateGuard();

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::RecordSessionDuck() {
  uma_helper_.RecordSessionSuspended(
      MediaSessionSuspendedSource::kSystemTransientDuck);
}

void MediaSessionImpl::OnPlayerPaused(MediaSessionPlayerObserver* observer,
                                      int player_id) {
  // If a playback is completed, BrowserMediaPlayerManager will call
  // OnPlayerPaused() after RemovePlayer(). This is a workaround.
  // Also, this method may be called when a player that is not added
  // to this session (e.g. a silent video) is paused. MediaSessionImpl
  // should ignore the paused player for this case.
  PlayerIdentifier identifier(observer, player_id);
  if (!normal_players_.count(identifier) &&
      !pepper_players_.count(identifier) &&
      !one_shot_players_.count(identifier)) {
    return;
  }

  // If the player to be removed is a pepper player, or there is more than one
  // observer, remove the paused one from the session.
  if (pepper_players_.count(identifier) || normal_players_.size() != 1) {
    RemovePlayer(observer, player_id);
    return;
  }

  // If the player is a one-shot player, just remove it since it is not expected
  // to resume a one-shot player via resuming MediaSession.
  if (one_shot_players_.count(identifier)) {
    RemovePlayer(observer, player_id);
    return;
  }

  // Otherwise, suspend the session.
  // The session might not have audio focus if it was paused prior to being
  // suspended, which is fine.
  OnSuspendInternal(SuspendType::kContent, State::SUSPENDED);
}

void MediaSessionImpl::RebuildAndNotifyMediaPositionChanged() {
  std::optional<media_session::MediaPosition> position;

  // If there was a position specified from Blink then we should use that.
  if (routed_service_ && routed_service_->position()) {
    position = routed_service_->position();

    // We do not throttle updates from media session API because there's
    // no effective way to disdinguish updates from single player or
    // different players.
    ResetDurationUpdateGuard();
  }

  // If we only have a single player then we should use the position from that.
  if (!position && normal_players_.size() == 1 && one_shot_players_.empty() &&
      pepper_players_.empty()) {
    auto& first = normal_players_.begin()->first;
    position = first.observer->GetPosition(first.player_id);

    if (should_throttle_duration_update_) {
      if (!guarding_player_id_ || *guarding_player_id_ != first) {
        ResetDurationUpdateGuard();
        guarding_player_id_ = first;
      }

      position = MaybeGuardDurationUpdate(position);
    }
  }

  if (position == position_)
    return;

  position_ = position;

  if (auto* pip_window_controller_ =
          VideoPictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->MediaSessionPositionChanged(position_);
  }

  for (auto& observer : observers_)
    observer->MediaSessionPositionChanged(position_);

  const bool is_considered_live =
      position_.has_value() && position_->duration().is_max();
  if (is_considered_live == is_considered_live_) {
    return;
  }

  // The available actions can be different depending on whether we're
  // considered live or not, so if that has changed we must re-notify for the
  // new state.
  is_considered_live_ = is_considered_live;
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::Resume(SuspendType suspend_type) {
  // If the site has registered an action handler for play, we should pass it to
  // the site and let them handle it.
  if (suspend_type == SuspendType::kUI &&
      ShouldRouteAction(media_session::mojom::MediaSessionAction::kPlay)) {
    DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay);
    return;
  }

  // When the resume requests comes from another source than system, audio focus
  // must be requested.
  if (suspend_type != SuspendType::kSystem) {
    // Request audio focus again in case we lost it because another app started
    // playing while the playback was paused. If the audio focus request is
    // delayed we will resume the player when the request completes.
    AudioFocusDelegate::AudioFocusResult result =
        RequestSystemAudioFocus(desired_audio_focus_type_);

    SetAudioFocusState(result != AudioFocusDelegate::AudioFocusResult::kFailed
                           ? State::ACTIVE
                           : State::INACTIVE);

    if (audio_focus_state_ != State::ACTIVE)
      return;
  } else {
    // System resume implies that we have the focus and should start playing if
    // the system was what suspended us.  Otherwise, we're suspended.
    SetAudioFocusState((suspend_type_ == SuspendType::kSystem)
                           ? State::ACTIVE
                           : State::SUSPENDED);
  }

  OnResumeInternal(suspend_type);
}

void MediaSessionImpl::Suspend(SuspendType suspend_type) {
  if (!IsActive())
    return;

  if (suspend_type == SuspendType::kUI) {
    // If the site has registered an action handler for pause then we should
    // pass it to the site and let them handle it.
    if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kPause)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause);
      return;
    }
  }

  OnSuspendInternal(suspend_type, State::SUSPENDED);
}

void MediaSessionImpl::Stop(SuspendType suspend_type) {
  DCHECK(audio_focus_state_ != State::INACTIVE);
  DCHECK(suspend_type != SuspendType::kContent);
  DCHECK(!HasPepper());

  if (suspend_type == SuspendType::kUI) {
    // If the site has registered an action handle for stop then we should
    // notify the site but continue stopping the media session.
    if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kStop)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kStop);
    }
  }

  if (auto* pip_window_controller_ =
          VideoPictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->Close(false /* should_pause_video */);
  }

  // TODO(mlamouri): merge the logic between UI and SYSTEM.
  if (suspend_type == SuspendType::kSystem) {
    OnSuspendInternal(suspend_type, State::INACTIVE);
    return;
  }

  if (audio_focus_state_ != State::SUSPENDED)
    OnSuspendInternal(suspend_type, State::SUSPENDED);

  DCHECK(audio_focus_state_ == State::SUSPENDED);
  normal_players_.clear();

  AbandonSystemAudioFocusIfNeeded();
  RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionImpl::Seek(base::TimeDelta seek_time) {
  DCHECK(!seek_time.is_zero());

  if (seek_time.is_positive()) {
    // If the site has registered an action handler for seek forward then we
    // should pass it to the site and let them handle it.
    if (ShouldRouteAction(
            media_session::mojom::MediaSessionAction::kSeekForward)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekForward);
      return;
    }

    for (const auto& it : normal_players_)
      it.first.observer->OnSeekForward(it.first.player_id, seek_time);
  } else if (seek_time.is_negative()) {
    // If the site has registered an action handler for seek backward then we
    // should pass it to the site and let them handle it.
    if (ShouldRouteAction(
            media_session::mojom::MediaSessionAction::kSeekBackward)) {
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekBackward);
      return;
    }

    for (const auto& it : normal_players_)
      it.first.observer->OnSeekBackward(it.first.player_id, seek_time * -1);
  }
}

bool MediaSessionImpl::IsControllable() const {
  if (audio_focus_state_ == State::INACTIVE || HasOnlyOneShotPlayers())
    return false;

#if !BUILDFLAG(IS_ANDROID)
  if (routed_service_ && routed_service_->playback_state() !=
                             blink::mojom::MediaSessionPlaybackState::NONE) {
    return true;
  }
#endif

  return desired_audio_focus_type_ == AudioFocusType::kGain;
}

void MediaSessionImpl::SetDuckingVolumeMultiplier(double multiplier) {
  ducking_volume_multiplier_ = std::clamp(multiplier, 0.0, 1.0);
}

void MediaSessionImpl::SetAudioFocusGroupId(
    const base::UnguessableToken& group_id) {
  audio_focus_group_id_ = group_id;
}

void MediaSessionImpl::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  UpdateVolumeMultiplier();
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  UpdateVolumeMultiplier();
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::UpdateVolumeMultiplier() {
  for (const auto& it : normal_players_) {
    it.first.observer->OnSetVolumeMultiplier(it.first.player_id,
                                             GetVolumeMultiplier());
  }

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());
}

double MediaSessionImpl::GetVolumeMultiplier() const {
  return is_ducking_ ? ducking_volume_multiplier_ : kUnduckedVolumeMultiplier;
}

bool MediaSessionImpl::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

bool MediaSessionImpl::IsSuspended() const {
  return audio_focus_state_ == State::SUSPENDED;
}

bool MediaSessionImpl::HasPepper() const {
  return !pepper_players_.empty();
}

bool MediaSessionImpl::HasOnlyOneShotPlayers() const {
  return !one_shot_players_.empty() && normal_players_.empty() &&
         pepper_players_.empty();
}

void MediaSessionImpl::SetDelegateForTests(
    std::unique_ptr<AudioFocusDelegate> delegate) {
  delegate_ = std::move(delegate);
}

MediaSessionUmaHelper* MediaSessionImpl::uma_helper_for_test() {
  return &uma_helper_;
}

void MediaSessionImpl::RemoveAllPlayersForTest() {
  normal_players_.clear();
  pepper_players_.clear();
  one_shot_players_.clear();
  AbandonSystemAudioFocusIfNeeded();
}

void MediaSessionImpl::OnImageDownloadComplete(
    GetMediaImageBitmapCallback callback,
    int minimum_size_px,
    int desired_size_px,
    bool source_icon,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  DCHECK(bitmaps.size() == sizes.size());
  SkBitmap image;
  double best_image_score = 0.0;

  // Rank |sizes| and |bitmaps| using MediaImageManager.
  for (size_t i = 0; i < bitmaps.size(); i++) {
    double image_score = media_session::MediaImageManager::GetImageSizeScore(
        minimum_size_px, desired_size_px, sizes.at(i));

    if (image_score > best_image_score)
      image = bitmaps.at(i);
  }

  // If the image is the wrong color type then we should convert it.
  SkBitmap bitmap;
  if (!image.isNull()) {
    if (image.colorType() == kRGBA_8888_SkColorType) {
      bitmap = image;
    } else {
      SkImageInfo info = image.info().makeColorType(kRGBA_8888_SkColorType);
      if (bitmap.tryAllocPixels(info))
        image.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
    }
  }

  if (source_icon) {
    GetPageData(web_contents()->GetPrimaryPage())
        .AddImageCache(image_url, bitmap);
  }

  std::move(callback).Run(bitmap);
}

void MediaSessionImpl::OnSystemAudioFocusRequested(bool result) {
  if (result)
    StopDucking();
}

void MediaSessionImpl::OnSuspendInternal(SuspendType suspend_type,
                                         State new_state) {
  DCHECK(!HasPepper());

  DCHECK(new_state == State::SUSPENDED || new_state == State::INACTIVE);
  // UI suspend cannot use State::INACTIVE.
  DCHECK(suspend_type == SuspendType::kSystem || new_state == State::SUSPENDED);

  if (HasOnlyOneShotPlayers())
    return;

  if (audio_focus_state_ != State::ACTIVE)
    return;

  switch (suspend_type) {
    case SuspendType::kUI:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::kUI);
      break;
    case SuspendType::kSystem:
      switch (new_state) {
        case State::SUSPENDED:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::kSystemTransient);
          break;
        case State::INACTIVE:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::kSystemPermanent);
          break;
        case State::ACTIVE:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
    case SuspendType::kContent:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::kCONTENT);
      break;
  }

  SetAudioFocusState(new_state);
  suspend_type_ = suspend_type;

  if (suspend_type != SuspendType::kContent) {
    // SuspendType::CONTENT happens when the suspend action came from
    // the page in which case the player is already paused.
    // Otherwise, the players need to be paused.
    for (const auto& it : normal_players_)
      it.first.observer->OnSuspend(it.first.player_id);
  }

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id,
                                       ducking_volume_multiplier_);

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::OnResumeInternal(SuspendType suspend_type) {
  if (suspend_type == SuspendType::kSystem && suspend_type_ != suspend_type)
    return;

  for (const auto& it : normal_players_)
    it.first.observer->OnResume(it.first.player_id);

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());

  RebuildAndNotifyMediaSessionInfoChanged();
}

MediaSessionImpl::MediaSessionImpl(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<MediaSessionImpl>(*web_contents),
      audio_focus_state_(State::INACTIVE),
      desired_audio_focus_type_(AudioFocusType::kGainTransientMayDuck),
      is_ducking_(false),
      ducking_volume_multiplier_(kDefaultDuckingVolumeMultiplier),
      routed_service_(nullptr) {
#if BUILDFLAG(IS_ANDROID)
  session_android_ = std::make_unique<MediaSessionAndroid>(this);
  should_throttle_duration_update_ = true;
#endif  // BUILDFLAG(IS_ANDROID)
  if (web_contents && web_contents->GetPrimaryMainFrame() &&
      web_contents->GetPrimaryMainFrame()->GetView()) {
    focused_ = web_contents->GetPrimaryMainFrame()->GetView()->HasFocus();
  }

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::Initialize() {
  delegate_ = AudioFocusDelegate::Create(this);
  delegate_->MediaSessionInfoChanged(GetMediaSessionInfoSync());

  DCHECK(web_contents());
  DidUpdateFaviconURL(web_contents()->GetPrimaryMainFrame(),
                      web_contents()->GetFaviconURLs());

  GetContentClient()->browser()->AddPresentationObserver(this, web_contents());
}

void MediaSessionImpl::OnPresentationsChanged(bool has_presentation) {
  has_presentation_ = has_presentation;
  RebuildAndNotifyMediaSessionInfoChanged();
}

AudioFocusDelegate::AudioFocusResult MediaSessionImpl::RequestSystemAudioFocus(
    AudioFocusType audio_focus_type) {
  // |kGainTransient| is not used in MediaSessionImpl.
  DCHECK_NE(media_session::mojom::AudioFocusType::kGainTransient,
            audio_focus_type);

  AudioFocusDelegate::AudioFocusResult result =
      delegate_->RequestAudioFocus(audio_focus_type);
  desired_audio_focus_type_ = audio_focus_type;

  bool success = result != AudioFocusDelegate::AudioFocusResult::kFailed;
  SetAudioFocusState(success ? State::ACTIVE : State::INACTIVE);

  // If we are delayed then we should return now and wait for the response from
  // the audio focus delegate.
  if (result == AudioFocusDelegate::AudioFocusResult::kDelayed)
    return result;

  OnSystemAudioFocusRequested(success);
  return result;
}

mojo::PendingRemote<media_session::mojom::MediaSession>
MediaSessionImpl::AddRemote() {
  mojo::PendingRemote<media_session::mojom::MediaSession> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void MediaSessionImpl::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());

  // Add the title and the url to the owner.
  std::vector<std::string> owner_parts;
  MaybePushBackString(owner_parts,
                      base::UTF16ToUTF8(web_contents()->GetTitle()));
  MaybePushBackString(owner_parts,
                      web_contents()->GetLastCommittedURL().spec());
  info->owner = base::JoinString(owner_parts, kDebugInfoOwnerSeparator);

  std::move(callback).Run(std::move(info));
}

media_session::mojom::MediaSessionInfoPtr
MediaSessionImpl::GetMediaSessionInfoSync() {
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());

  switch (audio_focus_state_) {
    case State::ACTIVE:
      info->state = MediaSessionInfo::SessionState::kActive;
      break;
    case State::SUSPENDED:
      info->state = MediaSessionInfo::SessionState::kSuspended;
      break;
    case State::INACTIVE:
      info->state = MediaSessionInfo::SessionState::kInactive;
      break;
  }

  // The state should always be kDucked if we are ducked.
  if (is_ducking_)
    info->state = MediaSessionInfo::SessionState::kDucking;

  // If we have Pepper players then we should force ducking.
  info->force_duck = HasPepper();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // If this is a webapp, and instanced media controls are on, mark this session
  // as a pwa session so that the browser sessions can stay isolated. This is
  // used to differentiate webapp sessions for different handling.
  auto* web_contents_delegate = web_contents()->GetDelegate();
  info->ignore_for_active_session =
      base::FeatureList::IsEnabled(features::kWebAppSystemMediaControls) &&
      web_contents_delegate &&
      web_contents_delegate->ShouldUseInstancedSystemMediaControls();
#else
  info->ignore_for_active_session = false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  if (always_ignore_for_active_session_for_testing_) {
    info->ignore_for_active_session = true;
  }

  // The playback state should use |IsActive| to determine whether we are
  // playing or not. However, if there is a |routed_service_| which is playing
  // then we should force the playback state to be playing.
  info->playback_state =
      IsActive() ? MediaPlaybackState::kPlaying : MediaPlaybackState::kPaused;
  if (routed_service_ &&
      routed_service_->playback_state() == MediaSessionPlaybackState::PLAYING) {
    info->playback_state = MediaPlaybackState::kPlaying;
  }

  info->audio_video_states = GetMediaAudioVideoStates();
  info->is_controllable = IsControllable();

  // If the browser context is off the record then it should be sensitive.
  // This is used as a proxy to hide the metadata from sensitive surfaces such
  // as the lock screen.
  // TODO(crbug.com/40282278): Remove this field once the new feature to hide
  // metadata from sensitive profiles is launched.
  info->is_sensitive =
      web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      !base::FeatureList::IsEnabled(media::kHideIncognitoMediaMetadata);

  info->picture_in_picture_state =
      web_contents()->HasPictureInPictureVideo() ||
              web_contents()->HasPictureInPictureDocument()
          ? media_session::mojom::MediaPictureInPictureState::
                kInPictureInPicture
          : media_session::mojom::MediaPictureInPictureState::
                kNotInPictureInPicture;

  auto shared_audio_device_id = GetSharedAudioOutputDeviceId();
  // When the default audio device is in use, or this session's players are
  // using different devices, the |audio_sink_id| attribute should remain unset.
  if (shared_audio_device_id != media::AudioDeviceDescription::kDefaultDeviceId)
    info->audio_sink_id = shared_audio_device_id;

  if (routed_service_) {
    info->microphone_state = routed_service_->microphone_state();
    info->camera_state = routed_service_->camera_state();
  } else {
    info->microphone_state = media_session::mojom::MicrophoneState::kUnknown;
    info->camera_state = media_session::mojom::CameraState::kUnknown;
  }

  info->muted = is_muted_;
  info->has_presentation = has_presentation_;

  // Disable Remote Playback by passing empty RemotePlaybackMetadata when there
  // are multiple media players.
  info->remote_playback_metadata = remote_playback_metadata_.Clone();
  if (normal_players_.size() > 1u && info->remote_playback_metadata) {
    info->remote_playback_metadata->remote_playback_disabled = true;
  }

  MediaSessionClient* media_session_client = MediaSessionClient::Get();
  info->hide_metadata = media_session_client
                            ? media_session_client->ShouldHideMetadata(
                                  web_contents()->GetBrowserContext())
                            : false;

  info->meets_visibility_threshold = HasSufficientlyVisibleVideo();

  return info;
}

void MediaSessionImpl::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoSync());
}

void MediaSessionImpl::AddObserver(
    mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer) {
  mojo::Remote<media_session::mojom::MediaSessionObserver>
      media_session_observer(std::move(observer));
  media_session_observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());
  media_session_observer->MediaSessionMetadataChanged(metadata_);
  media_session_observer->MediaSessionImagesChanged(images_);
  media_session_observer->MediaSessionPositionChanged(position_);

  std::vector<media_session::mojom::MediaSessionAction> actions(
      actions_.begin(), actions_.end());
  media_session_observer->MediaSessionActionsChanged(actions);

  observers_.Add(std::move(media_session_observer));
}

void MediaSessionImpl::FinishSystemAudioFocusRequest(
    AudioFocusType audio_focus_type,
    bool result) {
  // If the media session is not active then we do not need to enforce the
  // result of the audio focus request.
  if (audio_focus_state_ != State::ACTIVE) {
    AbandonSystemAudioFocusIfNeeded();
    return;
  }

  OnSystemAudioFocusRequested(result);

  if (!result && !HasPepper()) {
    switch (audio_focus_type) {
      case AudioFocusType::kGain:
        // If the gain audio focus request failed then we should suspend the
        // media session.
        OnSuspendInternal(SuspendType::kSystem, State::SUSPENDED);
        break;
      case AudioFocusType::kAmbient:
      case AudioFocusType::kGainTransient:
        // MediaSessionImpl does not use |kGainTransient| or |kAmbient|.
        NOTREACHED_IN_MIGRATION();
        break;
      case AudioFocusType::kGainTransientMayDuck:
        // The focus request failed, we should suspend any players that have
        // the same audio focus type.
        for (auto& player : normal_players_) {
          if (audio_focus_type == player.second)
            player.first.observer->OnSuspend(player.first.player_id);
        }
        break;
    }
  }
}

void MediaSessionImpl::PreviousTrack() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kPreviousTrack);
}

void MediaSessionImpl::NextTrack() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kNextTrack);
}

void MediaSessionImpl::SkipAd() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kSkipAd);
}

void MediaSessionImpl::PreviousSlide() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kPreviousSlide);
}

void MediaSessionImpl::NextSlide() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kNextSlide);
}

void MediaSessionImpl::SeekTo(base::TimeDelta seek_time) {
  // If the site has registered an action handler for seek to then we
  // should pass it to the site and let them handle it.
  if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kSeekTo)) {
    DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekTo,
                     blink::mojom::MediaSessionActionDetails::NewSeekTo(
                         blink::mojom::MediaSessionSeekToDetails::New(
                             seek_time, /*fast_seek=*/false)));
    return;
  }

  for (const auto& it : normal_players_)
    it.first.observer->OnSeekTo(it.first.player_id, seek_time);
}

void MediaSessionImpl::ScrubTo(base::TimeDelta seek_time) {
  // If the site has registered an action handler for seek to then we
  // should pass it to the site and let them handle it.
  if (ShouldRouteAction(media_session::mojom::MediaSessionAction::kSeekTo)) {
    DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekTo,
                     blink::mojom::MediaSessionActionDetails::NewSeekTo(
                         blink::mojom::MediaSessionSeekToDetails::New(
                             seek_time, /*fast_seek=*/true)));
    return;
  }

  for (const auto& it : normal_players_)
    it.first.observer->OnSeekTo(it.first.player_id, seek_time);
}

void MediaSessionImpl::EnterPictureInPicture() {
  if (base::FeatureList::IsEnabled(
          blink::features::kMediaSessionEnterPictureInPicture) &&
      ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kEnterPictureInPicture)) {
    DidReceiveAction(
        media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
    uma_helper_.RecordEnterPictureInPicture(
        MediaSessionUmaHelper::EnterPictureInPictureType::kRegisteredManual);
    return;
  }

  // There should be one and only one player when we enter picture-in-picture.
  DCHECK_EQ(normal_players_.size(), 1u);
  normal_players_.begin()->first.observer->OnEnterPictureInPicture(
      normal_players_.begin()->first.player_id);
  uma_helper_.RecordEnterPictureInPicture(
      MediaSessionUmaHelper::EnterPictureInPictureType::kDefaultHandler);
}

void MediaSessionImpl::ExitPictureInPicture() {
  static_cast<WebContentsImpl*>(web_contents())->ExitPictureInPicture();
}

void MediaSessionImpl::EnterAutoPictureInPicture() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kMediaSessionEnterPictureInPicture)) {
    return;
  }
  if (!ShouldRouteAction(
          media_session::mojom::MediaSessionAction::kEnterPictureInPicture)) {
    return;
  }

  DidReceiveAction(
      media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
  uma_helper_.RecordEnterPictureInPicture(
      MediaSessionUmaHelper::EnterPictureInPictureType::kRegisteredAutomatic);
}

void MediaSessionImpl::SetAudioSinkId(const std::optional<std::string>& id) {
  audio_device_id_for_origin_ = id;

  for (const auto& it : normal_players_) {
    it.first.observer->OnSetAudioSinkId(
        it.first.player_id,
        id.value_or(media::AudioDeviceDescription::kDefaultDeviceId));
  }
}

void MediaSessionImpl::ToggleMicrophone() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kToggleMicrophone);
}

void MediaSessionImpl::ToggleCamera() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kToggleCamera);
}

void MediaSessionImpl::HangUp() {
  DidReceiveAction(media_session::mojom::MediaSessionAction::kHangUp);
}

void MediaSessionImpl::Raise() {
  content::WebContentsDelegate* delegate = web_contents()->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(web_contents());
}

void MediaSessionImpl::SetMute(bool mute) {
  DCHECK_EQ(normal_players_.size(), 1u);
  normal_players_.begin()->first.observer->OnSetMute(
      normal_players_.begin()->first.player_id, mute);
}

void MediaSessionImpl::RequestMediaRemoting() {
  DCHECK_EQ(normal_players_.size(), 1u);
  normal_players_.begin()->first.observer->OnRequestMediaRemoting(
      normal_players_.begin()->first.player_id);
}

void MediaSessionImpl::GetMediaImageBitmap(
    const media_session::MediaImage& image,
    int minimum_size_px,
    int desired_size_px,
    GetMediaImageBitmapCallback callback) {
// We want to hide the media image from ChromeOS' media controls.
#if BUILDFLAG(IS_CHROMEOS)
  if (session_info_ && session_info_->hide_metadata) {
    MediaSessionClient* media_session_client = MediaSessionClient::Get();
    CHECK(media_session_client);
    std::move(callback).Run(media_session_client->GetThumbnailPlaceholder());
    return;
  }
#endif

  // We should make sure `image` is in `images_`.
  bool found = false;
  bool source_icon = false;
  for (auto& image_type : images_) {
    if (base::Contains(image_type.second, image)) {
      found = true;

      if (image_type.first ==
          media_session::mojom::MediaSessionImageType::kSourceIcon) {
        source_icon = true;
      }
      break;
    }
  }

  // Or the `image` is in chapters.
  if (!found) {
    for (auto& chapter : metadata_.chapters) {
      if (base::Contains(chapter.artwork(), image)) {
        found = true;
        break;
      }
    }
  }

  if (!found || !IsSizesAtLeast(image.sizes, minimum_size_px)) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  // Check the cache.
  PageData& page_data = GetPageData(web_contents()->GetPrimaryPage());
  if (source_icon) {
    if (auto* bitmap = page_data.GetImageCache(image.src)) {
      std::move(callback).Run(*bitmap);
      return;
    }
  }

  const gfx::Size preferred_size(desired_size_px, desired_size_px);
  web_contents()->DownloadImage(
      image.src, false /* is_favicon */, preferred_size,
      desired_size_px /* max_bitmap_size */, false /* bypass_cache */,
      base::BindOnce(&MediaSessionImpl::OnImageDownloadComplete,
                     base::Unretained(this),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(callback), SkBitmap()),
                     minimum_size_px, desired_size_px, source_icon));
}

void MediaSessionImpl::AbandonSystemAudioFocusIfNeeded() {
  if (audio_focus_state_ == State::INACTIVE || !normal_players_.empty() ||
      !pepper_players_.empty() || !one_shot_players_.empty()) {
    return;
  }
  delegate_->AbandonAudioFocus();
  is_ducking_ = false;

  SetAudioFocusState(State::INACTIVE);
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::SetAudioFocusState(State audio_focus_state) {
  if (audio_focus_state == audio_focus_state_)
    return;

  audio_focus_state_ = audio_focus_state;
  switch (audio_focus_state_) {
    case State::ACTIVE:
      uma_helper_.OnSessionActive();
      break;
    case State::SUSPENDED:
      uma_helper_.OnSessionSuspended();
      break;
    case State::INACTIVE:
      uma_helper_.OnSessionInactive();
      break;
  }

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::FlushForTesting() {
  observers_.FlushForTesting();
}

void MediaSessionImpl::RebuildAndNotifyMediaSessionInfoChanged() {
  media_session::mojom::MediaSessionInfoPtr current_info =
      GetMediaSessionInfoSync();

  if (current_info == session_info_)
    return;

  // Picture-in-Picture window controller needs to be updated on current media
  // session info.
  if (auto* pip_window_controller_ =
          VideoPictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->MediaSessionInfoChanged(current_info);
  }

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(current_info.Clone());

  delegate_->MediaSessionInfoChanged(current_info);

  session_info_ = std::move(current_info);

#if BUILDFLAG(IS_CHROMEOS)
  // If we need to hide the metadata, then we need to notify the metadata
  // observers with the hidden metadata. They might have received the metadata
  // before the info has been updated.
  if (session_info_->hide_metadata) {
    RebuildAndNotifyMetadataChanged();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool MediaSessionImpl::AddPepperPlayer(MediaSessionPlayerObserver* observer,
                                       int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);

  if (result == AudioFocusDelegate::AudioFocusResult::kFailed)
    return false;

  pepper_players_.insert(PlayerIdentifier(observer, player_id));

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();

  return result != AudioFocusDelegate::AudioFocusResult::kFailed;
}

bool MediaSessionImpl::AddOneShotPlayer(MediaSessionPlayerObserver* observer,
                                        int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);

  if (result == AudioFocusDelegate::AudioFocusResult::kFailed)
    return false;

  one_shot_players_.insert(PlayerIdentifier(observer, player_id));

  UpdateRoutedService();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();

  return true;
}

// MediaSessionService-related methods

void MediaSessionImpl::OnServiceCreated(MediaSessionServiceImpl* service) {
  const auto rfh_id = service->GetRenderFrameHostId();

  services_[rfh_id] = service;
  UpdateRoutedService();
}

void MediaSessionImpl::OnServiceDestroyed(MediaSessionServiceImpl* service) {
  services_.erase(service->GetRenderFrameHostId());

  if (routed_service_ == service)
    UpdateRoutedService();
}

void MediaSessionImpl::OnMediaSessionPlaybackStateChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::OnMediaSessionMetadataChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyMetadataChanged();
}

void MediaSessionImpl::OnMediaSessionActionsChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::OnMediaSessionInfoChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::DidReceiveAction(
    media_session::mojom::MediaSessionAction action) {
  DidReceiveAction(action, nullptr /* details */);
}

void MediaSessionImpl::DidReceiveAction(
    media_session::mojom::MediaSessionAction action,
    blink::mojom::MediaSessionActionDetailsPtr details) {
  // Pause all players in non-routed frames if the action is PAUSE.
  //
  // This is the default PAUSE action handler per Media Session API spec. The
  // reason for pausing all players in all other sessions is to avoid the
  // players in other frames keep the session active so that the UI will always
  // show the pause button but it does not pause anything (as the routed frame
  // already pauses when responding to the PAUSE action while other frames does
  // not).
  //
  // TODO(zqzhang): Currently, this might not work well on desktop as Pepper and
  // OneShot players are not really suspended, so that the session is still
  // active after this. See https://crbug.com/619084 and
  // https://crbug.com/596516.
  if (media_session::mojom::MediaSessionAction::kPause == action) {
    RenderFrameHost* rfh_of_routed_service =
        routed_service_ ? routed_service_->GetRenderFrameHost() : nullptr;
    for (const auto& player : normal_players_) {
      if (player.first.observer->render_frame_host() != rfh_of_routed_service)
        player.first.observer->OnSuspend(player.first.player_id);
    }
    for (const auto& player : pepper_players_) {
      if (player.observer->render_frame_host() != rfh_of_routed_service) {
        player.observer->OnSetVolumeMultiplier(player.player_id,
                                               ducking_volume_multiplier_);
      }
    }
    for (const auto& player : one_shot_players_) {
      if (player.observer->render_frame_host() != rfh_of_routed_service)
        player.observer->OnSuspend(player.player_id);
    }
  }

  if (!routed_service_)
    return;

  routed_service_->GetClient()->DidReceiveAction(action, std::move(details));
}

bool MediaSessionImpl::IsServiceActiveForRenderFrameHost(RenderFrameHost* rfh) {
  return services_.find(rfh->GetGlobalId()) != services_.end();
}

void MediaSessionImpl::UpdateRoutedService() {
  MediaSessionServiceImpl* new_service = ComputeServiceForRouting();

  if (new_service == routed_service_)
    return;

  routed_service_ = new_service;

  RebuildAndNotifyMetadataChanged();
  RebuildAndNotifyActionsChanged();
  RebuildAndNotifyMediaSessionInfoChanged();
  RebuildAndNotifyMediaPositionChanged();

  if (routed_service_ &&
      !BackForwardCacheImpl::IsMediaSessionServiceAllowed()) {
    // A page in the back-forward cache may affect the media control UI
    // displayed to users. So it is marked as ineligible as soon as a
    // MediaSession service is associated with it.
    BackForwardCache::DisableForRenderFrameHost(
        routed_service_->GetRenderFrameHostId(),
        BackForwardCacheDisable::DisabledReason(
            BackForwardCacheDisable::DisabledReasonId::kMediaSessionService));
  }
}

MediaSessionServiceImpl* MediaSessionImpl::ComputeServiceForRouting() {
  // The service selection strategy is: select a frame that has a playing/paused
  // player and has a corresponding MediaSessionService and return the
  // corresponding MediaSessionService. If multiple frames satisfy the criteria,
  // prefer the top-most frame.
  std::set<RenderFrameHost*> frames;
  for (const auto& player : normal_players_) {
    RenderFrameHost* frame = player.first.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  for (const auto& player : one_shot_players_) {
    RenderFrameHost* frame = player.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  for (const auto& player : pepper_players_) {
    RenderFrameHost* frame = player.observer->render_frame_host();
    if (frame)
      frames.insert(frame);
  }

  RenderFrameHost* best_frame = nullptr;
  size_t min_depth = std::numeric_limits<size_t>::max();
  std::map<RenderFrameHost*, size_t> map_rfh_to_depth;

  for (RenderFrameHost* frame : frames) {
    size_t depth = ComputeFrameDepth(frame, &map_rfh_to_depth);
    if (depth >= min_depth)
      continue;
    if (!IsServiceActiveForRenderFrameHost(frame))
      continue;
    best_frame = frame;
    min_depth = depth;
  }

  // If we don't have a suitable frame yet, then take the topmost frame that has
  // a MediaSessionService.
  if (!best_frame && base::FeatureList::IsEnabled(
                         blink::features::kMediaSessionEnterPictureInPicture)) {
    // `FrameTree::Nodes()` iterates in breadth-first order, so this is
    // guaranteed to find the topmost (or tied topmost) frame with an active
    // MediaSessionService.
    for (FrameTreeNode* node : static_cast<WebContentsImpl*>(web_contents())
                                   ->GetPrimaryFrameTree()
                                   .Nodes()) {
      RenderFrameHost* rfh = node->current_frame_host();
      if (IsServiceActiveForRenderFrameHost(rfh)) {
        best_frame = rfh;
        break;
      }
    }
  }

  return best_frame ? services_[best_frame->GetGlobalId()] : nullptr;
}

void MediaSessionImpl::OnMediaMutedStatusChanged(bool mute) {
  is_muted_ = mute;
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::OnPictureInPictureAvailabilityChanged() {
  if (normal_players_.size() != 1)
    return;

  RebuildAndNotifyActionsChanged();
}

void MediaSessionImpl::OnAudioOutputSinkIdChanged() {
  if (audio_device_id_for_origin_ &&
      audio_device_id_for_origin_ != GetSharedAudioOutputDeviceId()) {
    audio_device_id_for_origin_.reset();
  }

  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::OnAudioOutputSinkChangingDisabled() {
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::OnVideoVisibilityChanged() {
  if (normal_players_.size() == 0) {
    return;
  }
  RebuildAndNotifyMediaSessionInfoChanged();
}

void MediaSessionImpl::SetRemotePlaybackMetadata(
    media_session::mojom::RemotePlaybackMetadataPtr metadata) {
  remote_playback_metadata_ = std::move(metadata);
  RebuildAndNotifyMediaSessionInfoChanged();
}

bool MediaSessionImpl::ShouldRouteAction(
    media_session::mojom::MediaSessionAction action) const {
  return routed_service_ && base::Contains(routed_service_->actions(), action);
}

const base::UnguessableToken& MediaSessionImpl::GetSourceId() const {
  return MediaSessionData::GetOrCreate(web_contents()->GetBrowserContext())
      ->source_id();
}

const base::UnguessableToken& MediaSessionImpl::GetRequestId() const {
  return delegate_->request_id();
}

base::WeakPtr<MediaSessionImpl> MediaSessionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MediaSessionImpl::RebuildAndNotifyActionsChanged() {
  std::set<media_session::mojom::MediaSessionAction> actions =
      routed_service_ ? routed_service_->actions()
                      : std::set<media_session::mojom::MediaSessionAction>();

  // Picture-in-Picture window controller needs to know only actions that are
  // handled by the website.
  if (auto* pip_window_controller_ =
          VideoPictureInPictureWindowControllerImpl::FromWebContents(
              web_contents())) {
    pip_window_controller_->MediaSessionActionsChanged(actions);
  }

  // If we are controllable then we should always add these actions as we can
  // support them by directly interacting with the players underneath.
  if (IsControllable()) {
    actions.insert(media_session::mojom::MediaSessionAction::kPlay);
    actions.insert(media_session::mojom::MediaSessionAction::kPause);
    actions.insert(media_session::mojom::MediaSessionAction::kStop);

    // Support seeking as long as this isn't live media.
    if (!is_considered_live_) {
      actions.insert(media_session::mojom::MediaSessionAction::kSeekTo);
      actions.insert(media_session::mojom::MediaSessionAction::kScrubTo);
      actions.insert(media_session::mojom::MediaSessionAction::kSeekForward);
      actions.insert(media_session::mojom::MediaSessionAction::kSeekBackward);
    }
  }

  // If the website has specified an action handler for 'enterpictureinpicture',
  // then we should expose EnterAutoPictureInPicture as an available action.
  if (base::FeatureList::IsEnabled(
          blink::features::kMediaSessionEnterPictureInPicture) &&
      base::Contains(
          actions,
          media_session::mojom::MediaSessionAction::kEnterPictureInPicture)) {
    actions.insert(
        media_session::mojom::MediaSessionAction::kEnterAutoPictureInPicture);
    actions.insert(
        media_session::mojom::MediaSessionAction::kExitPictureInPicture);
  }

  if (base::FeatureList::IsEnabled(
          media::kGlobalMediaControlsPictureInPicture)) {
    if (IsPictureInPictureAvailable()) {
      actions.insert(
          media_session::mojom::MediaSessionAction::kEnterPictureInPicture);
      actions.insert(
          media_session::mojom::MediaSessionAction::kExitPictureInPicture);
    } else if (web_contents()->HasPictureInPictureVideo() ||
               web_contents()->HasPictureInPictureDocument()) {
      // If the media is already in the picture-in-picture state, we allow the
      // player to exit it.
      actions.insert(
          media_session::mojom::MediaSessionAction::kExitPictureInPicture);
    }
  }

  if (base::FeatureList::IsEnabled(
          media::kGlobalMediaControlsSeamlessTransfer) &&
      IsAudioOutputDeviceSwitchingSupported()) {
    actions.insert(
        media_session::mojom::MediaSessionAction::kSwitchAudioDevice);
  }

  if (actions_ == actions)
    return;

  actions_ = actions;

  std::vector<media_session::mojom::MediaSessionAction> actions_vec(
      actions.begin(), actions.end());
  for (auto& observer : observers_)
    observer->MediaSessionActionsChanged(actions_vec);
}

void MediaSessionImpl::RebuildAndNotifyMetadataChanged() {
  std::vector<media_session::MediaImage> artwork;
  media_session::MediaMetadata metadata;
  BuildMetadata(metadata, artwork);

  // If we have no artwork in |images_| or the arwork has changed then we should
  // update it with the latest artwork from the routed service.
  auto it = images_.find(MediaSessionImageType::kArtwork);
  bool images_changed = it == images_.end() || it->second != artwork;
  if (images_changed) {
    images_.insert_or_assign(MediaSessionImageType::kArtwork, artwork);
  }
  bool metadata_changed = metadata_ != metadata;
  if (metadata_changed) {
    metadata_ = metadata;
  }

  if (!images_changed && !metadata_changed) {
    return;
  }
  for (auto& observer : observers_) {
    if (metadata_changed) {
      observer->MediaSessionMetadataChanged(this->metadata_);
    }

    if (images_changed) {
      observer->MediaSessionImagesChanged(this->images_);
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void MediaSessionImpl::BuildPlaceholderMetadata(
    media_session::MediaMetadata& metadata,
    std::vector<media_session::MediaImage>& artwork) {
  if ((routed_service_ && routed_service_->metadata()) ||
      !metadata_.IsEmpty()) {
    MediaSessionClient* media_session_client = MediaSessionClient::Get();
    CHECK(media_session_client);

    metadata.title = media_session_client->GetTitlePlaceholder();
    metadata.artist = media_session_client->GetArtistPlaceholder();
    metadata.album = media_session_client->GetAlbumPlaceholder();
    metadata.source_title = media_session_client->GetSourceTitlePlaceholder();

    // Always make sure the metadata replacement is accompanied by the thumbnail
    // replacement.
    // An empty `MediaImage` so `GetMediaImageBitmap` is eventually triggered.
    // That is where we replace the artwork with the placeholder `Bitmap`.
    artwork.push_back(media_session::MediaImage());
  }
}
#endif

void MediaSessionImpl::BuildMetadata(
    media_session::MediaMetadata& metadata,
    std::vector<media_session::MediaImage>& artwork) {
  // We need to hide the metadata for ChromeOS here because the
  // `MediaNotificationItem` lives in //components which cannot depend on
  // //content. For other platforms, metadata is hidden in the
  // `SystemMediaControlsNotifier`.
#if BUILDFLAG(IS_CHROMEOS)
  if (session_info_ && session_info_->hide_metadata) {
    BuildPlaceholderMetadata(metadata, artwork);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (routed_service_ && routed_service_->metadata()) {
    metadata.title = routed_service_->metadata()->title;
    metadata.artist = routed_service_->metadata()->artist;
    metadata.album = routed_service_->metadata()->album;
    metadata.chapters = routed_service_->metadata()->chapterInfo;
    artwork = routed_service_->metadata()->artwork;
  }

  if (metadata.title.empty()) {
    metadata.title = SanitizeMediaTitle(web_contents()->GetTitle());
  }

  ContentClient* content_client = GetContentClient();
  const GURL& url = web_contents()->GetLastCommittedURL();

  // If |url| wraps a chrome extension ID or System Web App, we can display
  // the extension or app name instead, which is more human-readable.
  std::u16string source_title;
  WebContentsDelegate* delegate = web_contents()->GetDelegate();
  if (delegate) {
    source_title =
        base::UTF8ToUTF16(delegate->GetTitleForMediaControls(web_contents()));
  }

  if (source_title.empty()) {
    // If the url is a file then we should display a placeholder.
    source_title =
        url.SchemeIsFile()
            ? content_client->GetLocalizedString(IDS_MEDIA_SESSION_FILE_SOURCE)
            : url_formatter::FormatUrl(
                  url::Origin::Create(url).GetURL(),
                  url_formatter::kFormatUrlOmitDefaults |
                      url_formatter::kFormatUrlOmitHTTPS |
                      url_formatter::kFormatUrlOmitTrivialSubdomains,
                  base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  }

  metadata.source_title = source_title;
}

bool MediaSessionImpl::IsPictureInPictureAvailable() const {
  if (normal_players_.size() != 1)
    return false;

  auto& first = normal_players_.begin()->first;
  return first.observer->IsPictureInPictureAvailable(first.player_id);
}

bool MediaSessionImpl::HasSufficientlyVisibleVideo() const {
  for (const auto& player : normal_players_) {
    if (player.first.observer->HasSufficientlyVisibleVideo(
            player.first.player_id)) {
      return true;
    }
  }

  return false;
}

void MediaSessionImpl::GetVisibility(
    GetVisibilityCallback get_visibility_callback) {
  if (normal_players_.empty()) {
    std::move(get_visibility_callback).Run(false);
    return;
  }

  scoped_refptr<MediaPlayersCallbackAggregator> aggregator =
      MakeRefCounted<MediaPlayersCallbackAggregator>(
          std::move(get_visibility_callback));
  for (const auto& player : normal_players_) {
    if (player.first.observer->IsPaused(player.first.player_id)) {
      continue;
    }
    player.first.observer->OnRequestVisibility(
        player.first.player_id, aggregator->CreateVisibilityCallback());
  }
}

std::string MediaSessionImpl::GetSharedAudioOutputDeviceId() const {
  if (normal_players_.empty())
    return media::AudioDeviceDescription::kDefaultDeviceId;

  auto& first = normal_players_.begin()->first;
  const auto& first_id = first.observer->GetAudioOutputSinkId(first.player_id);
  if (base::ranges::all_of(normal_players_, [&first_id](const auto& player) {
        return player.first.observer->GetAudioOutputSinkId(
                   player.first.player_id) == first_id;
      })) {
    return first_id;
  }

  return media::AudioDeviceDescription::kDefaultDeviceId;
}

bool MediaSessionImpl::IsAudioOutputDeviceSwitchingSupported() const {
  if (normal_players_.empty())
    return false;

  return base::ranges::all_of(normal_players_, [](const auto& player) {
    return player.first.observer->SupportsAudioOutputDeviceSwitching(
        player.first.player_id);
  });
}

std::vector<MediaAudioVideoState> MediaSessionImpl::GetMediaAudioVideoStates() {
  RenderFrameHost* routed_rfh =
      routed_service_ ? routed_service_->GetRenderFrameHost() : nullptr;
  std::vector<MediaAudioVideoState> states;

  ForAllPlayers(base::BindRepeating(
      [](RenderFrameHost* routed_rfh, std::vector<MediaAudioVideoState>* states,
         const PlayerIdentifier& player) {
        // If we have a routed frame then we should limit the players to the
        // frame so it is aligned with the media metadata.
        if (routed_rfh && player.observer->render_frame_host() != routed_rfh)
          return;

        const bool has_audio = player.observer->HasAudio(player.player_id);
        const bool has_video = player.observer->HasVideo(player.player_id);
        if (has_audio && has_video) {
          states->push_back(MediaAudioVideoState::kAudioVideo);
        } else if (has_audio) {
          states->push_back(MediaAudioVideoState::kAudioOnly);
        } else if (has_video) {
          states->push_back(MediaAudioVideoState::kVideoOnly);
        }
      },
      routed_rfh, &states));

  return states;
}

void MediaSessionImpl::ForAllPlayers(
    base::RepeatingCallback<void(const PlayerIdentifier&)> callback) {
  for (const auto& player : normal_players_)
    callback.Run(player.first);

  for (const auto& player : one_shot_players_)
    callback.Run(player);

  for (const auto& player : pepper_players_)
    callback.Run(player);
}

std::optional<media_session::MediaPosition>
MediaSessionImpl::MaybeGuardDurationUpdate(
    std::optional<media_session::MediaPosition> position) {
  if (!position) {
    // |position| should never go back to unset state once it's
    // set. Therefore it's safe to return it here when it's unset.
    DCHECK(!is_throttling_);
    return position;
  }

  if (position_ && position_->duration() == position->duration())
    return position;

  if (duration_update_allowance_ == 0) {
    is_throttling_ = true;
    DCHECK(duration_update_allowance_timer_.IsRunning());

    // Reset the timer so that we can keep the media as livestream
    // until the time difference between two updates is greater
    // than |kDurationUpdateAllowanceIncreaseInterval|.
    duration_update_allowance_timer_.Reset();

    return media_session::MediaPosition(
        position->playback_rate(), base::TimeDelta::Max(),
        position->GetPosition(), position->end_of_media());
  }

  --duration_update_allowance_;
  DCHECK_GE(duration_update_allowance_, 0);
  if (!duration_update_allowance_timer_.IsRunning()) {
    duration_update_allowance_timer_.Start(
        FROM_HERE, kDurationUpdateAllowanceIncreaseInterval, this,
        &MediaSessionImpl::IncreaseDurationUpdateAllowance);
  }

  return position;
}

void MediaSessionImpl::IncreaseDurationUpdateAllowance() {
  ++duration_update_allowance_;

  if (duration_update_allowance_ == kDurationUpdateMaxAllowance)
    duration_update_allowance_timer_.Stop();

  if (is_throttling_) {
    is_throttling_ = false;
    RebuildAndNotifyMediaPositionChanged();
  }
}

void MediaSessionImpl::ResetDurationUpdateGuard() {
  duration_update_allowance_timer_.Stop();
  duration_update_allowance_ = kDurationUpdateMaxAllowance;
  is_throttling_ = false;
  guarding_player_id_.reset();
}

void MediaSessionImpl::SetShouldThrottleDurationUpdateForTest(
    bool should_throttle) {
  should_throttle_duration_update_ = should_throttle;
}

bool MediaSessionImpl::HasImageCacheForTest(const GURL& image_url) const {
  return GetPageData(web_contents()->GetPrimaryPage()).GetImageCache(image_url);
}

MediaSessionImpl::PageData::PageData(content::Page& page)
    : PageUserData(page) {}

MediaSessionImpl::PageData::~PageData() = default;

MediaSessionImpl::PageData& MediaSessionImpl::GetPageData(
    content::Page& page) const {
  return *PageData::GetOrCreateForPage(page);
}

PAGE_USER_DATA_KEY_IMPL(MediaSessionImpl::PageData);

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaSessionImpl);

}  // namespace content
