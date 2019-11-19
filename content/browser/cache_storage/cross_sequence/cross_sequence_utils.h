// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_UTILS_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_UTILS_H_

namespace content {

// Helper function for WrapCallbackForCurrentSequence().
template <typename... Args>
void RunWrappedCallbackOnTargetSequence(
    base::OnceCallback<void(Args...)> callback,
    Args... args) {
  std::move(callback).Run(std::forward<Args>(args)...);
}

// Helper function for WrapCallbackForCurrentSequence().
template <typename... Args>
void RunWrappedCallbackOnOtherSequence(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(Args...)> callback,
    Args... args) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&RunWrappedCallbackOnTargetSequence<Args...>,
                     std::move(callback), std::forward<Args>(args)...));
}

// This function wraps a given OnceCallback<> such that it will perform a
// PostTask() to the current sequence when the callback is invoked.
template <typename... Args>
base::OnceCallback<void(Args...)> WrapCallbackForCurrentSequence(
    base::OnceCallback<void(Args...)> callback) {
  return base::BindOnce(&RunWrappedCallbackOnOtherSequence<Args...>,
                        base::SequencedTaskRunnerHandle::Get(),
                        std::move(callback));
}

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_UTILS_H_
