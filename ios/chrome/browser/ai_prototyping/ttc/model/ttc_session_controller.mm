// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/to_string.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"

namespace {

// Returns the string representation of `lifecycle`.
std::string SessionLifecycleToString(TTCSessionLifecycle lifecycle) {
  switch (lifecycle) {
    case TTCSessionLifecycle::kInitializing:
      return "initializing";
    case TTCSessionLifecycle::kLive:
      return "live";
    case TTCSessionLifecycle::kFinished:
      return "finished";
  }
}

}  // namespace

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

#pragma mark - Properties

TTCJournal* TtcSessionController::journal() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return journal_.get();
}

void TtcSessionController::set_journal(TTCJournal* journal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  journal_ = journal ? journal->GetWeakPtr() : nullptr;
  conversation_->set_journal(journal);
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
  journal_.reset();
}

void TtcSessionController::SetLifecycle(TTCSessionLifecycle lifecycle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == lifecycle || lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  LogLifecycleTransition(lifecycle_, lifecycle);
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
  LogSessionError(error);
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
              usingBlock:
                  base::CallbackToBlock(
                      base::IgnoreArgs<NSNotification*>(base::BindRepeating(
                          &TtcSessionController::ApplicationDidEnterBackground,
                          weak_ptr_factory_.GetWeakPtr())))];
}

void TtcSessionController::ApplicationDidEnterBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  LogAppDidEnterBackground();
  StopSession();
}

void TtcSessionController::RemoveBackgroundObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (background_observer_) {
    [[NSNotificationCenter defaultCenter] removeObserver:background_observer_];
    background_observer_ = nil;
  }
}

#pragma mark - Journal Logging

void TtcSessionController::LogLifecycleTransition(
    TTCSessionLifecycle current_lifecycle,
    TTCSessionLifecycle new_lifecycle) {
  if (!journal_) {
    return;
  }
  std::string current_state = SessionLifecycleToString(current_lifecycle);
  std::string new_state = SessionLifecycleToString(new_lifecycle);
  journal_->Log(TTCJournalComponent::kSession, "TtcSessionLifecycle",
                TTCJournalDetailsBuilder()
                    .SetSummary("Session " + new_state)
                    .Add("current_state", current_state)
                    .Add("new_state", new_state)
                    .Build());
}

void TtcSessionController::LogSessionError(ttc::ErrorCode error) {
  if (!journal_) {
    return;
  }
  const int error_code = static_cast<int>(error);
  std::string desc = base::ToString(error);
  std::string summary = "Session error";
  if (!desc.empty()) {
    summary += ": " + desc;
  }
  journal_->LogError(TTCJournalComponent::kSession, "TtcSessionError",
                     TTCJournalDetailsBuilder()
                         .SetSummary(summary)
                         .AddError(desc)
                         .Add("error_code", error_code)
                         .Build());
}

void TtcSessionController::LogAppDidEnterBackground() {
  if (!journal_) {
    return;
  }
  journal_->Log(
      TTCJournalComponent::kSession, "TtcAppDidEnterBackground",
      TTCJournalDetailsBuilder().SetSummary("App entered background").Build());
}
