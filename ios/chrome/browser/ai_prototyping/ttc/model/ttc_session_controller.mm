// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"

#pragma mark - TtcConversation::Delegate

void TtcSessionController::OnConversationInitialized() {
  OnSessionInitialized();
}

void TtcSessionController::OnConversationClosed() {
  StopSession();
}

void TtcSessionController::OnAudioEnergyUpdated(float energy) {
  UserAudioLevelDidUpdate(energy);
}

void TtcSessionController::OnConversationError(ttc::ErrorCode error) {
  FailWithError(error);
}

#pragma mark - Initializers

TtcSessionController::TtcSessionController(
    std::unique_ptr<TtcConversation> conversation)
    : conversation_(std::move(conversation)) {
  CHECK(conversation_);
  conversation_->set_delegate(this);
  RegisterBackgroundObserver();
}

TtcSessionController::TtcSessionController(
    std::unique_ptr<ttc::TtcBackend> backend)
    : TtcSessionController(std::make_unique<TtcConversation>(
          [[TTCAudioSessionController alloc] init],
          std::move(backend))) {}

TtcSessionController::TtcSessionController()
    : TtcSessionController(std::make_unique<TtcConversation>()) {}

TtcSessionController::~TtcSessionController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
}

#pragma mark - Session Lifecycle

void TtcSessionController::StartSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  conversation_->Start();
}

void TtcSessionController::OnSessionInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kInitializing) {
    return;
  }
  SetLifecycle(TTCSessionLifecycle::kLive);
}

void TtcSessionController::StopSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  conversation_->Stop();
  SetLifecycle(TTCSessionLifecycle::kFinished);
}

void TtcSessionController::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopSession();
  weak_ptr_factory_.InvalidateWeakPtrs();
  conversation_->Disconnect();
  RemoveBackgroundObserver();
  observers_.Clear();
}

void TtcSessionController::SetLifecycle(TTCSessionLifecycle lifecycle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == lifecycle || lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  lifecycle_ = lifecycle;
  for (TtcSessionControllerObserver& observer : observers_) {
    observer.OnSessionLifecycleChanged(this, lifecycle_);
  }
}

#pragma mark - Audio & Errors

void TtcSessionController::UserAudioLevelDidUpdate(float audio_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished || observers_.empty()) {
    return;
  }
  for (TtcSessionControllerObserver& observer : observers_) {
    observer.OnUserAudioLevelUpdated(this, audio_level);
  }
}

void TtcSessionController::FailWithError(ttc::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished || fatal_error_reported_) {
    return;
  }
  const bool is_fatal = ttc::IsFatal(error);
  if (is_fatal) {
    fatal_error_reported_ = true;
  }
  for (TtcSessionControllerObserver& observer : observers_) {
    observer.OnSessionError(this, error);
  }
  if (is_fatal) {
    StopSession();
  }
}

#pragma mark - Observers

void TtcSessionController::AddObserver(TtcSessionControllerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer) {
    return;
  }
  observers_.AddObserver(observer);
}

void TtcSessionController::RemoveObserver(
    TtcSessionControllerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer) {
    return;
  }
  observers_.RemoveObserver(observer);
}

base::WeakPtr<TtcSessionController> TtcSessionController::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

#pragma mark - Private

void TtcSessionController::RegisterBackgroundObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(
                             base::IgnoreArgs<NSNotification*>(
                                 base::BindRepeating(
                                     &TtcSessionController::StopSession,
                                     weak_ptr_factory_.GetWeakPtr())))];
}

void TtcSessionController::RemoveBackgroundObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (background_observer_) {
    [[NSNotificationCenter defaultCenter] removeObserver:background_observer_];
    background_observer_ = nil;
  }
}
