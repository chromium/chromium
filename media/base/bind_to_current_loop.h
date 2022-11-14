// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
#define MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"

// Helpers for using base::BindPostTask() with the TaskRunner for the current
// sequence, ie. base::SequencedTaskRunner::GetCurrentDefault().

namespace media {

template <typename... Args>
inline base::RepeatingCallback<void(Args...)> BindToCurrentLoop(
    base::RepeatingCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(cb), location);
}

template <typename... Args>
inline base::OnceCallback<void(Args...)> BindToCurrentLoop(
    base::OnceCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(cb), location);
}

}  // namespace media

#endif  // MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
