// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "base/strings/string_util.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/platform/modules/mediasession/media_session.mojom.h"

#if defined(OS_ANDROID)
#include "content/browser/media/session/media_session_android.h"
#endif  // defined(OS_ANDROID)

namespace content {

using MediaSessionUserAction = MediaSessionUmaHelper::MediaSessionUserAction;
using media_session::mojom::MediaSessionInfo;

namespace {

const double kUnduckedVolumeMultiplier = 1.0;
const double kDefaultDuckingVolumeMultiplier = 0.2;

const char kDebugInfoOwnerSeparator[] = " - ";
const char kDebugInfoDucked[] = "Ducked";
const char kDebugInfoActive[] = "Active";
const char kDebugInfoInactive[] = "Inactive";
const char kDebugInfoStateSeparator[] = " ";

using MapRenderFrameHostToDepth = std::map<RenderFrameHost*, size_t>;

using media_session::mojom::AudioFocusType;

using MediaSessionSuspendedSource =
    MediaSessionUmaHelper::MediaSessionSuspendedSource;

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
    current_frame = current_frame->GetParent();
  }
  (*map_rfh_to_depth)[rfh] = depth;
  return depth;
}

MediaSessionUserAction MediaSessionActionToUserAction(
    blink::mojom::MediaSessionAction action) {
  switch (action) {
    case blink::mojom::MediaSessionAction::PLAY:
      return MediaSessionUserAction::Play;
    case blink::mojom::MediaSessionAction::PAUSE:
      return MediaSessionUserAction::Pause;
    case blink::mojom::MediaSessionAction::PREVIOUS_TRACK:
      return MediaSessionUserAction::PreviousTrack;
    case blink::mojom::MediaSessionAction::NEXT_TRACK:
      return MediaSessionUserAction::NextTrack;
    case blink::mojom::MediaSessionAction::SEEK_BACKWARD:
      return MediaSessionUserAction::SeekBackward;
    case blink::mojom::MediaSessionAction::SEEK_FORWARD:
      return MediaSessionUserAction::SeekForward;
  }
  NOTREACHED();
  return MediaSessionUserAction::Count;
}

// If the string is not empty then push it to the back of a vector.
void MaybePushBackString(std::vector<std::string>& vector,
                         const std::string& str) {
  if (!str.empty())
    vector.push_back(str);
}

}  // anonymous namespace

MediaSessionImpl::PlayerIdentifier::PlayerIdentifier(
    MediaSessionPlayerObserver* observer,
    int player_id)
    : observer(observer), player_id(player_id) {}

bool MediaSessionImpl::PlayerIdentifier::operator==(
    const PlayerIdentifier& other) const {
  return this->observer == other.observer && this->player_id == other.player_id;
}

bool MediaSessionImpl::PlayerIdentifier::operator<(
    const PlayerIdentifier& other) const {
  return MediaSessionImpl::PlayerIdentifier::Hash()(*this) <
         MediaSessionImpl::PlayerIdentifier::Hash()(other);
}

size_t MediaSessionImpl::PlayerIdentifier::Hash::operator()(
    const PlayerIdentifier& player_identifier) const {
  size_t hash = BASE_HASH_NAMESPACE::hash<MediaSessionPlayerObserver*>()(
      player_identifier.observer);
  hash += BASE_HASH_NAMESPACE::hash<int>()(player_identifier.player_id);
  return hash;
}

// static
MediaSession* MediaSession::Get(WebContents* web_contents) {
  return MediaSessionImpl::Get(web_contents);
}

// static
MediaSessionImpl* MediaSessionImpl::Get(WebContents* web_contents) {
  MediaSessionImpl* session = FromWebContents(web_contents);
  if (!session) {
    CreateForWebContents(web_contents);
    session = FromWebContents(web_contents);
    session->Initialize();
  }
  return session;
}

MediaSessionImpl::~MediaSessionImpl() {
  DCHECK(normal_players_.empty());
  DCHECK(pepper_players_.empty());
  DCHECK(one_shot_players_.empty());
  DCHECK(audio_focus_state_ == State::INACTIVE);
  for (auto& observer : observers_) {
    observer.MediaSessionDestroyed();
    observer.StopObserving();
  }
}

void MediaSessionImpl::WebContentsDestroyed() {
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
}

void MediaSessionImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (services_.count(rfh))
    OnServiceDestroyed(services_[rfh]);
}

void MediaSessionImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
  if (services_.count(rfh))
    services_[rfh]->DidFinishNavigation();
}

void MediaSessionImpl::AddObserver(MediaSessionObserver* observer) {
  observers_.AddObserver(observer);
  NotifyAddedObserver(observer);
}

void MediaSessionImpl::RemoveObserver(MediaSessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaSessionImpl::NotifyAddedObserver(MediaSessionObserver* observer) {
  observer->MediaSessionMetadataChanged(
      routed_service_ ? routed_service_->metadata() : base::nullopt);
  observer->MediaSessionActionsChanged(
      routed_service_ ? routed_service_->actions()
                      : std::set<blink::mojom::MediaSessionAction>());
  observer->MediaSessionStateChanged(IsControllable(), IsActuallyPaused());
}

void MediaSessionImpl::NotifyMediaSessionMetadataChange(
    const base::Optional<MediaMetadata>& metadata) {
  for (auto& observer : observers_)
    observer.MediaSessionMetadataChanged(metadata);
}

void MediaSessionImpl::NotifyMediaSessionActionsChange(
    const std::set<blink::mojom::MediaSessionAction>& actions) {
  for (auto& observer : observers_)
    observer.MediaSessionActionsChanged(actions);
}

bool MediaSessionImpl::AddPlayer(MediaSessionPlayerObserver* observer,
                                 int player_id,
                                 media::MediaContentType media_content_type) {
  if (media_content_type == media::MediaContentType::OneShot)
    return AddOneShotPlayer(observer, player_id);
  if (media_content_type == media::MediaContentType::Pepper)
    return AddPepperPlayer(observer, player_id);

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  AudioFocusType required_audio_focus_type;
  if (media_content_type == media::MediaContentType::Persistent)
    required_audio_focus_type = AudioFocusType::kGain;
  else
    required_audio_focus_type = AudioFocusType::kGainTransientMayDuck;

  PlayerIdentifier key(observer, player_id);

  // If the audio focus is already granted and is of type Content, there is
  // nothing to do. If it is granted of type Transient the requested type is
  // also transient, there is also nothing to do. Otherwise, the session needs
  // to request audio focus again.
  if (audio_focus_state_ == State::ACTIVE) {
    base::Optional<AudioFocusType> current_focus_type =
        delegate_->GetCurrentFocusType();
    if (current_focus_type == AudioFocusType::kGain ||
        current_focus_type == required_audio_focus_type) {
      auto iter = normal_players_.find(key);
      if (iter == normal_players_.end())
        normal_players_.emplace(std::move(key), required_audio_focus_type);
      else
        iter->second = required_audio_focus_type;
      return true;
    }
  }

  bool old_controllable = IsControllable();
  State old_audio_focus_state = audio_focus_state_;
  RequestSystemAudioFocus(required_audio_focus_type);

  if (audio_focus_state_ != State::ACTIVE)
    return false;

  // The session should be reset if a player is starting while all players are
  // suspended.
  if (old_audio_focus_state != State::ACTIVE)
    normal_players_.clear();

  auto iter = normal_players_.find(key);
  if (iter == normal_players_.end())
    normal_players_.emplace(std::move(key), required_audio_focus_type);
  else
    iter->second = required_audio_focus_type;

  UpdateRoutedService();

  if (old_audio_focus_state != audio_focus_state_ ||
      old_controllable != IsControllable()) {
    NotifyAboutStateChange();
  }

  return true;
}

void MediaSessionImpl::RemovePlayer(MediaSessionPlayerObserver* observer,
                                    int player_id) {
  bool was_controllable = IsControllable();

  PlayerIdentifier identifier(observer, player_id);

  auto iter = normal_players_.find(identifier);
  if (iter != normal_players_.end())
    normal_players_.erase(iter);

  auto it = pepper_players_.find(identifier);
  if (it != pepper_players_.end())
    pepper_players_.erase(it);

  it = one_shot_players_.find(identifier);
  if (it != one_shot_players_.end())
    one_shot_players_.erase(it);

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  // The session may become controllable after removing a one-shot player.
  // However AbandonSystemAudioFocusIfNeeded will short-return and won't notify
  // about the state change.
  if (!was_controllable && IsControllable())
    NotifyAboutStateChange();
}

void MediaSessionImpl::RemovePlayers(MediaSessionPlayerObserver* observer) {
  bool was_controllable = IsControllable();

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

  AbandonSystemAudioFocusIfNeeded();
  UpdateRoutedService();

  // The session may become controllable after removing a one-shot player.
  // However AbandonSystemAudioFocusIfNeeded will short-return and won't notify
  // about the state change.
  if (!was_controllable && IsControllable())
    NotifyAboutStateChange();
}

void MediaSessionImpl::RecordSessionDuck() {
  uma_helper_.RecordSessionSuspended(
      MediaSessionSuspendedSource::SystemTransientDuck);
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
  DCHECK(IsActive());
  OnSuspendInternal(SuspendType::kContent, State::SUSPENDED);
}

void MediaSessionImpl::Resume(SuspendType suspend_type) {
  if (!IsSuspended())
    return;

  if (suspend_type == SuspendType::kUI) {
    MediaSessionUmaHelper::RecordMediaSessionUserAction(
        MediaSessionUmaHelper::MediaSessionUserAction::PlayDefault);
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
  }

  OnResumeInternal(suspend_type);
}

void MediaSessionImpl::Suspend(SuspendType suspend_type) {
  if (!IsActive())
    return;

  if (suspend_type == SuspendType::kUI) {
    MediaSessionUmaHelper::RecordMediaSessionUserAction(
        MediaSessionUserAction::PauseDefault);
  }

  OnSuspendInternal(suspend_type, State::SUSPENDED);
}

void MediaSessionImpl::Stop(SuspendType suspend_type) {
  DCHECK(audio_focus_state_ != State::INACTIVE);
  DCHECK(suspend_type != SuspendType::kContent);
  DCHECK(!HasPepper());

  if (suspend_type == SuspendType::kUI) {
    MediaSessionUmaHelper::RecordMediaSessionUserAction(
        MediaSessionUmaHelper::MediaSessionUserAction::StopDefault);
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
}

void MediaSessionImpl::SeekForward(base::TimeDelta seek_time) {
  for (const auto& it : normal_players_)
    it.first.observer->OnSeekForward(it.first.player_id, seek_time);
}

void MediaSessionImpl::SeekBackward(base::TimeDelta seek_time) {
  for (const auto& it : normal_players_)
    it.first.observer->OnSeekBackward(it.first.player_id, seek_time);
}

bool MediaSessionImpl::IsControllable() const {
  // Only media session having focus Gain can be controllable unless it is
  // inactive. Also, the session will be uncontrollable if it contains one-shot
  // players.
  return audio_focus_state_ != State::INACTIVE &&
         desired_audio_focus_type_ == AudioFocusType::kGain &&
         one_shot_players_.empty();
}

bool MediaSessionImpl::IsActuallyPaused() const {
  if (routed_service_ && routed_service_->playback_state() ==
                             blink::mojom::MediaSessionPlaybackState::PLAYING) {
    return false;
  }

  return !IsActive();
}

void MediaSessionImpl::SetDuckingVolumeMultiplier(double multiplier) {
  ducking_volume_multiplier_ = base::ClampToRange(multiplier, 0.0, 1.0);
}

void MediaSessionImpl::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  UpdateVolumeMultiplier();
  NotifyObserversInfoChanged();
}

void MediaSessionImpl::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  UpdateVolumeMultiplier();
  NotifyObserversInfoChanged();
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

std::unique_ptr<base::CallbackList<void(MediaSessionImpl::State)>::Subscription>
MediaSessionImpl::RegisterMediaSessionStateChangedCallbackForTest(
    const StateChangedCallback& cb) {
  return media_session_state_listeners_.Add(cb);
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

void MediaSessionImpl::OnSystemAudioFocusRequested(bool result) {
  uma_helper_.RecordRequestAudioFocusResult(result);
  if (result)
    StopDucking();
}

void MediaSessionImpl::OnSuspendInternal(SuspendType suspend_type,
                                         State new_state) {
  DCHECK(!HasPepper());

  DCHECK(new_state == State::SUSPENDED || new_state == State::INACTIVE);
  // UI suspend cannot use State::INACTIVE.
  DCHECK(suspend_type == SuspendType::kSystem || new_state == State::SUSPENDED);

  if (!one_shot_players_.empty())
    return;

  if (audio_focus_state_ != State::ACTIVE)
    return;

  switch (suspend_type) {
    case SuspendType::kUI:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::UI);
      break;
    case SuspendType::kSystem:
      switch (new_state) {
        case State::SUSPENDED:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemTransient);
          break;
        case State::INACTIVE:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemPermanent);
          break;
        case State::ACTIVE:
          NOTREACHED();
          break;
      }
      break;
    case SuspendType::kContent:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::CONTENT);
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

  NotifyAboutStateChange();
}

void MediaSessionImpl::OnResumeInternal(SuspendType suspend_type) {
  if (suspend_type == SuspendType::kSystem && suspend_type_ != suspend_type)
    return;

  SetAudioFocusState(State::ACTIVE);

  for (const auto& it : normal_players_)
    it.first.observer->OnResume(it.first.player_id);

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());

  NotifyAboutStateChange();
}

MediaSessionImpl::MediaSessionImpl(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      audio_focus_state_(State::INACTIVE),
      desired_audio_focus_type_(AudioFocusType::kGainTransientMayDuck),
      is_ducking_(false),
      ducking_volume_multiplier_(kDefaultDuckingVolumeMultiplier),
      routed_service_(nullptr) {
#if defined(OS_ANDROID)
  session_android_.reset(new MediaSessionAndroid(this));
#endif  // defined(OS_ANDROID)
}

void MediaSessionImpl::Initialize() {
  delegate_ = AudioFocusDelegate::Create(this);
  delegate_->MediaSessionInfoChanged(GetMediaSessionInfoSync());
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

void MediaSessionImpl::BindToMojoRequest(
    mojo::InterfaceRequest<media_session::mojom::MediaSession> request) {
  bindings_.AddBinding(this, std::move(request));
}

void MediaSessionImpl::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());

  // Convert the address of |this| to a string and use it as the name.
  std::stringstream stream;
  stream << this;
  info->name = stream.str();

  // Add the title and the url to the owner.
  std::vector<std::string> owner_parts;
  MaybePushBackString(owner_parts,
                      base::UTF16ToUTF8(web_contents()->GetTitle()));
  MaybePushBackString(owner_parts,
                      web_contents()->GetLastCommittedURL().spec());
  info->owner = base::JoinString(owner_parts, kDebugInfoOwnerSeparator);

  // Add the ducking state to state.
  std::vector<std::string> state_parts;
  MaybePushBackString(state_parts,
                      IsActive() ? kDebugInfoActive : kDebugInfoInactive);
  MaybePushBackString(state_parts, is_ducking_ ? kDebugInfoDucked : "");
  info->state = base::JoinString(state_parts, kDebugInfoStateSeparator);

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
  return info;
}

void MediaSessionImpl::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoSync());
}

void MediaSessionImpl::AddObserver(
    media_session::mojom::MediaSessionObserverPtr observer) {
  observer->MediaSessionInfoChanged(GetMediaSessionInfoSync());
  mojo_observers_.AddPtr(std::move(observer));
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

  if (!result) {
    switch (audio_focus_type) {
      case AudioFocusType::kGain:
        // If the gain audio focus request failed then we should suspend the
        // media session.
        OnSuspendInternal(SuspendType::kSystem, State::SUSPENDED);
        break;
      case AudioFocusType::kGainTransient:
        // MediaSessionImpl does not use |kGainTransient|.
        NOTREACHED();
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
  DidReceiveAction(blink::mojom::MediaSessionAction::PREVIOUS_TRACK);
}

void MediaSessionImpl::NextTrack() {
  DidReceiveAction(blink::mojom::MediaSessionAction::NEXT_TRACK);
}

void MediaSessionImpl::AbandonSystemAudioFocusIfNeeded() {
  if (audio_focus_state_ == State::INACTIVE || !normal_players_.empty() ||
      !pepper_players_.empty() || !one_shot_players_.empty()) {
    return;
  }
  delegate_->AbandonAudioFocus();
  is_ducking_ = false;

  SetAudioFocusState(State::INACTIVE);
  NotifyAboutStateChange();
}

void MediaSessionImpl::NotifyAboutStateChange() {
  media_session_state_listeners_.Notify(audio_focus_state_);

  bool is_actually_paused = IsActuallyPaused();
  for (auto& observer : observers_)
    observer.MediaSessionStateChanged(IsControllable(), is_actually_paused);
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

  NotifyObserversInfoChanged();
}

void MediaSessionImpl::FlushForTesting() {
  mojo_observers_.FlushForTesting();
}

void MediaSessionImpl::NotifyObserversInfoChanged() {
  media_session::mojom::MediaSessionInfoPtr current_info =
      GetMediaSessionInfoSync();

  mojo_observers_.ForAllPtrs(
      [&current_info](media_session::mojom::MediaSessionObserver* observer) {
        observer->MediaSessionInfoChanged(current_info.Clone());
      });

  delegate_->MediaSessionInfoChanged(current_info.Clone());
}

bool MediaSessionImpl::AddPepperPlayer(MediaSessionPlayerObserver* observer,
                                       int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);
  DCHECK_NE(AudioFocusDelegate::AudioFocusResult::kFailed, result);

  pepper_players_.insert(PlayerIdentifier(observer, player_id));

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  NotifyAboutStateChange();
  return result != AudioFocusDelegate::AudioFocusResult::kFailed;
}

bool MediaSessionImpl::AddOneShotPlayer(MediaSessionPlayerObserver* observer,
                                        int player_id) {
  AudioFocusDelegate::AudioFocusResult result =
      RequestSystemAudioFocus(AudioFocusType::kGain);

  if (result == AudioFocusDelegate::AudioFocusResult::kFailed)
    return false;

  one_shot_players_.insert(PlayerIdentifier(observer, player_id));
  NotifyAboutStateChange();

  return true;
}

// MediaSessionService-related methods

void MediaSessionImpl::OnServiceCreated(MediaSessionServiceImpl* service) {
  RenderFrameHost* rfh = service->GetRenderFrameHost();
  if (!rfh)
    return;

  services_[rfh] = service;
  UpdateRoutedService();
}

void MediaSessionImpl::OnServiceDestroyed(MediaSessionServiceImpl* service) {
  services_.erase(service->GetRenderFrameHost());
  if (routed_service_ == service) {
    routed_service_ = nullptr;
    UpdateRoutedService();
  }
}

void MediaSessionImpl::OnMediaSessionPlaybackStateChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;
  NotifyAboutStateChange();
}

void MediaSessionImpl::OnMediaSessionMetadataChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  NotifyMediaSessionMetadataChange(routed_service_->metadata());
}

void MediaSessionImpl::OnMediaSessionActionsChanged(
    MediaSessionServiceImpl* service) {
  if (service != routed_service_)
    return;

  NotifyMediaSessionActionsChange(routed_service_->actions());
}

void MediaSessionImpl::DidReceiveAction(
    blink::mojom::MediaSessionAction action) {
  MediaSessionUmaHelper::RecordMediaSessionUserAction(
      MediaSessionActionToUserAction(action));

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
  if (blink::mojom::MediaSessionAction::PAUSE == action) {
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

  routed_service_->GetClient()->DidReceiveAction(action);
}

bool MediaSessionImpl::IsServiceActiveForRenderFrameHost(RenderFrameHost* rfh) {
  return services_.find(rfh) != services_.end();
}

void MediaSessionImpl::UpdateRoutedService() {
  MediaSessionServiceImpl* new_service = ComputeServiceForRouting();
  if (new_service == routed_service_)
    return;

  routed_service_ = new_service;
  if (routed_service_) {
    NotifyMediaSessionMetadataChange(routed_service_->metadata());
    NotifyMediaSessionActionsChange(routed_service_->actions());
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

  return best_frame ? services_[best_frame] : nullptr;
}

}  // namespace content
