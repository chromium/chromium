// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/web_state.h"

namespace actor {

ActorTask::ActorTask(ActorTaskId task_id, const std::string& title)
    : task_id_(task_id), title_(title) {
  engine_ = std::make_unique<ActorEngine>();
}

ActorTask::~ActorTask() = default;

ActorTaskState ActorTask::GetState() const {
  return state_;
}

void ActorTask::Act(std::vector<std::unique_ptr<ActorTool>> actions,
                    const std::string& task_update,
                    PerformActionsCallback callback) {
  // TODO(crbug.com/503054406): Check for invalid states.
  state_ = ActorTaskState::kActing;
  engine_->Act(
      std::move(actions),
      base::BindOnce(&ActorTask::OnActCompleted, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ActorTask::OnActCompleted(PerformActionsCallback callback,
                               std::vector<ActionResult> results) {
  // TODO(crbug.com/503054406): Check for tool errors.
  std::move(callback).Run(std::move(results));
}

void ActorTask::Stop(ActorTaskStoppedReason stop_reason) {
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Pause(bool from_actor) {
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Resume() {
  // TODO(crbug.com/496164697): Implement and test.
}

bool ActorTask::IsControllingWebState(web::WebState* web_state) const {
  if (!web_state) {
    return false;
  }

  for (const base::WeakPtr<web::WebState> controlled_web_state :
       controlled_web_states_) {
    if (controlled_web_state && controlled_web_state->GetUniqueIdentifier() ==
                                    web_state->GetUniqueIdentifier()) {
      return true;
    }
  }
  return false;
}

}  // namespace actor
