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

namespace actor {
class ActorTask;
}  // namespace actor

class ActorToolFactory;
class AggregatedJournal;
class ProfileIOS;

@protocol ActorTaskUIDelegate;

// Service responsible for handling Actor requests. The normal flow is to
// `CreateTask` and reuse this ID for an entire Actor task (journey).
// `PerformActions` can be called multiple times on the same task sequentially
// as the actions complete.
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
  // `PerformActions` instead. It will be cleaned up soon.
  void ExecuteAction(const optimization_guide::proto::Action& action,
                     ActorTool::ActorCallback callback);

  // Creates a new task.
  actor::ActorTaskId CreateTask(const std::string& title,
                                id<ActorTaskUIDelegate> delegate,
                                bool allow_incognito_web_states);

  // Submits actions to an active task with a task update string (a short blurb
  // which tells the user what the Actor is currently doing in plain language).
  void PerformActions(actor::ActorTaskId task_id,
                      std::vector<std::unique_ptr<ActorTool>> actions,
                      const std::string& task_update,
                      actor::PerformActionsCallback callback);

  // Pauses a task.
  void PauseTask(actor::ActorTaskId task_id, bool from_actor);

  // Stops a task.
  void StopTask(actor::ActorTaskId task_id,
                actor::ActorTaskStoppedReason reason);

  // Returns the list of supported capabilities.
  std::vector<optimization_guide::proto::Action::ActionCase>
  GetSupportedCapabilities() const;

  // Returns the aggregated journal for this service.
  AggregatedJournal* GetJournal() { return journal_.get(); }

 private:
  // The profile associated with this service instance.
  raw_ptr<ProfileIOS> profile_;

  // Factory used to instantiate the appropriate tool for a given action.
  std::unique_ptr<ActorToolFactory> tool_factory_;

  // Journal used for logging task actions, state transitions, and results.
  std::unique_ptr<AggregatedJournal> journal_;

  // Map of active tasks, keyed by their task ID.
  std::map<actor::ActorTaskId, std::unique_ptr<actor::ActorTask>> active_tasks_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
