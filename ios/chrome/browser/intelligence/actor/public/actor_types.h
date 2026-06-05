// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_

#import <vector>

#import "base/functional/callback_forward.h"
#import "base/types/id_type.h"
#import "components/actor/core/task_id.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

class ActorToolRequest;

// Result of creating a batch of ActorToolRequests from Action protos.
using CreateActorToolRequestsResult =
    base::expected<std::vector<std::unique_ptr<ActorToolRequest>>,
                   ToolExecutionResult>;

// Strongly typed, performant unique ID representing an ActorTask.
using ActorTaskId = actor::TaskId;
static_assert(ActorTaskId(0).is_null(), "0 must be a null ActorTaskId");

// Represents the high-level orchestration state of an ActorTask.
enum class ActorTaskState {
  // Task is initialized but has not started executing tools.
  kInit = 0,
  // Task is actively executing through its engine.
  kActing = 1,
  // Task is waiting for AI provider to reflect on next actions to execute.
  kReflecting = 2,
  // Task execution was paused by the actor.
  kPausedByActor = 3,
  // Task execution was paused by the user.
  kPausedByUser = 4,
  // Task execution was cancelled or aborted.
  kCancelled = 5,
  // Task successfully completed.
  kFinished = 6,
  // Task is currently waiting for input or confirmation from the user.
  kWaitingOnUser = 7,
  // Task execution encountered a terminal failure.
  kFailed = 8
};

// Reasons why an ActorTask was stopped.
enum class ActorTaskStoppedReason {
  // Task was explicitly stopped by the user.
  kStoppedByUser = 0,
  // Task successfully completed its execution.
  kTaskComplete = 1,
  // The underlying model encountered an error during the task.
  kModelError = 2,
  // A browser failure occurred.
  kBrowserFailure = 3,
  // One of the tabs executing the task was detached or destroyed.
  kTabDetached = 4,
  // System or browser is shutting down.
  kShutdown = 5,
  // User started a new chat session, aborting the current task.
  kUserStartedNewChat = 6
};

// Represents the result of an action execution.
// TODO(crbug.com/505085267): Add latency and stabilization information.
struct ActionResult {
  explicit ActionResult(ToolExecutionResult result);
  ~ActionResult();
  ActionResult(const ActionResult&) = delete;
  ActionResult& operator=(const ActionResult&) = delete;
  ActionResult(ActionResult&&);
  ActionResult& operator=(ActionResult&&);

  // The result of the tool execution.
  ToolExecutionResult tool_result;
};

// Callback for when ActorTask/ActorEngine's `Act` finishes executing actions.
using ActCallback = base::OnceCallback<void(std::vector<ActionResult>)>;

// Represents a response for a tab observation (PageContext extraction),
// associating the WebStateID with the extraction response or failure reason.
struct TabObservationResponse {
  TabObservationResponse();
  TabObservationResponse(
      web::WebStateID tab_id,
      PageContextWrapperCallbackResponse page_context_response,
      bool web_state_exists);
  ~TabObservationResponse();
  TabObservationResponse(const TabObservationResponse&) = delete;
  TabObservationResponse& operator=(const TabObservationResponse&) = delete;
  TabObservationResponse(TabObservationResponse&&);
  TabObservationResponse& operator=(TabObservationResponse&&);

  web::WebStateID tab_id;
  PageContextWrapperCallbackResponse page_context_response;
  bool web_state_exists = true;
};

// Result of a set of actions execution, including relevant PageContext
// extractions for the task's controlled WebStates.
struct PerformActionsResult {
  PerformActionsResult();
  ~PerformActionsResult();
  PerformActionsResult(const PerformActionsResult&) = delete;
  PerformActionsResult& operator=(const PerformActionsResult&) = delete;
  PerformActionsResult(PerformActionsResult&&);
  PerformActionsResult& operator=(PerformActionsResult&&);

  // The action results for each action that was executed.
  std::vector<ActionResult> action_results;

  // The PageContext extractions for the task's controlled WebStates.
  std::vector<std::unique_ptr<TabObservationResponse>> page_contexts;
};

// Callback for when a set of actions finishes execution.
using PerformActionsCallback = base::OnceCallback<void(PerformActionsResult)>;

// Callback for a "tab observation" (nomenclature aligned with
// `chrome/browser/actor`). Tab is equivalent to WebState and observation is
// equivalent to a PageContext extraction. Not an "observing" pattern. In
// practice, this is the completion callback executed after a rich actionable
// mode PageContextWrapper extraction.
using TabObservationCallback =
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
