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
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/to_string.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

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

// Returns whether `response` contains a valid `AnnotatedPageContent` payload.
bool HasAnnotatedPageContent(
    const PageContextWrapperCallbackResponse& response) {
  return response.has_value() && response.value() &&
         response.value()->has_annotated_page_content();
}

// Returns whether `response` failed because the page's context is not eligible
// to be shared with the backend (unsupported scheme/MIME type, unsafe page, or
// force-detached context).
bool IsPageContextIneligible(
    const PageContextWrapperCallbackResponse& response) {
  if (response.has_value()) {
    return false;
  }
  switch (response.error()) {
    case PageContextWrapperError::kPageNotExtractableError:
    case PageContextWrapperError::kPageUnsafeError:
    case PageContextWrapperError::kForceDetachError:
      return true;
    case PageContextWrapperError::kGenericError:
    case PageContextWrapperError::kAPCError:
    case PageContextWrapperError::kScreenshotError:
    case PageContextWrapperError::kPDFDataError:
    case PageContextWrapperError::kTimeout:
    case PageContextWrapperError::kInnerTextError:
      return false;
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
  base::WeakPtr<TtcSessionController> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  SetLifecycle(TTCSessionLifecycle::kLive);
  if (!weak_this || lifecycle_ != TTCSessionLifecycle::kLive) {
    return;
  }
  if (conversation_->IsMicMuted()) {
    OnConversationMicMutedChanged(true);
    if (!weak_this || lifecycle_ != TTCSessionLifecycle::kLive) {
      return;
    }
  }
  // Starts tracking and extracting page context only after session setup and
  // toolset registration have completed, so `ContextUpdate` is never queued
  // ahead of `SessionSetupRequest` or `ToolsetUpdate`.
  if (voice_focused_web_state_tracker_ &&
      !voice_focused_web_state_tracker_observation_.IsObserving()) {
    voice_focused_web_state_tracker_observation_.Observe(
        voice_focused_web_state_tracker_.get());
    OnVoiceFocusedWebStateChanged(
        voice_focused_web_state_tracker_->GetActiveWebState());
  }
}

void TtcSessionController::OnConversationClosed() {
  StopSessionAsync();
}

void TtcSessionController::OnConversationMicMutedChanged(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kLive ||
      notified_mic_muted_ == muted) {
    return;
  }
  notified_mic_muted_ = muted;
  LogMicMuted(muted);
  for (TtcSessionControllerObserver& observer : observers_) {
    observer.OnMicMutedChanged(this, muted);
  }
}

void TtcSessionController::OnAudioEnergyUpdated(float energy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished) {
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
    StopSessionAsync();
  }
}

void TtcSessionController::OnToolCall(
    const ttc::ToolRequest& tool_request,
    ttc::ToolResponseCallback tool_response_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ == TTCSessionLifecycle::kFinished || !actuation_handler_) {
    if (lifecycle_ != TTCSessionLifecycle::kFinished) {
      LogActuationUnavailable(tool_request.name);
    }
    if (tool_response_callback) {
      std::move(tool_response_callback)
          .Run(ttc::ToolResponse::Error(
              actor::mojom::ActionResultCode::kExecutorDestroyed));
    }
    return;
  }

  Browser* browser = voice_focused_web_state_tracker_
                         ? voice_focused_web_state_tracker_->GetBrowser()
                         : nullptr;
  actuation_handler_->ProcessToolCall(tool_request, GetVoiceFocusedWebState(),
                                      browser,
                                      std::move(tool_response_callback));
}

#pragma mark - TtcVoiceFocusedWebStateTracker::Observer

void TtcSessionController::OnVoiceFocusedWebStateChanged(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kLive) {
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
  if (!web_state) {
    conversation_->SendContextUpdate(GURL(), /*title=*/std::string(),
                                     /*apc=*/{});
    return;
  }
  page_context_monitor_ =
      std::make_unique<TtcPageContextMonitor>(*web_state, this, journal_.get());
}

#pragma mark - TtcPageContextMonitor::Delegate

void TtcSessionController::OnPageContextFetched(
    PageContextWrapperCallbackResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kLive) {
    return;
  }
  if (!HasAnnotatedPageContent(response)) {
    web::WebState* web_state = GetVoiceFocusedWebState();
    if (!web_state) {
      return;
    }
    // Send a tab-only update with empty `AnnotatedPageContent` so the backend
    // clears stale page content from the previous page. Only share the origin
    // when the page's context is not eligible to be shared.
    const GURL& committed_url = web_state->GetLastCommittedURL();
    if (IsPageContextIneligible(response)) {
      conversation_->SendContextUpdate(
          url::Origin::Create(committed_url).GetURL(),
          /*title=*/std::string(), /*apc=*/{});
      return;
    }
    conversation_->SendContextUpdate(
        committed_url, base::UTF16ToUTF8(web_state->GetTitle()), /*apc=*/{});
    return;
  }
  const optimization_guide::proto::PageContext& page_context =
      *response.value();
  const optimization_guide::proto::AnnotatedPageContent& apc =
      page_context.annotated_page_content();
  const optimization_guide::proto::FrameData& main_frame_data =
      apc.main_frame_data();
  // Fall back to top-level `PageContext` metadata when `main_frame_data`
  // fields are empty.
  const std::string& main_frame_url = main_frame_data.url();
  const GURL url =
      main_frame_url.empty() ? GURL(page_context.url()) : GURL(main_frame_url);
  const std::string& main_frame_title = main_frame_data.title();
  const std::string& title =
      main_frame_title.empty() ? page_context.title() : main_frame_title;
  conversation_->SendContextUpdate(url, title, apc);
}

#pragma mark - Initializers

TtcSessionController::TtcSessionController(
    std::unique_ptr<TtcConversation> conversation,
    std::unique_ptr<TtcVoiceFocusedWebStateTracker>
        voice_focused_web_state_tracker,
    std::unique_ptr<TtcActuationHandler> actuation_handler)
    : conversation_(std::move(conversation)),
      actuation_handler_(std::move(actuation_handler)),
      voice_focused_web_state_tracker_(
          std::move(voice_focused_web_state_tracker)) {
  CHECK(conversation_);
  conversation_->set_delegate(this);
  if (actuation_handler_) {
    actuation_handler_->SetCloseSessionCallback(
        base::BindRepeating(&TtcSessionController::StopSessionAsync,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  RegisterBackgroundObserver();
}

TtcSessionController::TtcSessionController(
    std::unique_ptr<ttc::TtcBackend> backend,
    BrowserList* browser_list,
    std::unique_ptr<TtcActuationHandler> actuation_handler)
    : TtcSessionController(
          std::make_unique<TtcConversation>(
              [[TTCAudioSessionController alloc] init],
              std::move(backend)),
          browser_list
              ? std::make_unique<TtcVoiceFocusedWebStateTracker>(browser_list)
              : nullptr,
          std::move(actuation_handler)) {}

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
  if (actuation_handler_) {
    actuation_handler_->set_journal(journal);
  }
  if (page_context_monitor_) {
    page_context_monitor_->set_journal(journal);
  }
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
}

bool TtcSessionController::SetMicMuted(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lifecycle_ != TTCSessionLifecycle::kLive ||
      conversation_->IsMicMuted() == muted) {
    return false;
  }
  base::WeakPtr<TtcSessionController> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  conversation_->SetMicMuted(muted);
  return weak_this && lifecycle_ == TTCSessionLifecycle::kLive &&
         conversation_->IsMicMuted() == muted;
}

bool TtcSessionController::IsMicMuted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_ == TTCSessionLifecycle::kLive &&
         conversation_->IsMicMuted();
}

void TtcSessionController::StopSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_) {
    return;
  }
  is_stopped_ = true;
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
  if (actuation_handler_) {
    actuation_handler_->StopTask(actor::ActorTaskStoppedReason::kStoppedByUser);
  }
  SetLifecycle(TTCSessionLifecycle::kFinished);
}

void TtcSessionController::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopSession();
  if (actuation_handler_) {
    actuation_handler_->Disconnect();
    actuation_handler_.reset();
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  conversation_->Disconnect();
  RemoveBackgroundObserver();
  observers_.Clear();
  journal_.reset();
}

void TtcSessionController::StopSessionAsync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetLifecycle(TTCSessionLifecycle::kFinished);
  // Stopping the conversation closes the backend and audio session, so it must
  // be deferred asynchronously to avoid tearing down `TtcConversation` or
  // `TtcBackend` while a delegate callback is still on the stack.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TtcSessionController::StopSession,
                                weak_ptr_factory_.GetWeakPtr()));
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

void TtcSessionController::LogMicMuted(bool muted) {
  if (!journal_) {
    return;
  }
  journal_->Log(
      TTCJournalComponent::kSession, "TtcMicMuted",
      TTCJournalDetailsBuilder()
          .SetSummary(muted ? "Microphone muted" : "Microphone unmuted")
          .Add("muted", muted)
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

void TtcSessionController::LogActuationUnavailable(
    const std::string& tool_name) {
  if (!journal_) {
    return;
  }
  journal_->LogError(
      TTCJournalComponent::kActuation, "ActuationDispatchRejected",
      TTCJournalDetailsBuilder()
          .SetSummary("Tool rejected: Actuation service not available")
          .Add("tool_name", tool_name)
          .Add("result_code",
               static_cast<int64_t>(
                   actor::mojom::ActionResultCode::kExecutorDestroyed))
          .AddError("Actuation service not available")
          .Build());
}
