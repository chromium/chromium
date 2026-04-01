// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

namespace actor {

ActorTask::ActorTask(ActorTaskId task_id,
                     const std::string& title,
                     id<ActorTaskUIDelegate> delegate)
    : task_id_(task_id), title_(title), delegate_(delegate) {}

ActorTask::~ActorTask() = default;

}  // namespace actor
