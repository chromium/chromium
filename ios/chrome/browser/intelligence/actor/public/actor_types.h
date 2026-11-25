// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_

#import <memory>
#import <string>
#import <variant>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"
#import "base/types/id_type.h"
#import "components/actor/core/task_id.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state_id.h"

namespace url {
class Origin;
}  // namespace url

namespace actor {

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

// Returns whether `state` corresponds to a terminal actor task state.
bool IsTerminalState(ActorTaskState state);

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
  // System or browser is shutting down (including a cancelled/expired
  // background task).
  kShutdown = 5,
  // User started a new chat session, aborting the current task.
  kUserStartedNewChat = 6
};

// Why an intervention delegate failed to show a prompt.
enum class InterventionDelegateError {
  // The intervention delegate does not support the request.
  kUnsupported,
};

// Result from an intervention delegate prompt.
template <typename T>
using InterventionDelegateResult = base::expected<T, InterventionDelegateError>;

// Why an intervention request failed.
enum class InterventionError {
  // The task could not take the request when it was sent. The task is not
  // stopped.
  kTaskRejected,
  // The task does not exist or was stopped.
  kTaskWentAway,
  // The intervention delegate does not support the request.
  kDelegateUnsupported,
};

// Result of an intervention request.
template <typename T>
using InterventionResult = base::expected<T, InterventionError>;

// Receives the result of an intervention request.
template <typename T>
using InterventionResultCallback =
    base::OnceCallback<void(InterventionResult<T>)>;

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

// Returns the results reported for an `Act()` request rejected or aborted as a
// whole (e.g. `kExecutionEngineExistingAction` for a concurrent request): a
// single result carrying `code`. An empty vector is not used because it is
// indistinguishable from a successful empty action sequence.
std::vector<ActionResult> MakeActionResults(mojom::ActionResultCode code);

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

// Asks the user to confirm before the task continues.
struct ConfirmationInterrupt {
  ConfirmationInterrupt();
  ConfirmationInterrupt(const ConfirmationInterrupt&);
  ConfirmationInterrupt& operator=(const ConfirmationInterrupt&);
  ConfirmationInterrupt(ConfirmationInterrupt&&) noexcept;
  ConfirmationInterrupt& operator=(ConfirmationInterrupt&&) noexcept;
  ~ConfirmationInterrupt();

  // Prompt title. Required.
  std::string title;
  // Prompt subtitle. Optional; not shown when empty.
  std::string subtitle;
  // Label of the confirming button. Required.
  std::string button_text;
};

// Asks the user a question and waits for a typed answer.
struct ClarificationInterrupt {
  // Question asked to the user. Required.
  std::string message;
};

// An interrupt; the alternative selects its type.
using InterruptRequest =
    std::variant<ConfirmationInterrupt, ClarificationInterrupt>;

// How the user answered an interrupt.
enum class InterruptResponse {
  // The user confirmed a `ConfirmationInterrupt`.
  kConfirmed,
  // The user answered a `ClarificationInterrupt`.
  kAnswered,
  // The user paused the task since the last reply, so the interrupt went
  // unanswered.
  kUserPaused,
};

// Reply of a task to an interrupt.
// TODO(crbug.com/590286157): Tie interrupt replies to each request kind.
class InterruptReply {
 public:
  // Returns a reply for a confirmed `ConfirmationInterrupt`.
  static InterruptReply Confirmed();
  // Returns a reply for an answered `ClarificationInterrupt`. `answer` must
  // be non-empty.
  static InterruptReply Answered(std::string answer);
  // Returns a reply indicating the user paused the task before answering.
  static InterruptReply UserPaused();

  InterruptReply(const InterruptReply&);
  InterruptReply& operator=(const InterruptReply&);
  InterruptReply(InterruptReply&&) noexcept;
  InterruptReply& operator=(InterruptReply&&) noexcept;
  ~InterruptReply();

  // How the user answered.
  InterruptResponse response() const { return response_; }
  // Text typed by the user. Empty unless `response()` is `kAnswered`.
  const std::string& answer() const { return answer_; }

 private:
  InterruptReply(InterruptResponse response, std::string answer);

  InterruptResponse response_;
  std::string answer_;
};

// Result of a successful `ActorService::InterruptTask()`.
struct InterruptTaskResult {
  InterruptTaskResult(InterruptReply reply,
                      std::unique_ptr<TabObservationResponse> observation);
  ~InterruptTaskResult();
  InterruptTaskResult(const InterruptTaskResult&) = delete;
  InterruptTaskResult& operator=(const InterruptTaskResult&) = delete;
  InterruptTaskResult(InterruptTaskResult&&) noexcept;
  InterruptTaskResult& operator=(InterruptTaskResult&&) noexcept;

  // How the user answered.
  InterruptReply reply;
  // Observation of the task's tab once the user answered, or null if the task
  // controls no live tab.
  std::unique_ptr<TabObservationResponse> observation;
};

// Callback for a "tab observation" (nomenclature aligned with
// `chrome/browser/actor`). Tab is equivalent to WebState and observation is
// equivalent to a PageContext extraction. Not an "observing" pattern. In
// practice, this is the completion callback executed after a rich actionable
// mode PageContextWrapper extraction.
using TabObservationCallback =
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>;

// Callback invoked with the result of a navigation confirmation query (e.g.
// from the server-side Origin Critic model). `permission_granted` is true
// if the navigation to the new origin is aligned with the task.
using NavigationConfirmationDecisionCallback =
    base::OnceCallback<void(bool permission_granted)>;

// Callback registered by the task creator to evaluate out-of-turn cross-origin
// navigations against the Origin Critic model.
using NavigationConfirmationCallback = base::RepeatingCallback<void(
    const url::Origin& navigation_origin,
    NavigationConfirmationDecisionCallback callback)>;

// Callback invoked with the user's decision on whether allow navigating to
// a sensitive or unconfirmed origin (`true` if the user confirmed).
using UserConfirmationDialogDecisionCallback =
    base::OnceCallback<void(bool permission_granted)>;

// Callback to prompt the user for confirmation before navigating to
// `navigation_origin`.
using UserConfirmationDialogCallback = base::RepeatingCallback<void(
    const url::Origin& navigation_origin,
    UserConfirmationDialogDecisionCallback callback)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
