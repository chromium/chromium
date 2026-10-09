// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
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

}  // namespace

ActorTaskBackgroundWorker::ActorTaskBackgroundWorker(
    TaskStateDelegate* delegate,
    ActorTaskId task_id,
    AggregatedJournal& journal)
    : delegate_(delegate), task_id_(task_id), journal_(journal) {
  CHECK(delegate_);
}

ActorTaskBackgroundWorker::~ActorTaskBackgroundWorker() {
  StopHeartbeatTimer();
  FinalizeBackgroundTask(/*success=*/false);
}

void ActorTaskBackgroundWorker::SetContext(
    BackgroundContinuedProcessingTaskContext* context) {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return;
  }
  background_task_context_ = context;
  UpdateBackgroundTaskSubtitle(delegate_->GetLastTaskUpdate());
}

void ActorTaskBackgroundWorker::OnWebStateAdded() {
  StartHeartbeatTimer();
}

void ActorTaskBackgroundWorker::OnWebStateDestroyed() {
  if (delegate_->GetControlledWebStates().empty()) {
    StopHeartbeatTimer();
  }
}

void ActorTaskBackgroundWorker::OnStateChanged(ActorTaskState new_state) {
  if (IsTerminalState(new_state)) {
    StopHeartbeatTimer();
    return;
  }

  if (new_state == ActorTaskState::kActing) {
    UpdateBackgroundTaskSubtitle(delegate_->GetLastTaskUpdate());
    StartHeartbeatTimer();
  } else if (!ShouldKeepBackgroundTaskAlive(new_state)) {
    // End the background task before long waits (e.g., paused) to avoid
    // stalling system progress.
    FinalizeBackgroundTask(/*success=*/true);
  }
}

void ActorTaskBackgroundWorker::OnWillExecuteTool() {
  IncrementBackgroundTaskProgress();
}

void ActorTaskBackgroundWorker::OnStopped(bool success) {
  StopHeartbeatTimer();
  FinalizeBackgroundTask(success);
}

#pragma mark - Private

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

void ActorTaskBackgroundWorker::FinalizeBackgroundTask(bool success) {
  if (!background_task_context_) {
    return;
  }

  if (!background_task_context_.completed) {
    [background_task_context_ setTaskCompletedWithSuccess:success];
  }
  background_task_context_ = nil;
}

void ActorTaskBackgroundWorker::StartHeartbeatTimer() {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return;
  }

  if (IsTerminalState(delegate_->GetTaskState())) {
    return;
  }

  if (heartbeat_timer_.IsRunning()) {
    return;
  }

  if (delegate_->GetControlledWebStates().empty()) {
    return;
  }

  heartbeat_timer_.Start(
      FROM_HERE, GetGeminiActorBackgroundWebStateKeepAliveHeartbeatInterval(),
      base::BindRepeating(&ActorTaskBackgroundWorker::SendHeartbeatPing,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ActorTaskBackgroundWorker::StopHeartbeatTimer() {
  heartbeat_timer_.Stop();
}

void ActorTaskBackgroundWorker::SendHeartbeatPing() {
  const std::vector<web::WebState*> web_states =
      delegate_->GetControlledWebStates();
  if (web_states.empty()) {
    StopHeartbeatTimer();
    return;
  }

  for (web::WebState* web_state : web_states) {
    if (!web_state) {
      continue;
    }

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
