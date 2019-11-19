// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/audio_focus_request.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

namespace {

mojom::EnforcementMode GetDefaultEnforcementMode() {
  if (base::FeatureList::IsEnabled(features::kAudioFocusEnforcement)) {
    if (base::FeatureList::IsEnabled(features::kAudioFocusSessionGrouping))
      return mojom::EnforcementMode::kSingleGroup;
    return mojom::EnforcementMode::kSingleSession;
  }

  return mojom::EnforcementMode::kNone;
}

}  // namespace

// MediaPowerDelegate will pause all playback if the device is suspended.
class MediaPowerDelegate : public base::PowerObserver {
 public:
  explicit MediaPowerDelegate(base::WeakPtr<AudioFocusManager> owner)
      : owner_(owner) {
    base::PowerMonitor::AddObserver(this);
  }

  ~MediaPowerDelegate() override { base::PowerMonitor::RemoveObserver(this); }

  // base::PowerObserver:
  void OnSuspend() override {
    DCHECK(owner_);
    owner_->SuspendAllSessions();
  }

 private:
  const base::WeakPtr<AudioFocusManager> owner_;

  DISALLOW_COPY_AND_ASSIGN(MediaPowerDelegate);
};

class AudioFocusManager::SourceObserverHolder {
 public:
  SourceObserverHolder(AudioFocusManager* owner,
                       const base::UnguessableToken& source_id,
                       mojo::PendingRemote<mojom::AudioFocusObserver> observer)
      : identity_(source_id), observer_(std::move(observer)) {
    // Set a connection error handler so that we will remove observers that have
    // had an error / been closed.
    observer_.set_disconnect_handler(base::BindOnce(
        &AudioFocusManager::CleanupSourceObservers, base::Unretained(owner)));
  }

  ~SourceObserverHolder() = default;

  bool is_valid() const { return observer_.is_connected(); }

  const base::UnguessableToken& identity() const { return identity_; }

  void OnFocusGained(mojom::AudioFocusRequestStatePtr session) {
    observer_->OnFocusGained(std::move(session));
  }

  void OnFocusLost(mojom::AudioFocusRequestStatePtr session) {
    observer_->OnFocusLost(std::move(session));
  }

 private:
  const base::UnguessableToken identity_;
  mojo::Remote<mojom::AudioFocusObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(SourceObserverHolder);
};

void AudioFocusManager::RequestAudioFocus(
    mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
    mojo::PendingRemote<mojom::MediaSession> media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    RequestAudioFocusCallback callback) {
  auto request_id = base::UnguessableToken::Create();

  RequestAudioFocusInternal(
      std::make_unique<AudioFocusRequest>(
          weak_ptr_factory_.GetWeakPtr(), std::move(receiver),
          std::move(media_session), std::move(session_info), type, request_id,
          GetBindingSourceName(), base::UnguessableToken::Create(),
          GetBindingIdentity()),
      type);

  std::move(callback).Run(request_id);
}

void AudioFocusManager::RequestGroupedAudioFocus(
    const base::UnguessableToken& request_id,
    mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
    mojo::PendingRemote<mojom::MediaSession> media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    const base::UnguessableToken& group_id,
    RequestGroupedAudioFocusCallback callback) {
  if (IsFocusEntryPresent(request_id)) {
    std::move(callback).Run(false /* success */);
    return;
  }

  RequestAudioFocusInternal(
      std::make_unique<AudioFocusRequest>(
          weak_ptr_factory_.GetWeakPtr(), std::move(receiver),
          std::move(media_session), std::move(session_info), type, request_id,
          GetBindingSourceName(), group_id, GetBindingIdentity()),
      type);

  std::move(callback).Run(true /* success */);
}

void AudioFocusManager::GetFocusRequests(GetFocusRequestsCallback callback) {
  std::vector<mojom::AudioFocusRequestStatePtr> requests;

  for (const auto& row : audio_focus_stack_)
    requests.push_back(row->ToAudioFocusRequestState());

  std::move(callback).Run(std::move(requests));
}

void AudioFocusManager::GetDebugInfoForRequest(
    const RequestId& request_id,
    GetDebugInfoForRequestCallback callback) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() != request_id)
      continue;

    row->ipc()->GetDebugInfo(base::BindOnce(
        [](const base::UnguessableToken& group_id,
           const base::UnguessableToken& identity,
           GetDebugInfoForRequestCallback callback,
           mojom::MediaSessionDebugInfoPtr info) {
          // Inject the |group_id| into the state string. This is because in
          // some cases the group id is automatically generated by the media
          // session service so the session is unaware of it.
          if (!info->state.empty())
            info->state += " ";
          info->state += "GroupId=" + group_id.ToString();

          // Inject the identity into the state string.
          info->state += " Identity=" + identity.ToString();

          std::move(callback).Run(std::move(info));
        },
        row->group_id(), row->identity(), std::move(callback)));
    return;
  }

  std::move(callback).Run(mojom::MediaSessionDebugInfo::New());
}

void AudioFocusManager::AbandonAudioFocusInternal(RequestId id) {
  if (audio_focus_stack_.empty())
    return;

  bool was_top_most_session = audio_focus_stack_.back()->id() == id;

  auto row = RemoveFocusEntryIfPresent(id);
  if (!row)
    return;

  EnforceAudioFocus();
  MaybeUpdateActiveSession();

  // Notify observers that we lost audio focus.
  mojom::AudioFocusRequestStatePtr session_state =
      row->ToAudioFocusRequestState();

  for (const auto& observer : observers_)
    observer->OnFocusLost(session_state.Clone());

  for (auto& holder : source_observers_) {
    if (holder->identity() == row->identity())
      holder->OnFocusLost(session_state.Clone());
  }

  if (!was_top_most_session || audio_focus_stack_.empty())
    return;

  // Notify observers that the session on top gained focus.
  AudioFocusRequest* new_session = audio_focus_stack_.back().get();

  for (const auto& observer : observers_)
    observer->OnFocusGained(new_session->ToAudioFocusRequestState());

  for (auto& holder : source_observers_) {
    if (holder->identity() == new_session->identity())
      holder->OnFocusGained(new_session->ToAudioFocusRequestState());
  }
}

void AudioFocusManager::AddObserver(
    mojo::PendingRemote<mojom::AudioFocusObserver> observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.Add(std::move(observer));
}

void AudioFocusManager::SetSource(const base::UnguessableToken& identity,
                                  const std::string& name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto& context = receivers_.current_context();
  context->identity = identity;
  context->source_name = name;
}

void AudioFocusManager::SetEnforcementMode(mojom::EnforcementMode mode) {
  if (mode == mojom::EnforcementMode::kDefault)
    mode = GetDefaultEnforcementMode();

  if (mode == enforcement_mode_)
    return;

  enforcement_mode_ = mode;

  if (audio_focus_stack_.empty())
    return;

  EnforceAudioFocus();
}

void AudioFocusManager::AddSourceObserver(
    const base::UnguessableToken& source_id,
    mojo::PendingRemote<mojom::AudioFocusObserver> observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_observers_.push_back(std::make_unique<SourceObserverHolder>(
      this, source_id, std::move(observer)));
}

void AudioFocusManager::GetSourceFocusRequests(
    const base::UnguessableToken& source_id,
    GetFocusRequestsCallback callback) {
  std::vector<mojom::AudioFocusRequestStatePtr> requests;

  for (const auto& row : audio_focus_stack_) {
    if (row->identity() == source_id)
      requests.push_back(row->ToAudioFocusRequestState());
  }

  std::move(callback).Run(std::move(requests));
}

void AudioFocusManager::CreateActiveMediaController(
    mojo::PendingReceiver<mojom::MediaController> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  active_media_controller_.BindToInterface(std::move(receiver));
}

void AudioFocusManager::CreateMediaControllerForSession(
    mojo::PendingReceiver<mojom::MediaController> receiver,
    const base::UnguessableToken& receiver_id) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() != receiver_id)
      continue;

    row->BindToMediaController(std::move(receiver));
    break;
  }
}

void AudioFocusManager::SuspendAllSessions() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& row : audio_focus_stack_)
    row->ipc()->Suspend(mojom::MediaSession::SuspendType::kUI);
}

void AudioFocusManager::BindToInterface(
    mojo::PendingReceiver<mojom::AudioFocusManager> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  receivers_.Add(this, std::move(receiver),
                 std::make_unique<ReceiverContext>());
}

void AudioFocusManager::BindToDebugInterface(
    mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  debug_receivers_.Add(this, std::move(receiver));
}

void AudioFocusManager::BindToControllerManagerInterface(
    mojo::PendingReceiver<mojom::MediaControllerManager> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  controller_receivers_.Add(this, std::move(receiver));
}

void AudioFocusManager::RequestAudioFocusInternal(
    std::unique_ptr<AudioFocusRequest> row,
    mojom::AudioFocusType type) {
  const auto& identity = row->identity();
  row->set_audio_focus_type(type);
  audio_focus_stack_.push_back(std::move(row));

  EnforceAudioFocus();
  MaybeUpdateActiveSession();

  // Notify observers that we were gained audio focus.
  mojom::AudioFocusRequestStatePtr session_state =
      audio_focus_stack_.back()->ToAudioFocusRequestState();
  for (const auto& observer : observers_)
    observer->OnFocusGained(session_state.Clone());

  for (auto& holder : source_observers_) {
    if (holder->identity() == identity)
      holder->OnFocusGained(session_state.Clone());
  }
}

void AudioFocusManager::EnforceAudioFocus() {
  DCHECK_NE(mojom::EnforcementMode::kDefault, enforcement_mode_);
  if (audio_focus_stack_.empty())
    return;

  EnforcementState state;

  for (auto& session : base::Reversed(audio_focus_stack_)) {
    EnforceSingleSession(session.get(), state);

    // Update the flags based on the audio focus type of this session. If the
    // session is suspended then any transient audio focus type should not have
    // an effect.
    switch (session->audio_focus_type()) {
      case mojom::AudioFocusType::kGain:
        state.should_stop = true;
        break;
      case mojom::AudioFocusType::kGainTransient:
        if (!session->IsSuspended())
          state.should_suspend = true;
        break;
      case mojom::AudioFocusType::kGainTransientMayDuck:
        if (!session->IsSuspended())
          state.should_duck = true;
        break;
      case mojom::AudioFocusType::kAmbient:
        break;
    }
  }
}

void AudioFocusManager::MaybeUpdateActiveSession() {
  AudioFocusRequest* active = nullptr;

  for (auto& row : base::Reversed(audio_focus_stack_)) {
    if (!row->info()->is_controllable)
      continue;

    active = row.get();
    break;
  }

  if (active) {
    active_media_controller_.SetMediaSession(active);
  } else {
    active_media_controller_.ClearMediaSession();
  }
}

AudioFocusManager::AudioFocusManager()
    : enforcement_mode_(GetDefaultEnforcementMode()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  power_delegate_ =
      std::make_unique<MediaPowerDelegate>(weak_ptr_factory_.GetWeakPtr());
}

AudioFocusManager::~AudioFocusManager() = default;

std::unique_ptr<AudioFocusRequest> AudioFocusManager::RemoveFocusEntryIfPresent(
    RequestId id) {
  std::unique_ptr<AudioFocusRequest> row;

  for (auto iter = audio_focus_stack_.begin(); iter != audio_focus_stack_.end();
       ++iter) {
    if ((*iter)->id() == id) {
      row.swap((*iter));
      audio_focus_stack_.erase(iter);
      break;
    }
  }

  return row;
}

bool AudioFocusManager::IsFocusEntryPresent(
    const base::UnguessableToken& id) const {
  for (auto& row : audio_focus_stack_) {
    if (row->id() == id)
      return true;
  }

  return false;
}

const std::string& AudioFocusManager::GetBindingSourceName() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return receivers_.current_context()->source_name;
}

const base::UnguessableToken& AudioFocusManager::GetBindingIdentity() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return receivers_.current_context()->identity;
}

bool AudioFocusManager::IsSessionOnTopOfAudioFocusStack(
    RequestId id,
    mojom::AudioFocusType type) const {
  return !audio_focus_stack_.empty() && audio_focus_stack_.back()->id() == id &&
         audio_focus_stack_.back()->audio_focus_type() == type;
}

bool AudioFocusManager::ShouldSessionBeSuspended(
    const AudioFocusRequest* session,
    const EnforcementState& state) const {
  bool should_suspend_any = state.should_stop || state.should_suspend;

  switch (enforcement_mode_) {
    case mojom::EnforcementMode::kSingleSession:
      return should_suspend_any;
    case mojom::EnforcementMode::kSingleGroup:
      return should_suspend_any &&
             session->group_id() != audio_focus_stack_.back()->group_id();
    case mojom::EnforcementMode::kNone:
      return false;
    case mojom::EnforcementMode::kDefault:
      NOTIMPLEMENTED();
      return false;
  }
}

bool AudioFocusManager::ShouldSessionBeDucked(
    const AudioFocusRequest* session,
    const EnforcementState& state) const {
  switch (enforcement_mode_) {
    case mojom::EnforcementMode::kSingleSession:
    case mojom::EnforcementMode::kSingleGroup:
      if (session->info()->force_duck)
        return state.should_duck || ShouldSessionBeSuspended(session, state);
      return state.should_duck;
    case mojom::EnforcementMode::kNone:
      return false;
    case mojom::EnforcementMode::kDefault:
      NOTIMPLEMENTED();
      return false;
  }
}

void AudioFocusManager::EnforceSingleSession(AudioFocusRequest* session,
                                             const EnforcementState& state) {
  if (ShouldSessionBeDucked(session, state)) {
    session->ipc()->StartDucking();
  } else {
    session->ipc()->StopDucking();
  }

  // If the session wants to be ducked instead of suspended we should stop now.
  if (session->info()->force_duck)
    return;

  if (ShouldSessionBeSuspended(session, state)) {
    session->Suspend(state);
  } else {
    session->ReleaseTransientHold();
  }
}

void AudioFocusManager::CleanupSourceObservers() {
  base::EraseIf(source_observers_,
                [](const auto& holder) { return !holder->is_valid(); });
}

}  // namespace media_session
