// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SCOPED_TASK_RUNNER_OBSERVER_H_
#define MEDIA_AUDIO_SCOPED_TASK_RUNNER_OBSERVER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/current_thread.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace media {

// A common base class for AudioOutputDevice and AudioInputDevice that manages
// a task runner and monitors it for destruction. If the object goes out of
// scope before the task runner, the object will automatically remove itself
// from the task runner's list of destruction observers.
// NOTE: The class that inherits from this class must implement the
// WillDestroyCurrentMessageLoop virtual method from DestructionObserver.
class ScopedTaskRunnerObserver
    : public base::CurrentThread::DestructionObserver {
 public:
  explicit ScopedTaskRunnerObserver(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  ScopedTaskRunnerObserver(const ScopedTaskRunnerObserver&) = delete;
  ScopedTaskRunnerObserver& operator=(const ScopedTaskRunnerObserver&) = delete;

 protected:
  ~ScopedTaskRunnerObserver() override;

  // Accessor to the loop that's used by the derived class.
  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return task_runner_;
  }

 private:
  // Call to add or remove ourselves from the list of destruction observers for
  // the message loop.
  void ObserveLoopDestruction(bool enable, base::WaitableEvent* done);

  // A pointer to the task runner. In case it gets destroyed before this object
  // goes out of scope, PostTask() etc will fail but not crash.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace media.

#endif  // MEDIA_AUDIO_SCOPED_TASK_RUNNER_OBSERVER_H_
