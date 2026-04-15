// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_

#import <map>
#import <memory>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;

namespace actor {

class ActorTask;
class ActorToolFactory;
class AggregatedJournal;

// Service responsible for handling Actor requests. The normal flow is to
// `CreateTask` and reuse this ID for an entire Actor task (journey).
// `ExecuteTools` can be called multiple times on the same task sequentially.
class ActorService : public KeyedService {
 public:
  explicit ActorService(ProfileIOS* profile);
  ActorService(ProfileIOS* profile,
               std::unique_ptr<ActorToolFactory> tool_factory);
  ~ActorService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes the given action.
  // TODO(crbug.com/498191921): This is legacy/deprecated. Use `CreateTask` and
  // `ExecuteTools` instead. It will be cleaned up soon.
  void ExecuteAction(const optimization_guide::proto::Action& action,
                     ToolExecutionCallback callback);

  // Creates a new task.
  ActorTaskId CreateTask(const std::string& title,
                         bool allow_incognito_web_states);

  // Submits tools to an active task with a task update string (a short blurb
  // which tells the user what the Actor is currently doing in plain language).
  void ExecuteTools(ActorTaskId task_id,
                    std::vector<std::unique_ptr<ActorTool>> tools,
                    const std::string& task_update,
                    PerformActionsCallback callback);

  // Pauses a task.
  void PauseTask(ActorTaskId task_id, bool from_actor);

  // Stops a task.
  void StopTask(ActorTaskId task_id, ActorTaskStoppedReason reason);

  // Returns the list of supported capabilities.
  std::vector<optimization_guide::proto::Action::ActionCase>
  GetSupportedCapabilities() const;

  // Returns the aggregated journal for this service.
  AggregatedJournal* GetJournal() { return journal_.get(); }

 private:
  // The profile associated with this service instance.
  raw_ptr<ProfileIOS> profile_;

  // Actor tool factory.
  std::unique_ptr<ActorToolFactory> tool_factory_;

  // Journal used for logging task tools, state transitions, and results.
  std::unique_ptr<AggregatedJournal> journal_;

  // Map of active tasks, keyed by their task ID.
  std::map<ActorTaskId, std::unique_ptr<ActorTask>> active_tasks_;

  // Generator for unique task IDs.
  ActorTaskId::Generator next_task_id_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorService> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
