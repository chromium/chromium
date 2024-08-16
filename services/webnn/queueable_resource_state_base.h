// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_BASE_H_
#define SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_BASE_H_

#include <queue>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace webnn {

class ResourceTask;

// A handle to a resource which may be queued in a `ResourceTask`. This class is
// reference counted so that operations that are in progress can keep the
// resources they are using alive until they complete. This class may not be
// passed between threads.
//
// This class should not be extended directly. Please extend the
// `QueueableResourceState` class instead.
class QueueableResourceStateBase
    : public base::RefCounted<QueueableResourceStateBase> {
 public:
  bool CanLock(bool exclusive) const;
  void Lock(bool exclusive);
  void Unlock();

  void EnqueueTask(scoped_refptr<ResourceTask> task);

  ResourceTask* PeekTask() const;
  scoped_refptr<ResourceTask> PopTask();

  bool IsLockedShared() const;
  bool IsLockedExclusive() const;

 protected:
  QueueableResourceStateBase();
  virtual ~QueueableResourceStateBase();

  SEQUENCE_CHECKER(sequence_checker_);

  enum class State {
    kUnlocked,
    kLockedShared,
    kLockedExclusive,
  };
  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kUnlocked;

 private:
  friend class base::RefCounted<QueueableResourceStateBase>;

  std::queue<scoped_refptr<ResourceTask>> waiting_tasks_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_QUEUEABLE_RESOURCE_STATE_BASE_H_
