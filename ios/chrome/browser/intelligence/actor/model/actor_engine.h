// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_

#import <memory>
#import <optional>
#import <vector>

#import "base/functional/callback.h"

class ActorTool;

namespace actor {

// Executes a sequence of actions moving through the state machine.
class ActorEngine {
 public:
  // Represents the current execution stage of the engine for the active tools.
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
    // Tool execution finished successfully.
    kCompleted,
    // Tool execution hit a terminal failure.
    kFailed
  };

  // Indicates the terminal outcome of the engine's overall execution sequence.
  enum class ActorEngineResult {
    // Default value.
    kUnknown = 0,
    // All requested tools completed successfully.
    kSuccess,
    // A tool failed or could not be verified, aborting the remaining sequence.
    kFailed,
    // A tool operation or prompt timed out.
    kTimeout,
    // The engine's execution was manually aborted.
    kCancelled,
  };

  using EngineCompleteCallback = base::OnceCallback<void(ActorEngineResult)>;

  ActorEngine();
  ~ActorEngine();
  ActorEngine(const ActorEngine&) = delete;
  ActorEngine& operator=(const ActorEngine&) = delete;

  using ExecuteToolsCallback = base::OnceCallback<void(ActorEngineResult)>;

  // Performs the given sequence of tools and invokes the callback when
  // completed.
  void ExecuteTools(std::vector<std::unique_ptr<ActorTool>> tools,
                    ExecuteToolsCallback callback);

  // Cancels any ongoing and pending tools.
  void CancelOngoingAndPendingTools(ActorEngineResult reason);

  // Accessors. TODO(crbug.com/496164779): Remove when they are used internally
  // in ActorEngine, this is to fix compilation warnings about unused fields.
  State state() const { return state_; }
  const std::vector<std::unique_ptr<ActorTool>>& pending_tools() const {
    return pending_tools_;
  }
  const EngineCompleteCallback& completion_callback() const {
    return completion_callback_;
  }

 private:
  // The core state machine loop. Evaluates `state_` and routes to the correct
  // method.
  void AdvanceState();

  // Executes the next tool.
  void ExecuteNextTool();

  // State machine handlers.
  void HandlePreExecutionChecks();
  void HandleToolVerify();
  void HandleUiPreInvoke();
  void HandleToolInvoke();
  void HandleUiPostInvoke();
  void HandleToolCompleted();
  void HandleToolFailed();

  // The current state of the execution engine.
  State state_;

  // The sequence of tools remaining to be executed.
  std::vector<std::unique_ptr<ActorTool>> pending_tools_;

  // Invoked when all actions complete successfully or a terminal error occurs.
  EngineCompleteCallback completion_callback_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENGINE_H_
