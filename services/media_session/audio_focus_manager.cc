// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <iterator>
#include <utility>

#include "base/containers/adapters.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/audio_focus_manager_metrics_helper.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {

class AudioFocusManager::StackRow : public mojom::AudioFocusRequestClient {
 public:
  StackRow(AudioFocusManager* owner,
           mojom::AudioFocusRequestClientRequest request,
           mojom::MediaSessionPtr session,
           mojom::MediaSessionInfoPtr session_info,
           mojom::AudioFocusType audio_focus_type,
           RequestId id,
           const std::string& source_name)
      : id_(id),
        source_name_(source_name),
        metrics_helper_(source_name),
        session_(std::move(session)),
        session_info_(std::move(session_info)),
        audio_focus_type_(audio_focus_type),
        binding_(this, std::move(request)),
        owner_(owner) {
    // Listen for mojo errors.
    binding_.set_connection_error_handler(
        base::BindOnce(&AudioFocusManager::StackRow::OnConnectionError,
                       base::Unretained(this)));
    session_.set_connection_error_handler(
        base::BindOnce(&AudioFocusManager::StackRow::OnConnectionError,
                       base::Unretained(this)));

    metrics_helper_.OnRequestAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kInitial,
        audio_focus_type);
  }

  ~StackRow() override = default;

  // mojom::AudioFocusRequestClient.
  void RequestAudioFocus(mojom::MediaSessionInfoPtr session_info,
                         mojom::AudioFocusType type,
                         RequestAudioFocusCallback callback) override {
    session_info_ = std::move(session_info);

    if (IsActive() && owner_->IsSessionOnTopOfAudioFocusStack(id(), type)) {
      // Early returning if |media_session| is already on top (has focus) and is
      // active.
      std::move(callback).Run();
      return;
    }

    // Remove this StackRow for the audio focus stack.
    std::unique_ptr<StackRow> row = owner_->RemoveFocusEntryIfPresent(id());
    DCHECK(row);

    owner_->RequestAudioFocusInternal(std::move(row), type,
                                      std::move(callback));

    metrics_helper_.OnRequestAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kUpdate,
        audio_focus_type_);
  }

  void AbandonAudioFocus() override {
    metrics_helper_.OnAbandonAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::kAPI);

    owner_->AbandonAudioFocusInternal(id_);
  }

  void MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) override {
    session_info_ = std::move(info);
  }

  void GetRequestId(GetRequestIdCallback callback) override {
    std::move(callback).Run(id());
  }

  mojom::MediaSession* session() { return session_.get(); }
  const mojom::MediaSessionInfoPtr& info() const { return session_info_; }
  mojom::AudioFocusType audio_focus_type() const { return audio_focus_type_; }

  void SetAudioFocusType(mojom::AudioFocusType type) {
    audio_focus_type_ = type;
  }

  bool IsActive() const {
    return session_info_->state ==
           mojom::MediaSessionInfo::SessionState::kActive;
  }

  RequestId id() const { return id_; }

  const std::string& source_name() const { return source_name_; }

 private:
  void OnConnectionError() {
    // Since we have multiple pathways that can call |OnConnectionError| we
    // should use the |encountered_error_| bit to make sure we abandon focus
    // just the first time.
    if (encountered_error_)
      return;
    encountered_error_ = true;

    metrics_helper_.OnAbandonAudioFocus(
        AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::
            kConnectionError);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AudioFocusManager::AbandonAudioFocusInternal,
                                  base::Unretained(owner_), id_));
  }

  const RequestId id_;
  const std::string source_name_;

  AudioFocusManagerMetricsHelper metrics_helper_;
  bool encountered_error_ = false;

  mojom::MediaSessionPtr session_;
  mojom::MediaSessionInfoPtr session_info_;
  mojom::AudioFocusType audio_focus_type_;
  mojo::Binding<mojom::AudioFocusRequestClient> binding_;

  // Weak pointer to the owning |AudioFocusManager| instance.
  AudioFocusManager* owner_;

  DISALLOW_COPY_AND_ASSIGN(StackRow);
};

void AudioFocusManager::RequestAudioFocus(
    mojom::AudioFocusRequestClientRequest request,
    mojom::MediaSessionPtr media_session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    RequestAudioFocusCallback callback) {
  RequestAudioFocusInternal(
      std::make_unique<StackRow>(
          this, std::move(request), std::move(media_session),
          std::move(session_info), type, base::UnguessableToken::Create(),
          GetBindingSourceName()),
      type, std::move(callback));
}

void AudioFocusManager::GetFocusRequests(GetFocusRequestsCallback callback) {
  std::vector<mojom::AudioFocusRequestStatePtr> requests;

  for (const auto& row : audio_focus_stack_) {
    auto request = mojom::AudioFocusRequestState::New();
    request->session_info = row->info().Clone();
    request->audio_focus_type = row->audio_focus_type();
    request->request_id = row->id();
    request->source_name = row->source_name();
    requests.push_back(std::move(request));
  }

  std::move(callback).Run(std::move(requests));
}

void AudioFocusManager::GetDebugInfoForRequest(
    const RequestId& request_id,
    GetDebugInfoForRequestCallback callback) {
  for (auto& row : audio_focus_stack_) {
    if (row->id() != request_id)
      continue;

    row->session()->GetDebugInfo(std::move(callback));
    return;
  }

  std::move(callback).Run(mojom::MediaSessionDebugInfo::New());
}

void AudioFocusManager::AbandonAudioFocusInternal(RequestId id) {
  if (audio_focus_stack_.empty())
    return;

  if (audio_focus_stack_.back()->id() != id) {
    RemoveFocusEntryIfPresent(id);
    return;
  }

  auto row = std::move(audio_focus_stack_.back());
  audio_focus_stack_.pop_back();

  if (audio_focus_stack_.empty()) {
    // Notify observers that we lost audio focus.
    observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
      observer->OnFocusLost(row->info().Clone());
    });

    DidChangeFocus();
    return;
  }

  if (IsAudioFocusEnforcementEnabled())
    EnforceAudioFocusAbandon(row->audio_focus_type());

  DidChangeFocus();

  // Notify observers that we lost audio focus.
  observers_.ForAllPtrs([&row](mojom::AudioFocusObserver* observer) {
    observer->OnFocusLost(row->info().Clone());
  });

  // Notify observers that the session on top gained focus.
  StackRow* new_session = audio_focus_stack_.back().get();
  observers_.ForAllPtrs([&new_session](mojom::AudioFocusObserver* observer) {
    observer->OnFocusGained(new_session->info().Clone(),
                            new_session->audio_focus_type());
  });
}

void AudioFocusManager::AddObserver(mojom::AudioFocusObserverPtr observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddPtr(std::move(observer));
}

void AudioFocusManager::SetSourceName(const std::string& name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.dispatch_context()->source_name = name;
}

void AudioFocusManager::BindToInterface(
    mojom::AudioFocusManagerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.AddBinding(this, std::move(request),
                       std::make_unique<BindingContext>());
}

void AudioFocusManager::BindToDebugInterface(
    mojom::AudioFocusManagerDebugRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  debug_bindings_.AddBinding(this, std::move(request));
}

void AudioFocusManager::BindToActiveControllerInterface(
    mojom::MediaControllerRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  active_media_controller_.BindToInterface(std::move(request));
}

void AudioFocusManager::RequestAudioFocusInternal(
    std::unique_ptr<StackRow> row,
    mojom::AudioFocusType type,
    base::OnceCallback<void()> callback) {
  // If audio focus is enabled then we should enforce this request and make sure
  // the new active session is not ducking.
  if (IsAudioFocusEnforcementEnabled()) {
    EnforceAudioFocusRequest(type);
    row->session()->StopDucking();
  }

  row->SetAudioFocusType(type);
  audio_focus_stack_.push_back(std::move(row));

  DidChangeFocus();

  // Notify observers that we were gained audio focus.
  mojom::MediaSessionInfoPtr session_info =
      audio_focus_stack_.back()->info().Clone();
  observers_.ForAllPtrs(
      [&session_info, type](mojom::AudioFocusObserver* observer) {
        observer->OnFocusGained(session_info.Clone(), type);
      });

  // We always grant the audio focus request but this may not always be the case
  // in the future.
  std::move(callback).Run();
}

void AudioFocusManager::EnforceAudioFocusRequest(mojom::AudioFocusType type) {
  DCHECK(IsAudioFocusEnforcementEnabled());

  for (auto& old_session : audio_focus_stack_) {
    // If the session has the force duck flag set then we should always duck it.
    if (old_session->info()->force_duck) {
      old_session->session()->StartDucking();
      continue;
    }

    switch (type) {
      case mojom::AudioFocusType::kGain:
      case mojom::AudioFocusType::kGainTransient:
        old_session->session()->Suspend(
            mojom::MediaSession::SuspendType::kSystem);
        break;
      case mojom::AudioFocusType::kGainTransientMayDuck:
        old_session->session()->StartDucking();
        break;
    }
  }
}

void AudioFocusManager::EnforceAudioFocusAbandon(mojom::AudioFocusType type) {
  DCHECK(IsAudioFocusEnforcementEnabled());

  // Allow the top-most MediaSession having force duck to unduck even if
  // it is not active.
  for (auto iter = audio_focus_stack_.rbegin();
       iter != audio_focus_stack_.rend(); ++iter) {
    if (!(*iter)->info()->force_duck)
      continue;

    // TODO(beccahughes): Replace with std::rotate.
    auto duck_row = std::move(*iter);
    duck_row->session()->StopDucking();
    audio_focus_stack_.erase(std::next(iter).base());
    audio_focus_stack_.push_back(std::move(duck_row));
    return;
  }

  DCHECK(!audio_focus_stack_.empty());
  StackRow* top = audio_focus_stack_.back().get();

  switch (type) {
    case mojom::AudioFocusType::kGain:
      // Do nothing. The abandoned session suspended all the media sessions and
      // they should stay suspended to avoid surprising the user.
      break;
    case mojom::AudioFocusType::kGainTransient:
      // The abandoned session suspended all the media sessions but we should
      // start playing the top one again as the abandoned media was transient.
      top->session()->Resume(mojom::MediaSession::SuspendType::kSystem);
      break;
    case mojom::AudioFocusType::kGainTransientMayDuck:
      // The abandoned session ducked all the media sessions so we should unduck
      // them. If they are not playing then they will not resume.
      for (auto& session : base::Reversed(audio_focus_stack_)) {
        session->session()->StopDucking();

        // If the new session is ducking then we should continue ducking all but
        // the new session.
        if (top->audio_focus_type() ==
            mojom::AudioFocusType::kGainTransientMayDuck)
          break;
      }
      break;
  }
}

void AudioFocusManager::DidChangeFocus() {
  if (audio_focus_stack_.empty()) {
    active_media_controller_.ClearMediaSession();
  } else {
    active_media_controller_.SetMediaSession(
        audio_focus_stack_.back()->session());
  }
}

AudioFocusManager::AudioFocusManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AudioFocusManager::~AudioFocusManager() = default;

std::unique_ptr<AudioFocusManager::StackRow>
AudioFocusManager::RemoveFocusEntryIfPresent(RequestId id) {
  std::unique_ptr<StackRow> row;

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

const std::string& AudioFocusManager::GetBindingSourceName() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return bindings_.dispatch_context()->source_name;
}

bool AudioFocusManager::IsSessionOnTopOfAudioFocusStack(
    RequestId id,
    mojom::AudioFocusType type) const {
  return !audio_focus_stack_.empty() && audio_focus_stack_.back()->id() == id &&
         audio_focus_stack_.back()->audio_focus_type() == type;
}

}  // namespace media_session
