// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_

#import <string>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@protocol ActorTaskUIDelegate;

namespace actor {

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of actions to execute
// sequentially.
class ActorTask {
 public:
  ActorTask(ActorTaskId task_id,
            const std::string& title,
            id<ActorTaskUIDelegate> delegate);
  ~ActorTask();

  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;

  // Accessors. TODO(crbug.com/496164697): Remove when they are used internally
  // in ActorTask, this is to fix compilation.
  ActorTaskId task_id() const { return task_id_; }
  const std::string& title() const { return title_; }
  id<ActorTaskUIDelegate> delegate() const { return delegate_; }

 private:
  // The task's ID.
  const ActorTaskId task_id_;
  // The task's title.
  const std::string title_;
  // The task's UI delegate.
  __weak id<ActorTaskUIDelegate> delegate_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
