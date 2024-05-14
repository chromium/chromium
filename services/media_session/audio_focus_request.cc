// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_request.h"

#include "base/task/single_thread_task_runner.h"
#include "services/media_session/audio_focus_manager.h"

namespace media_session {

AudioFocusRequest::AudioFocusRequest(
    base::WeakPtr<AudioFocusManager> owner,
    mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
    mojo::PendingRemote<mojom::MediaSession> session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType audio_focus_type,
    const base::UnguessableToken& id,
    const std::string& source_name,
    const base::UnguessableToken& group_id,
    const base::UnguessableToken& identity)
    : session_(std::move(session)),
      session_info_(std::move(session_info)),
      audio_focus_type_(audio_focus_type),
      receiver_(this, std::move(receiver)),
      id_(id),
      source_name_(source_name),
      group_id_(group_id),
      identity_(identity),
      owner_(std::move(owner)) {
  // Listen for mojo errors.
  receiver_.set_disconnect_handler(base::BindOnce(
      &AudioFocusRequest::OnConnectionError, base::Unretained(this)));
  session_.set_disconnect_handler(base::BindOnce(
      &AudioFocusRequest::OnConnectionError, base::Unretained(this)));
}

AudioFocusRequest::~AudioFocusRequest() = default;

void AudioFocusRequest::RequestAudioFocus(
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    RequestAudioFocusCallback callback) {
  SetSessionInfo(std::move(session_info));

  if (session_info_->state == mojom::MediaSessionInfo::SessionState::kActive &&
      owner_->IsSessionOnTopOfAudioFocusStack(id(), type)) {
    // Return early is this session is already on top of the stack.
    std::move(callback).Run();
    return;
  }

  // Remove this AudioFocusRequest for the audio focus stack.
  std::unique_ptr<AudioFocusRequest> row =
      owner_->RemoveFocusEntryIfPresent(id());
  DCHECK(row);

  owner_->RequestAudioFocusInternal(std::move(row), type);

  std::move(callback).Run();
}

void AudioFocusRequest::AbandonAudioFocus() {
  owner_->AbandonAudioFocusInternal(id_);
}

void AudioFocusRequest::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr info) {
  bool suspended_change =
      (info->state == mojom::MediaSessionInfo::SessionState::kSuspended ||
       IsSuspended()) &&
      info->state != session_info_->state;

  SetSessionInfo(std::move(info));

  // If we have transitioned to/from a suspended state then we should
  // re-enforce audio focus.
  if (suspended_change) {
    owner_->EnforceAudioFocus();
  }
}

bool AudioFocusRequest::IsSuspended() const {
  return session_info_->state ==
         mojom::MediaSessionInfo::SessionState::kSuspended;
}

mojom::AudioFocusRequestStatePtr AudioFocusRequest::ToAudioFocusRequestState()
    const {
  auto request = mojom::AudioFocusRequestState::New();
  request->session_info = session_info_.Clone();
  request->audio_focus_type = audio_focus_type_;
  request->request_id = id_;
  request->source_name = source_name_;
  request->source_id =
      identity_.is_empty() ? std::nullopt : std::make_optional(identity_);
  return request;
}

void AudioFocusRequest::BindToMediaController(
    mojo::PendingReceiver<mojom::MediaController> receiver) {
  if (!controller_) {
    controller_ = std::make_unique<MediaController>();
    controller_->SetMediaSession(this);
  }

  controller_->BindToInterface(std::move(receiver));
}

void AudioFocusRequest::Suspend(const EnforcementState& state) {
  DCHECK(!session_info_->force_duck);

  // In most cases if we stop or suspend we should call the ::Suspend method
  // on the media session. The only exception is if the session has the
  // |prefer_stop_for_gain_focus_loss| bit set in which case we should use
  // ::Stop and ::Suspend respectively.
  if (state.should_stop && session_info_->prefer_stop_for_gain_focus_loss) {
    session_->Stop(mojom::MediaSession::SuspendType::kSystem);
  } else {
    was_suspended_ = was_suspended_ || state.should_suspend;
    session_->Suspend(mojom::MediaSession::SuspendType::kSystem);
  }
}

void AudioFocusRequest::ReleaseTransientHold() {
  DCHECK(!session_info_->force_duck);

  if (!was_suspended_) {
    return;
  }

  was_suspended_ = false;

  if (delayed_action_) {
    PerformUIAction(*delayed_action_);
    delayed_action_.reset();
    return;
  }

  session_->Resume(mojom::MediaSession::SuspendType::kSystem);
}

void AudioFocusRequest::PerformUIAction(mojom::MediaSessionAction action) {
  // If the session was temporarily suspended by the service then we should
  // delay the action until the session is resumed.
  if (was_suspended_) {
    delayed_action_ = action;
    return;
  }

  switch (action) {
    case mojom::MediaSessionAction::kPause:
      session_->Suspend(mojom::MediaSession::SuspendType::kUI);
      break;
    case mojom::MediaSessionAction::kPlay:
      session_->Resume(mojom::MediaSession::SuspendType::kUI);
      break;
    case mojom::MediaSessionAction::kStop:
      session_->Stop(mojom::MediaSession::SuspendType::kUI);
      break;
    default:
      // Only UI transport actions are supported.
      NOTREACHED_IN_MIGRATION();
  }
}

void AudioFocusRequest::GetMediaImageBitmap(
    const MediaImage& image,
    int minimum_size_px,
    int desired_size_px,
    GetMediaImageBitmapCallback callback) {
  session_->GetMediaImageBitmap(
      image, minimum_size_px, desired_size_px,
      base::BindOnce(&AudioFocusRequest::OnImageDownloaded,
                     base::Unretained(this), std::move(callback)));
}

void AudioFocusRequest::SetSessionInfo(
    mojom::MediaSessionInfoPtr session_info) {
  bool is_controllable_changed =
      session_info_->is_controllable != session_info->is_controllable;

  session_info_ = std::move(session_info);

  if (is_controllable_changed) {
    owner_->MaybeUpdateActiveSession();
  }
}

void AudioFocusRequest::OnConnectionError() {
  // Since we have multiple pathways that can call |OnConnectionError| we
  // should use the |encountered_error_| bit to make sure we abandon focus
  // just the first time.
  if (encountered_error_) {
    return;
  }

  encountered_error_ = true;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AudioFocusManager::AbandonAudioFocusInternal,
                                owner_, id_));
}

void AudioFocusRequest::OnImageDownloaded(GetMediaImageBitmapCallback callback,
                                          const SkBitmap& bitmap) {
  std::move(callback).Run(bitmap);
}

}  // namespace media_session
