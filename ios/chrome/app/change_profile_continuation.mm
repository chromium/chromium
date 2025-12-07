// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/change_profile_continuation.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"

namespace {

// Helper to chain two continuations.
void ChainChangeProfileContinuationsImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ChangeProfileContinuation continuation1,
    ChangeProfileContinuation continuation2,
    SceneState* scene_state,
    base::OnceClosure closure) {
  // Abort the chain if the SceneState has been deallocated (chaining uses
  // a weak pointer, so it is possible to end up in that state if the app
  // shuts down during a profile switching).
  if (!scene_state) {
    return;
  }

  __weak SceneState* weak_scene_state = scene_state;
  std::move(continuation1)
      .Run(scene_state,
           base::BindPostTask(
               task_runner,
               base::BindOnce(std::move(continuation2), weak_scene_state,
                              std::move(closure))));
}

}  // anonymous namespace

ChangeProfileContinuation ChainChangeProfileContinuations(
    ChangeProfileContinuation continuation1,
    ChangeProfileContinuation continuation2) {
  CHECK(continuation1);
  CHECK(continuation2);

  return base::BindOnce(&ChainChangeProfileContinuationsImpl,
                        base::SequencedTaskRunner::GetCurrentDefault(),
                        std::move(continuation1), std::move(continuation2));
}
