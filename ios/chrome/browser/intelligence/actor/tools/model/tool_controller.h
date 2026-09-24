// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_CONTROLLER_H_

#import <memory>
#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/notimplemented.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

class ToolDelegate;
class ActorTool;
class ActorToolRequest;

// ToolController manages the lifetime and state transitions of an ActorTool.
//
// Mirrored from chrome/browser/actor/tools/tool_controller.h
class ToolController {
 public:
  using ResultCallback = base::OnceCallback<void(ToolExecutionResult)>;

  enum class State {
    kInit = 0,
    kReady,  // Ready for CreateToolAndValidate().
    kCreating,
    kValidating,
    kPostValidate,
    kInvokable,  // Ready for Invoke().
    kPreInvoke,
    kInvoking,
    kPostInvoke,
  };

  ToolController(ToolDelegate* tool_delegate);
  ~ToolController();
  ToolController(const ToolController&) = delete;
  ToolController& operator=(const ToolController&) = delete;

  // Transitions state and runs validation hooks.
  void CreateToolAndValidate(const ActorToolRequest& request,
                             ResultCallback callback);

  // Performs tool execution, including pre-execution hooks and observation
  // delays.
  void Invoke(ResultCallback result_callback);
  void Cancel();
  // Fails the currently executing tool with `code`.
  void FailCurrentTool(mojom::ActionResultCode code);

  // Returns the current state of the controller.
  State state() const { return state_; }

 private:
  // This state is non-null whenever a tool invocation is in progress.
  struct ActiveState {
    ActiveState(
        std::unique_ptr<ActorTool> tool,
        ResultCallback completion_callback,
        std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry);
    ~ActiveState();
    ActiveState(const ActiveState&) = delete;
    ActiveState& operator=(const ActiveState&) = delete;

    // Both `tool` and `completion_callback` are initialized when active_state_
    // is set. `completion_callback` will be null after a step (validation or
    // execution) finishes and before the next step begins.
    // `completion_callback` holds two different callbacks over its lifetime:
    // a callback for when CreateToolAndValidate is finished and, next, a
    // callback for when Invoke is finished.
    std::unique_ptr<ActorTool> tool;
    ResultCallback completion_callback;
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry;
  };

  void SetState(State state);

  // Called when the tool itself finishes its execution.
  void DidFinishToolExecution(ToolExecutionResult result);

  // Clears the current tool invocation and returns the result.
  void CompleteToolRequest(ToolExecutionResult result);

  void PostValidate(ToolExecutionResult result);
  void PostUpdateTask(ToolExecutionResult result);
  void PostInvokeTool(ToolExecutionResult result);
  void WaitForObservation(ToolExecutionResult result);
  void ObservationDelayComplete(
      ToolExecutionResult action_result,
      ObservationDelayController::Result observation_result);

  ActorTaskId task_id() const;
  AggregatedJournal& journal() const;

  State state_ = State::kInit;

  std::optional<ActiveState> active_state_;

  // Set while a tool invocation is in progress, delays invocation of the
  // completion_callback until the page is ready for observation.
  std::unique_ptr<ObservationDelayController> observation_delayer_;

  raw_ptr<ToolDelegate> tool_delegate_;

  base::WeakPtrFactory<ToolController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_CONTROLLER_H_
