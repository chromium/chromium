// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/tool_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class TtcTaskInterventionHandler;

namespace actor {
class ActorService;
}  // namespace actor

namespace optimization_guide::proto {
class Action;
}  // namespace optimization_guide::proto

namespace web {
class WebState;
}  // namespace web

struct TtcActuationRequest;

// The handler for TTC actuations, bridging incoming actuation requests to the
// Chromium Actor Service orchestration layer.
class TtcActuationHandler {
 public:
  using ActuationCallback = ttc::ToolResponseCallback;

  // Constructs an actuation handler backed by `actor_service`.
  explicit TtcActuationHandler(actor::ActorService* actor_service);
  TtcActuationHandler(const TtcActuationHandler&) = delete;
  TtcActuationHandler& operator=(const TtcActuationHandler&) = delete;
  ~TtcActuationHandler();

  // Optional session journal for structured event logging.
  TTCJournal* journal() const;
  void set_journal(TTCJournal* journal);

  // Disconnects active references, cancels any active task with `kShutdown`,
  // and fails any pending actuation callback with `kExecutorDestroyed`.
  void Disconnect();

  // Validates `tool_request` via `TtcToolValidator`, dispatches the resulting
  // actuation request on `active_web_state`, and invokes `callback` (which must
  // be non-null) with the execution result, or `kTabWentAway` if
  // `active_web_state` is null or off-the-record.
  void ProcessToolCall(const ttc::ToolRequest& tool_request,
                       web::WebState* active_web_state,
                       ActuationCallback callback);

  // Dispatches `request` for execution on `active_web_state`, lazily creating
  // the session's Actor task if needed, and invokes `callback` (which must be
  // non-null) upon completion, or with `kTabWentAway` if `active_web_state` is
  // null or off-the-record. The underlying Actor task remains active to allow
  // subsequent actuation requests until explicitly stopped or disconnected.
  void DispatchActuationRequest(const TtcActuationRequest& request,
                                web::WebState* active_web_state,
                                ActuationCallback callback);

  // Stops the active Actor task (if any) with the specified `reason`. If an
  // actuation request is currently in progress, its callback is invoked with
  // `kTaskWentAway`.
  void StopTask(actor::ActorTaskStoppedReason reason =
                    actor::ActorTaskStoppedReason::kTaskComplete);

 private:
  // Handles completion of `ActorService::PerformActions`.
  void OnPerformActionsFinished(actor::PerformActionsResult result);

  // Logs an `ActuationDispatchRejected` error entry to `journal_` (using
  // `reason` if non-empty, or `base::ToString(code)` otherwise) and invokes
  // `callback` with `code`.
  void FailWithError(ActuationCallback callback,
                     actor::mojom::ActionResultCode code,
                     std::string_view reason = {});

  // Begins an `ActuationExecute` async journal entry for `actions`, or returns
  // `nullptr` if `journal_` is null.
  std::unique_ptr<TTCJournal::PendingAsyncEntry> BeginActuationAsyncEntry(
      const std::vector<optimization_guide::proto::Action>& actions);

  SEQUENCE_CHECKER(sequence_checker_);

  // The `ActorService` instance used to create tasks and perform actions.
  raw_ptr<actor::ActorService> actor_service_ = nullptr;

  // The `ActorTaskId` owned by this handler for the current session, or a null
  // `ActorTaskId` when no task is active.
  actor::ActorTaskId task_id_{};

  // Active actuation callback awaiting completion, if any.
  ActuationCallback active_callback_;

  // Active async journal entry for the in-flight actuation, if any.
  std::unique_ptr<TTCJournal::PendingAsyncEntry> active_async_entry_;

  // Intervention delegate used for Actor tasks created by this handler.
  TtcTaskInterventionHandler* intervention_delegate_ = nil;

  // Optional session journal for structured event logging.
  base::WeakPtr<TTCJournal> journal_;

  base::WeakPtrFactory<TtcActuationHandler> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
