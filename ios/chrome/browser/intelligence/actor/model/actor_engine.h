// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_

#import <memory>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_id.h"

@class ActorTaskInterventionHandler;

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ActorTask;
class ActorTaskFormFillingHandler;
class ActorTool;
class ActorToolFactory;
class ActorToolRequest;
class ToolController;

// Executes a sequence of actions moving through the state machine.
//
// Note on terminology: A "tool" (represented by `ActorTool`) is the capability
// (e.g., click or type), while an "action" is a specific instance of that
// capability being executed with specific parameters.
//
// Each action execution includes checks, UI updates, and the core work which is
// the tool invocation.
class ActorEngine : public ToolDelegate {
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

  ActorEngine(ExecutionUpdatesDelegate* execution_updates_delegate,
              ActorTask* owner_task);

  ~ActorEngine() override;
  ActorEngine(const ActorEngine&) = delete;
  ActorEngine& operator=(const ActorEngine&) = delete;

  // Performs the given sequence of actions and invokes the callback when
  // completed.
  void Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
           ActCallback callback);

  // Cancels any ongoing and pending actions.
  void CancelOngoingAndPendingActions(EngineResult reason);

  // ToolDelegate:
  ActorTaskId GetTaskId() const override;
  AggregatedJournal& GetJournal() const override;
  ActorToolFactory& GetToolFactory() const override;
  ActorTaskFormFillingHandler* GetActorTaskFormFillingHandler() override;
  void InterruptFromTool() override;
  void UninterruptFromTool() override;
  bool IsWindowIdValid(int32_t window_id) override;
  web::WebState* InsertWebState(
      int32_t window_id,
      const web::NavigationManager::WebLoadParams& load_params,
      bool in_background) override;

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

  // Callback invoked when tool validation is complete.
  void OnToolValidationComplete(ToolExecutionResult result);

  // Callback invoked when a tool completes execution, which bridges the tool's
  // `ToolExecutionResult` into an `ActionResult`.
  void OnToolExecutionComplete(ToolExecutionResult result);

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

  // The state machine responsible for validating, creating, invoking and
  // post-invocation of the current tool.
  std::unique_ptr<ToolController> tool_controller_;

  // The delegate to notify of execution milestones.
  raw_ptr<ExecutionUpdatesDelegate> execution_updates_delegate_ = nullptr;

  // The ActorTask that owns this ActorEngine.
  raw_ptr<ActorTask> owner_task_ = nullptr;

  // The handler for form filling and login tasks.
  std::unique_ptr<ActorTaskFormFillingHandler> form_filling_handler_;

  // Handler object that intercepts task UI interventions.
  __strong ActorTaskInterventionHandler* intervention_handler_ = nil;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorEngine> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
