// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_TASK_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_TASK_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner_helpers.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace storage {

class QuotaTaskObserver;

// A base class for quota tasks.
//
// Instances of this class own themselves and schedule themselves for deletion
// when async tasks are either aborted or completed.
// This class is not thread-safe and it's subclasses need not be either.
// CallCompleted(), Abort(), and DeleteSoon() must be called on the same thread
// that is the constructor is called on.
// TODO(kinuko): Revise this using base::OnceCallback.
class QuotaTask {
 public:
  QuotaTask(const QuotaTask&) = delete;
  QuotaTask& operator=(const QuotaTask&) = delete;

  void Start();

 protected:
  explicit QuotaTask(QuotaTaskObserver* observer);
  virtual ~QuotaTask();

  // The task body.
  virtual void Run() = 0;

  // Called upon completion, on the original message loop.
  virtual void Completed() = 0;

  // Called when the task is aborted.
  virtual void Aborted() {}

  void CallCompleted();

  // Call this to delete itself.
  void DeleteSoon();

  QuotaTaskObserver* observer() const { return observer_; }

 private:
  friend class base::DeleteHelper<QuotaTask>;
  friend class QuotaTaskObserver;

  void Abort();

  raw_ptr<QuotaTaskObserver,
          FlakyDanglingUntriaged | AcrossTasksDanglingUntriaged>
      observer_;
  const scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;
  bool delete_scheduled_;
};

class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaTaskObserver {
 protected:
  friend class QuotaTask;

  QuotaTaskObserver();
  virtual ~QuotaTaskObserver();

  void RegisterTask(QuotaTask* task);
  void UnregisterTask(QuotaTask* task);

  std::set<raw_ptr<QuotaTask, SetExperimental>> running_quota_tasks_;
};
}

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_TASK_H_
