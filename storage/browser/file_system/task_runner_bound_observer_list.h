// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_TASK_RUNNER_BOUND_OBSERVER_LIST_H_
#define STORAGE_BROWSER_FILE_SYSTEM_TASK_RUNNER_BOUND_OBSERVER_LIST_H_

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"

namespace storage {

// An observer list helper to notify on a given task runner.
// Observer pointers must be kept alive until this list dispatches all the
// notifications.
//
// Unlike regular ObserverList or ObserverListThreadSafe internal observer
// list is immutable (though not declared const) and cannot be modified after
// constructed.
template <typename Observer>
class TaskRunnerBoundObserverList {
 public:
  using ObserversListMap =
      std::map<Observer*, scoped_refptr<base::SequencedTaskRunner>>;

  // Creates an empty list.
  TaskRunnerBoundObserverList() {}

  // Creates a new list with given |observers|.
  explicit TaskRunnerBoundObserverList(const ObserversListMap& observers)
      : observers_(observers) {}

  virtual ~TaskRunnerBoundObserverList() {}

  // Returns a new observer list with given observer.
  // It is valid to give nullptr as |runner_to_notify|, and in that case
  // notifications are dispatched on the current runner.
  // Note that this is a const method and does NOT change 'this' observer
  // list but returns a new list.
  TaskRunnerBoundObserverList AddObserver(
      Observer* observer,
      base::SequencedTaskRunner* runner_to_notify) const {
    ObserversListMap observers = observers_;
    observers.insert(std::make_pair(observer, runner_to_notify));
    return TaskRunnerBoundObserverList(observers);
  }

  // Notify on the task runner that is given to AddObserver.
  // If we're already on the runner this just dispatches the method.
  template <typename Method, typename... Params>
  void Notify(Method method, Params&&... params) const {
    for (auto& observer : observers_) {
      if (!observer.second || observer.second->RunsTasksInCurrentSequence()) {
        ((*observer.first).*method)(params...);
        continue;
      }
      observer.second->PostTask(
          FROM_HERE,
          base::BindOnce(method, base::Unretained(observer.first), params...));
    }
  }

 private:
  ObserversListMap observers_;
};

class FileAccessObserver;
class FileChangeObserver;
class FileUpdateObserver;

using AccessObserverList = TaskRunnerBoundObserverList<FileAccessObserver>;
using ChangeObserverList = TaskRunnerBoundObserverList<FileChangeObserver>;
using UpdateObserverList = TaskRunnerBoundObserverList<FileUpdateObserver>;

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_TASK_RUNNER_BOUND_OBSERVER_LIST_H_
