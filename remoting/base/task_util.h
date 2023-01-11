// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TASK_UTIL_H_
#define REMOTING_BASE_TASK_UTIL_H_

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"

namespace remoting {

// Wraps and returns a callback such that it will call the original callback on
// the thread where this method is called.
template <typename... Args>
base::OnceCallback<void(Args...)> WrapCallbackToCurrentSequence(
    const base::Location& from_here,
    base::OnceCallback<void(Args...)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         const base::Location& from_here,
         base::OnceCallback<void(Args...)> callback, Args... args) {
        base::OnceClosure closure =
            base::BindOnce(std::move(callback), std::forward<Args>(args)...);
        if (task_runner->RunsTasksInCurrentSequence()) {
          std::move(closure).Run();
          return;
        }
        task_runner->PostTask(from_here, std::move(closure));
      },
      base::SequencedTaskRunner::GetCurrentDefault(), from_here,
      std::move(callback));
}

// Similar to base::SequenceBound::Post, but executes the callback (which should
// be the last method argument) on the sequence where the task is posted from.
// Say if you want to call this method and make |callback| run on the current
// sequence:
//
//   client_.AsyncCall(&DirectoryClient::DeleteHost).WithArgs(host_id,
//   callback);
//
// You can just do:
//
//   PostWithCallback(FROM_HERE, &client_, &DirectoryClient::DeleteHost,
//                    callback, host_id);
//
// Note that |callback| is moved before other arguments, as type deduction does
// not work the other way around.
//
// Also you should probably bind a WeakPtr in your callback rather than using
// base::Unretained, since the underlying sequence bound object will generally
// be deleted after the owning object.
template <typename SequenceBoundType,
          typename... MethodArgs,
          typename... Args,
          typename... CallbackArgs>
void PostWithCallback(const base::Location& from_here,
                      base::SequenceBound<SequenceBoundType>* client,
                      void (SequenceBoundType::*method)(MethodArgs...),
                      base::OnceCallback<void(CallbackArgs...)> callback,
                      Args&&... args) {
  client->AsyncCall(method, from_here)
      .WithArgs(std::forward<Args>(args)...,
                WrapCallbackToCurrentSequence(from_here, std::move(callback)));
}

}  // namespace remoting

#endif  // REMOTING_BASE_TASK_UTIL_H_
