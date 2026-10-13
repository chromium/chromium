// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import <map>
#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_id.h"

@class TtcTaskInterventionHandler;
class WebStateList;

namespace actor {
class ActorService;
}  // namespace actor

struct TtcActuationRequest;
struct TtcActuationResponse;

// The handler for TTC actuations, bridging incoming actuation
// requests to the Chromium Actor Service orchestration layer.
class TtcActuationHandler {
 public:
  using ActuationCallback =
      base::OnceCallback<void(const TtcActuationResponse&)>;

  TtcActuationHandler(actor::ActorService* actor_service,
                      WebStateList* web_state_list,
                      SessionID browser_id);
  TtcActuationHandler(const TtcActuationHandler&) = delete;
  TtcActuationHandler& operator=(const TtcActuationHandler&) = delete;
  ~TtcActuationHandler();

  // Disconnects active references, cancels in-flight tasks with `kShutdown`,
  // and fails pending actuation callbacks with `kExecutorDestroyed`.
  void Disconnect();

  // Creates a new Actor task titled `title` associated with the currently
  // active WebState in the browser, and binds that WebState to the task.
  // Returns the allocated `ActorTaskId`, or a null `ActorTaskId` if there is no
  // active WebState, if the active WebState is off-the-record, or if
  // ActorService is unavailable.
  actor::ActorTaskId CreateTask(const std::string& title);

  // Dispatches `request` for execution under `task_id`. The underlying Actor
  // task remains active to allow subsequent actuation requests until explicitly
  // stopped or disconnected.
  void DispatchActuationRequest(const TtcActuationRequest& request,
                                actor::ActorTaskId task_id,
                                ActuationCallback callback);

  // Stops the task identified by `task_id` with the specified `reason` and
  // releases its tab binding. If an actuation request is currently in progress
  // for `task_id`, its callback is invoked with `kTaskWentAway`.
  void StopTask(actor::ActorTaskId task_id,
                actor::ActorTaskStoppedReason reason =
                    actor::ActorTaskStoppedReason::kTaskComplete);

 private:
  struct ActiveActuation {
    ActuationCallback callback;
    std::string call_id;
  };

  // Handles completion of `ActorService::PerformActions`.
  void OnPerformActionsFinished(actor::ActorTaskId task_id,
                                actor::PerformActionsResult result);

  SEQUENCE_CHECKER(sequence_checker_);

  // The ActorService used to execute tasks.
  raw_ptr<actor::ActorService> actor_service_ = nullptr;

  // The WebStateList associated with the active browser.
  raw_ptr<WebStateList> web_state_list_ = nullptr;

  // The SessionID of the browser window.
  std::optional<SessionID> browser_id_;

  // Map from active Actor task IDs to their controlled WebState IDs.
  std::map<actor::ActorTaskId, web::WebStateID> task_to_web_state_id_map_;

  // Active actuation callbacks awaiting completion, keyed by task ID.
  std::map<actor::ActorTaskId, ActiveActuation> active_callbacks_;

  // Intervention delegate used for Actor tasks created by this handler.
  TtcTaskInterventionHandler* intervention_delegate_ = nil;

  base::WeakPtrFactory<TtcActuationHandler> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
