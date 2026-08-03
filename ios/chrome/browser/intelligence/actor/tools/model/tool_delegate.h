// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace web {
class WebState;
}  // namespace web

namespace actor {
class ActorToolFactory;
class ActorTaskFormFillingHandler;
class AggregatedJournal;

// Provides the tools layer with access to objects owned by the orchestration
// layer. The lifetime of this is tied to the ActorTask, which outlives any
// tools that use it.
//
// This is based on chrome/browser/actor/tools/tool_delegate.h.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the unique identifier of the active task.
  virtual ActorTaskId GetTaskId() const = 0;

  // Returns the journal used for logging.
  virtual AggregatedJournal& GetJournal() const = 0;

  // Returns the tool factory associated with this task.
  virtual ActorToolFactory& GetToolFactory() const = 0;

  // Returns the handler for form filling and login tasks.
  virtual ActorTaskFormFillingHandler* GetActorTaskFormFillingHandler() = 0;

  // Temporarily interrupts the task's execution flow because the tool is
  // waiting for user interaction (e.g. device re-authentication). This pauses
  // the task and transitions it to the `ActorTaskState::kWaitingOnUser` state.
  virtual void InterruptFromTool() = 0;

  // Resumes the task's execution flow once the user interaction is completed.
  // This restores the task to `ActorTaskState::kActing`.
  virtual void UninterruptFromTool() = 0;

  // Returns whether the window identified by `window_id` exists.
  virtual bool IsWindowIdValid(int32_t window_id) = 0;

  // Inserts a new WebState with `load_params` in the window identified by
  // `window_id`. Position of the new tab is determined by the delegate. Returns
  // the inserted WebState, or nullptr if the insertion failed.
  virtual web::WebState* InsertWebState(
      int32_t window_id,
      const web::NavigationManager::WebLoadParams& load_params,
      bool in_background) = 0;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
