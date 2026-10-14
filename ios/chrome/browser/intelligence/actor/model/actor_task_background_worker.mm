// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"

#import <algorithm>
#import <string_view>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace actor {

namespace {

// Minimal zero side effects script executed to generate IPC activity.
constexpr char16_t kHeartbeatScript[] = u";";

// Returns whether `task_state` should keep a background task alive.
bool ShouldKeepBackgroundTaskAlive(ActorTaskState task_state) {
  switch (task_state) {
    case ActorTaskState::kInit:
    case ActorTaskState::kActing:
    case ActorTaskState::kReflecting:
      return true;
    case ActorTaskState::kWaitingOnUser:
    case ActorTaskState::kPausedByActor:
    case ActorTaskState::kPausedByUser:
    case ActorTaskState::kCancelled:
    case ActorTaskState::kFinished:
    case ActorTaskState::kFailed:
      return false;
  }
}

// Returns whether the context is completed with success when the background
// task ends for `reason`.
bool IsSuccessfulEndReason(ActorBackgroundTaskEndReason reason) {
  switch (reason) {
    case ActorBackgroundTaskEndReason::kSucceeded:
    case ActorBackgroundTaskEndReason::kWaitingOnUser:
      return true;
    case ActorBackgroundTaskEndReason::kFailed:
    case ActorBackgroundTaskEndReason::kExpired:
    case ActorBackgroundTaskEndReason::kCancelled:
    case ActorBackgroundTaskEndReason::kDestroyed:
      return false;
  }
}

// Returns the message ID of the background task title describing why the task
// is waiting on the user in `task_state`. Must only be called for non-terminal
// states where `ShouldKeepBackgroundTaskAlive(task_state)` is false.
int WaitingOnUserTitleMessageId(ActorTaskState task_state) {
  switch (task_state) {
    case ActorTaskState::kWaitingOnUser:
      return IDS_IOS_ACTOR_BACKGROUND_TASK_INPUT_NEEDED;
    case ActorTaskState::kPausedByActor:
    case ActorTaskState::kPausedByUser:
      return IDS_IOS_ACTOR_BACKGROUND_TASK_STOPPED;
    case ActorTaskState::kInit:
    case ActorTaskState::kActing:
    case ActorTaskState::kReflecting:
    case ActorTaskState::kCancelled:
    case ActorTaskState::kFinished:
    case ActorTaskState::kFailed:
      NOTREACHED();
  }
}

// Returns the journal name of `reason`.
std::string_view EndReasonToString(ActorBackgroundTaskEndReason reason) {
  switch (reason) {
    case ActorBackgroundTaskEndReason::kSucceeded:
      return "succeeded";
    case ActorBackgroundTaskEndReason::kFailed:
      return "failed";
    case ActorBackgroundTaskEndReason::kWaitingOnUser:
      return "waiting_on_user";
    case ActorBackgroundTaskEndReason::kExpired:
      return "expired";
    case ActorBackgroundTaskEndReason::kCancelled:
      return "cancelled";
    case ActorBackgroundTaskEndReason::kDestroyed:
      return "destroyed";
  }
}

}  // namespace

ActorTaskBackgroundWorker::ActorTaskBackgroundWorker(
    TaskStateDelegate* delegate,
    ActorTaskId task_id,
    AggregatedJournal& journal)
    : delegate_(delegate), task_id_(task_id), journal_(journal) {
  CHECK(delegate_);
}

ActorTaskBackgroundWorker::~ActorTaskBackgroundWorker() {
  heartbeat_timer_.Stop();
  FinalizeBackgroundTask(ActorBackgroundTaskEndReason::kDestroyed);
}

const std::string& ActorTaskBackgroundWorker::title() const {
  return delegate_->GetTaskTitle();
}

base::WeakPtr<ActorTaskBackgroundWorker>
ActorTaskBackgroundWorker::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ActorTaskBackgroundWorker::ShouldRequestBackgroundTask() const {
  const bool is_eligible = GetBackgroundTaskRequestEligibility();
  base::UmaHistogramBoolean(kActorBackgroundTaskRequestEligibilityHistogram,
                            is_eligible);
  return is_eligible;
}

void ActorTaskBackgroundWorker::SetContext(
    BackgroundContinuedProcessingTaskContext* context) {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return;
  }
  background_task_context_ = context;
  background_task_started_at_ = base::TimeTicks::Now();
  LogJournalEvent(*journal_, GURL(), task_id_,
                  "ActorTaskBackgroundWorker::SetContext",
                  {{"status", "background_task_started"}});
  UpdateBackgroundTaskSubtitle(delegate_->GetLastTaskUpdate());
}

void ActorTaskBackgroundWorker::OnWebStateAdded(web::WebState* web_state) {
  CHECK(web_state);
  if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }
  UpdateHeartbeatTimer();
}

void ActorTaskBackgroundWorker::SetAllScenesForegroundActive(
    bool all_scenes_foreground_active) {
  all_scenes_foreground_active_ = all_scenes_foreground_active;
  UpdateHeartbeatTimer();
}

void ActorTaskBackgroundWorker::OnStateChanged(ActorTaskState new_state) {
  UpdateHeartbeatTimer();
  if (new_state == ActorTaskState::kActing) {
    UpdateBackgroundTaskSubtitle(delegate_->GetLastTaskUpdate());
  } else if (!IsTerminalState(new_state) &&
             !ShouldKeepBackgroundTaskAlive(new_state)) {
    // End the background task when waiting on the user (e.g., paused, yielded)
    // to avoid stalling system progress.
    FinalizeBackgroundTask(ActorBackgroundTaskEndReason::kWaitingOnUser);
  }
}

void ActorTaskBackgroundWorker::OnWillExecuteTool() {
  IncrementBackgroundTaskProgress();
}

void ActorTaskBackgroundWorker::OnStopped(bool success) {
  heartbeat_timer_.Stop();
  FinalizeBackgroundTask(success ? ActorBackgroundTaskEndReason::kSucceeded
                                 : ActorBackgroundTaskEndReason::kFailed);
}

#pragma mark - web::WebStateObserver

void ActorTaskBackgroundWorker::WasShown(web::WebState* web_state) {
  UpdateHeartbeatTimer();
}

void ActorTaskBackgroundWorker::WasHidden(web::WebState* web_state) {
  UpdateHeartbeatTimer();
}

void ActorTaskBackgroundWorker::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  UpdateHeartbeatTimer();
}

#pragma mark - Private

bool ActorTaskBackgroundWorker::GetBackgroundTaskRequestEligibility() const {
  bool is_eligible = true;

  if (!IsGeminiActorBackgroundingEnabled()) {
    base::UmaHistogramEnumeration(
        kActorBackgroundTaskIneligibilityReasonHistogram,
        ActorBackgroundTaskIneligibilityReason::kBackgroundingDisabled);
    is_eligible = false;
  }

  if (background_task_context_ && !background_task_context_.completed) {
    base::UmaHistogramEnumeration(
        kActorBackgroundTaskIneligibilityReasonHistogram,
        ActorBackgroundTaskIneligibilityReason::kAlreadyHasContext);
    is_eligible = false;
  }

  if (!ShouldKeepBackgroundTaskAlive(delegate_->GetTaskState())) {
    base::UmaHistogramEnumeration(
        kActorBackgroundTaskIneligibilityReasonHistogram,
        ActorBackgroundTaskIneligibilityReason::kStateDoesNotKeepAlive);
    is_eligible = false;
  }

  return is_eligible;
}

void ActorTaskBackgroundWorker::UpdateBackgroundTaskSubtitle(
    const std::string& task_update) {
  if (!background_task_context_ || task_update.empty()) {
    return;
  }
  NSString* subtitle = base::SysUTF8ToNSString(task_update);
  if ([background_task_context_.subtitle isEqualToString:subtitle]) {
    return;
  }
  background_task_context_.subtitle = subtitle;
}

void ActorTaskBackgroundWorker::IncrementBackgroundTaskProgress() {
  if (background_task_context_) {
    [background_task_context_ incrementStepProgress];
  }
}

void ActorTaskBackgroundWorker::FinalizeBackgroundTask(
    ActorBackgroundTaskEndReason reason) {
  if (!background_task_context_) {
    return;
  }
  BackgroundContinuedProcessingTaskContext* context = background_task_context_;
  background_task_context_ = nil;

  ActorBackgroundTaskEndReason effective_reason = reason;
  if (context.completed) {
    effective_reason = context.cancelled
                           ? ActorBackgroundTaskEndReason::kCancelled
                           : ActorBackgroundTaskEndReason::kExpired;
  } else if (effective_reason == ActorBackgroundTaskEndReason::kWaitingOnUser) {
    // Complete the background task to free up system resources without filling
    // the progress bar. The title is intentionally swapped as subtitle.
    NSString* title = l10n_util::GetNSString(
        WaitingOnUserTitleMessageId(delegate_->GetTaskState()));
    NSString* subtitle = base::SysUTF8ToNSString(delegate_->GetTaskTitle());
    [context updateTitle:title subtitle:subtitle];
    [context setTaskCompletedWithSuccess:IsSuccessfulEndReason(effective_reason)
                            fillProgress:NO];
  } else {
    [context
        setTaskCompletedWithSuccess:IsSuccessfulEndReason(effective_reason)];
  }

  base::UmaHistogramEnumeration(kActorBackgroundTaskEndReasonHistogram,
                                effective_reason);
  base::UmaHistogramLongTimes(
      kActorBackgroundTaskDurationHistogram,
      base::TimeTicks::Now() - background_task_started_at_);
  LogJournalEvent(
      *journal_, GURL(), task_id_,
      "ActorTaskBackgroundWorker::FinalizeBackgroundTask",
      {{"end_reason", std::string(EndReasonToString(effective_reason))},
       {"progress", base::NumberToString(context.fractionCompleted)}});
}

bool ActorTaskBackgroundWorker::ShouldRunHeartbeat() {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return false;
  }

  if (IsTerminalState(delegate_->GetTaskState())) {
    return false;
  }

  if (!web_state_observations_.IsObservingAnySource()) {
    return false;
  }

  // Any of the WebStates may be in a scene that is not foreground active,
  // start the heartbeat eagerly.
  if (!all_scenes_foreground_active_) {
    return true;
  }

  return std::ranges::any_of(web_state_observations_.sources(),
                             [](const raw_ptr<web::WebState>& web_state) {
                               return !web_state->IsVisible();
                             });
}

void ActorTaskBackgroundWorker::UpdateHeartbeatTimer() {
  if (!ShouldRunHeartbeat()) {
    heartbeat_timer_.Stop();
    return;
  }

  if (heartbeat_timer_.IsRunning()) {
    return;
  }

  heartbeat_timer_.Start(
      FROM_HERE, GetGeminiActorBackgroundWebStateKeepAliveHeartbeatInterval(),
      base::BindRepeating(&ActorTaskBackgroundWorker::SendHeartbeatPing,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ActorTaskBackgroundWorker::SendHeartbeatPing() {
  if (!ShouldRunHeartbeat()) {
    heartbeat_timer_.Stop();
    return;
  }

  for (web::WebState* web_state : web_state_observations_.sources()) {
    web::WebFramesManager* frames_manager =
        web_state->GetWebFramesManager(web::ContentWorld::kIsolatedWorld);
    if (!frames_manager) {
      continue;
    }

    web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
    if (!main_frame) {
      continue;
    }

    main_frame->ExecuteJavaScript(
        kHeartbeatScript,
        base::BindOnce(&ActorTaskBackgroundWorker::OnHeartbeatPingResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       web_state->GetUniqueIdentifier()));
  }
}

void ActorTaskBackgroundWorker::OnHeartbeatPingResponse(
    web::WebStateID web_state_id,
    const base::Value* /*result*/,
    NSError* error) {
  if (error) {
    LogJournalEvent(
        *journal_, GURL(), task_id_, "ActorTask::HeartbeatPingFailed",
        {{"web_state_id", base::NumberToString(web_state_id.identifier())},
         {"error_domain", base::SysNSStringToUTF8(error.domain)},
         {"error_code", base::NumberToString(error.code)}});
  }
}

}  // namespace actor
