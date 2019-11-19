// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class TaskHandle::Runner : public WTF::ThreadSafeRefCounted<Runner> {
 public:
  explicit Runner(base::OnceClosure task) : task_(std::move(task)) {}

  base::WeakPtr<Runner> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  bool IsActive() const { return task_ && !task_.IsCancelled(); }

  void Cancel() {
    base::OnceClosure task = std::move(task_);
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  ~Runner() { Cancel(); }

  // The TaskHandle parameter on run() holds a reference to the Runner to keep
  // it alive while a task is pending in a task queue, and clears the reference
  // on the task disposal, so that it doesn't leave a circular reference like
  // below:
  //   struct Foo : GarbageCollected<Foo> {
  //     void bar() {}
  //     TaskHandle m_handle;
  //   };
  //
  //   foo.m_handle = taskRunner->postCancellableTask(
  //       FROM_HERE, WTF::bind(&Foo::bar, wrapPersistent(foo)));
  //
  // There is a circular reference in the example above as:
  //   foo -> m_handle -> m_runner -> m_task -> Persistent<Foo> in WTF::bind.
  // The TaskHandle parameter on run() is needed to break the circle by clearing
  // |m_task| when the wrapped base::OnceClosure is deleted.
  void Run(const TaskHandle&) {
    base::OnceClosure task = std::move(task_);
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(task).Run();
  }

 private:
  base::OnceClosure task_;
  base::WeakPtrFactory<Runner> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

}  // namespace blink

namespace base {

using RunnerMethodType =
    void (blink::TaskHandle::Runner::*)(const blink::TaskHandle&);

template <>
struct CallbackCancellationTraits<
    RunnerMethodType,
    std::tuple<base::WeakPtr<blink::TaskHandle::Runner>, blink::TaskHandle>> {
  static constexpr bool is_cancellable = true;

  static bool IsCancelled(RunnerMethodType,
                          const base::WeakPtr<blink::TaskHandle::Runner>&,
                          const blink::TaskHandle& handle) {
    return !handle.IsActive();
  }

  static bool MaybeValid(RunnerMethodType,
                         const base::WeakPtr<blink::TaskHandle::Runner>&,
                         const blink::TaskHandle& handle) {
    // TODO(https://crbug.com/653394): Consider returning a thread-safe best
    // guess of validity.
    return true;
  }
};

}  // namespace base

namespace blink {

bool TaskHandle::IsActive() const {
  return runner_ && runner_->IsActive();
}

void TaskHandle::Cancel() {
  if (runner_) {
    runner_->Cancel();
    runner_ = nullptr;
  }
}

TaskHandle::TaskHandle() = default;

TaskHandle::~TaskHandle() {
  Cancel();
}

TaskHandle::TaskHandle(TaskHandle&&) = default;

TaskHandle& TaskHandle::operator=(TaskHandle&& other) {
  TaskHandle tmp(std::move(other));
  runner_.swap(tmp.runner_);
  return *this;
}

TaskHandle::TaskHandle(scoped_refptr<Runner> runner)
    : runner_(std::move(runner)) {
  DCHECK(runner_);
}

TaskHandle PostCancellableTask(base::SequencedTaskRunner& task_runner,
                               const base::Location& location,
                               base::OnceClosure task) {
  DCHECK(task_runner.RunsTasksInCurrentSequence());
  scoped_refptr<TaskHandle::Runner> runner =
      base::AdoptRef(new TaskHandle::Runner(std::move(task)));
  task_runner.PostTask(location,
                       WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                                 TaskHandle(runner)));
  return TaskHandle(runner);
}

TaskHandle PostDelayedCancellableTask(base::SequencedTaskRunner& task_runner,
                                      const base::Location& location,
                                      base::OnceClosure task,
                                      base::TimeDelta delay) {
  DCHECK(task_runner.RunsTasksInCurrentSequence());
  scoped_refptr<TaskHandle::Runner> runner =
      base::AdoptRef(new TaskHandle::Runner(std::move(task)));
  task_runner.PostDelayedTask(
      location,
      WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                TaskHandle(runner)),
      delay);
  return TaskHandle(runner);
}

TaskHandle PostNonNestableCancellableTask(
    base::SequencedTaskRunner& task_runner,
    const base::Location& location,
    base::OnceClosure task) {
  DCHECK(task_runner.RunsTasksInCurrentSequence());
  scoped_refptr<TaskHandle::Runner> runner =
      base::AdoptRef(new TaskHandle::Runner(std::move(task)));
  task_runner.PostNonNestableTask(
      location, WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                          TaskHandle(runner)));
  return TaskHandle(runner);
}

TaskHandle PostNonNestableDelayedCancellableTask(
    base::SequencedTaskRunner& task_runner,
    const base::Location& location,
    base::OnceClosure task,
    base::TimeDelta delay) {
  DCHECK(task_runner.RunsTasksInCurrentSequence());
  scoped_refptr<TaskHandle::Runner> runner =
      base::AdoptRef(new TaskHandle::Runner(std::move(task)));
  task_runner.PostNonNestableDelayedTask(
      location,
      WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                TaskHandle(runner)),
      delay);
  return TaskHandle(runner);
}

}  // namespace blink
