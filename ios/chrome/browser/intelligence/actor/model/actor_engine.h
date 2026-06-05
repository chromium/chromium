// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_

#import <memory>
#import <optional>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

class ActorTool;
class ActorToolRequest;
class ActorToolFactory;
class ObservationDelayController;

// Executes a sequence of actions moving through the state machine.
//
// Note on terminology: A "tool" (represented by `ActorTool`) is the capability
// (e.g., click or type), while an "action" is a specific instance of that
// capability being executed with specific parameters.
//
// Each action execution includes checks, UI updates, and the core work which is
// the tool invocation.
class ActorEngine {
 public:
  // Delegate interface to receive granular tool execution progress updates
  // from the engine. This allows the owning `ActorTask` to track which specific
  // tool is currently being executed.
  class ExecutionUpdatesDelegate {
   public:
    virtual ~ExecutionUpdatesDelegate() = default;

    // Called immediately before a tool is executed.
    // `tool_type` is the type of the tool to be executed, and `web_state_id` is
    // the identifier of the target WebState.
    virtual void OnWillExecuteTool(ToolType tool_type,
                                   web::WebStateID web_state_id) = 0;
  };

  // Represents the current execution stage of the engine for the active
  // actions.
  enum class State {
    // Default value.
    kUnknown = 0,
    // Engine is waiting to begin.
    kInit,
    // Safety and verification checks.
    kPreExecutionChecks,
    // Verifies the tool payload.
    kToolVerify,
    // Invokes UI updates or prompts prior to tool execution.
    kUiPreInvoke,
    // Executes the core logic of the tool asynchronously.
    kToolInvoke,
    // Invokes UI updates after tool execution.
    kUiPostInvoke,
    // Action execution finished successfully.
    kCompleted,
    // Action execution hit a terminal failure.
    kFailed
  };

  // Indicates the terminal outcome of the engine's overall execution sequence.
  enum class EngineResult {
    // Default value.
    kUnknown = 0,
    // All requested actions completed successfully.
    kSuccess,
    // An action failed or could not be verified, aborting the remaining
    // sequence.
    kFailed,
    // An action tool invocation or prompt timed out.
    kTimeout,
    // The engine's execution was manually aborted.
    kCancelled,
  };

  ActorEngine(ActorTaskId task_id,
              AggregatedJournal* journal,
              ExecutionUpdatesDelegate* execution_updates_delegate,
              ActorToolFactory* tool_factory);

  ~ActorEngine();
  ActorEngine(const ActorEngine&) = delete;
  ActorEngine& operator=(const ActorEngine&) = delete;

  // Performs the given sequence of actions and invokes the callback when
  // completed.
  void Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
           ActCallback callback);

  // Cancels any ongoing and pending actions.
  void CancelOngoingAndPendingActions(EngineResult reason);

 private:
  friend class ActorEngineTest;

  // Executes the next action.
  void ExecuteNextAction();

  // Triggers the UI pre-invoke phase.
  void UiPreInvoke();

  // Sets the engine state and logs the transition.
  void SetState(State new_state);

  // Callback for when UI pre-invoke is finished.
  void FinishedUiPreInvoke(ActionResult result);

  // Callback invoked when a tool completes execution, which bridges the tool's
  // `ToolExecutionResult` into an `ActionResult`.
  void OnToolExecutionComplete(ActorTool* tool,
                               ToolExecutionResult tool_result);

  // Callback for when tool execution is finished.
  void FinishedToolInvoke(ActionResult result);

  // Callback for when UI post-invoke is finished.
  void FinishedUiPostInvoke(ActionResult result);

  // Completes the current sequence of actions, handling success or failure.
  // This method should be called when the execution of the tool sequence is
  // finished or when a terminal failure occurs in the action. It updates the
  // engine state and runs the completion callback. If a failure occurs, it
  // records the failure result, potentially overwriting a previous success
  // result for the same actions if it failed in a post-tool-invoke step.
  void CompleteActions(ActionResult result);

  // Returns the index of the action currently in progress.
  size_t InProgressActionIndex() const;

  // The tool factory used to instantiate tools. This is owned by the creator
  // of the engine (i.e. ActorService) and is guaranteed to outlive this
  // instance.
  raw_ptr<ActorToolFactory> tool_factory_;

  // The current tool being executed. This is reset when the action is completed
  // or the ActorEngine is destroyed.
  std::unique_ptr<ActorTool> current_tool_;

  // The current state of the execution engine.
  State state_;

  // The sequence of actions to be executed.
  std::vector<std::unique_ptr<ActorToolRequest>> action_sequence_;

  // The index of the *next* action that will be invoked. Prefer to use
  // `InProgressActionIndex()` to get the index of the action currently being
  // executed.
  size_t next_action_index_ = 0;

  // Invoked when all actions complete or a terminal error occurs.
  ActCallback completion_callback_;

  // Accumulated results of executed actions. Results are added here in
  // `FinishedToolInvoke` on successful tool execution. If a subsequent step
  // (like UI post-invoke) fails, the result at the corresponding index is
  // overwritten with the failure. If a failure occurs before tool execution,
  // the failure result is added here by `CompleteActions`. Aligns with Desktop
  // implementation.
  std::vector<ActionResult> action_results_;

  // The ID of the task that owns this engine.
  ActorTaskId task_id_;

  // The aggregated journal for logging.
  raw_ptr<AggregatedJournal> journal_;

  // Current async entry for journal logging.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> current_async_entry_;

  // This is used to add delays after tool invocations to ensure that the page
  // is ready for another tool invocation.
  //
  // TODO(crbug.com/504625981): Replace this with a ToolController once setup.
  std::unique_ptr<ObservationDelayController> observation_delay_controller_;

  // The delegate to notify of execution milestones.
  raw_ptr<ExecutionUpdatesDelegate> execution_updates_delegate_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorEngine> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
