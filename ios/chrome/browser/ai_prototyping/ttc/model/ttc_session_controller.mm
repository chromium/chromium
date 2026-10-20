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
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kInitializing) {
    return;
  }
  conversation_->SendToolSetUpdate(ttc::GetDefaultToolDefinitions());
  SetLifecycle(TTCSessionLifecycle::kLive);
}

void TtcSessionController::OnConversationClosed() {
  StopSession();
}

void TtcSessionController::OnAudioEnergyUpdated(float energy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished || observers_.empty()) {
    return;
  }
  for (TtcSessionControllerObserver& observer : observers_) {
    observer.OnUserAudioLevelUpdated(this, energy);
  }
}

void TtcSessionController::OnConversationError(ttc::ErrorCode error) {
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

#pragma mark - TtcVoiceFocusedWebStateTracker::Observer

void TtcSessionController::OnVoiceFocusedWebStateChanged(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  web::WebState* current_web_state =
      page_context_monitor_ ? page_context_monitor_->web_state() : nullptr;
  if (current_web_state == web_state) {
    return;
  }
  if (page_context_monitor_) {
    page_context_monitor_->Disconnect();
    page_context_monitor_.reset();
  }
  // TODO(b/555804152): Signal to the backend when page context is invalidated
  // or cannot be extracted for the focused WebState.
  if (web_state) {
    page_context_monitor_ =
        std::make_unique<TtcPageContextMonitor>(*web_state, this);
  }
}

#pragma mark - TtcPageContextMonitor::Delegate

void TtcSessionController::OnPageContextFetched(
    PageContextWrapperCallbackResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished || !response.has_value()) {
    return;
  }
  const optimization_guide::proto::PageContext* page_context =
      response.value().get();
  if (!page_context || !page_context->has_annotated_page_content()) {
    return;
  }
  const optimization_guide::proto::AnnotatedPageContent& apc =
      page_context->annotated_page_content();
  const optimization_guide::proto::FrameData& main_frame_data =
      apc.main_frame_data();
  // Fall back to top-level `PageContext` metadata when `main_frame_data`
  // fields are empty.
  const std::string& main_frame_url = main_frame_data.url();
  const GURL url =
      main_frame_url.empty() ? GURL(page_context->url()) : GURL(main_frame_url);
  const std::string& main_frame_title = main_frame_data.title();
  const std::string& title =
      main_frame_title.empty() ? page_context->title() : main_frame_title;
  conversation_->SendContextUpdate(url, title, apc);
}

#pragma mark - Initializers

TtcSessionController::TtcSessionController(
    std::unique_ptr<TtcConversation> conversation,
    std::unique_ptr<TtcVoiceFocusedWebStateTracker>
        voice_focused_web_state_tracker)
    : conversation_(std::move(conversation)),
      voice_focused_web_state_tracker_(
          std::move(voice_focused_web_state_tracker)) {
  CHECK(conversation_);
  conversation_->set_delegate(this);
  RegisterBackgroundObserver();
}

TtcSessionController::TtcSessionController(
    std::unique_ptr<ttc::TtcBackend> backend,
    BrowserList* browser_list)
    : TtcSessionController(
          std::make_unique<TtcConversation>(
              [[TTCAudioSessionController alloc] init],
              std::move(backend)),
          browser_list
              ? std::make_unique<TtcVoiceFocusedWebStateTracker>(browser_list)
              : nullptr) {}

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

web::WebState* TtcSessionController::GetVoiceFocusedWebState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return voice_focused_web_state_tracker_
             ? voice_focused_web_state_tracker_->GetActiveWebState()
             : nullptr;
}

#pragma mark - Session Lifecycle

void TtcSessionController::StartSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  conversation_->Start();
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
    return;
  }
  // The backend queues outgoing frames until the connection is established, so
  // page context extraction can start before initialization completes.
  if (voice_focused_web_state_tracker_ &&
      !voice_focused_web_state_tracker_observation_.IsObserving()) {
    voice_focused_web_state_tracker_observation_.Observe(
        voice_focused_web_state_tracker_.get());
    OnVoiceFocusedWebStateChanged(
        voice_focused_web_state_tracker_->GetActiveWebState());
  }
}

void TtcSessionController::StopSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_context_monitor_) {
    page_context_monitor_->Disconnect();
    page_context_monitor_.reset();
  }
  voice_focused_web_state_tracker_observation_.Reset();
  if (voice_focused_web_state_tracker_) {
    voice_focused_web_state_tracker_->Disconnect();
    voice_focused_web_state_tracker_.reset();
  }
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
