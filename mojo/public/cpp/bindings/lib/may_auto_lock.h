// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAY_AUTO_LOCK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAY_AUTO_LOCK_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/synchronization/lock.h"

namespace mojo {
namespace internal {

// Similar to base::AutoLock, except that it does nothing if |lock| passed into
// the constructor is null.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MayAutoLock {
 public:
  explicit MayAutoLock(std::optional<base::Lock>* lock)
      : lock_(lock->has_value() ? &lock->value() : nullptr) {
    if (lock_)
      lock_->Acquire();
  }

  MayAutoLock(const MayAutoLock&) = delete;
  MayAutoLock& operator=(const MayAutoLock&) = delete;

  ~MayAutoLock() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

 private:
  // `lock_` is not a raw_ptr<...> for performance reasons: on-stack pointer +
  // based on analysis of sampling profiler data.
  RAW_PTR_EXCLUSION base::Lock* lock_;
};

// Similar to base::AutoUnlock, except that it does nothing if |lock| passed
// into the constructor is null.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MayAutoUnlock {
 public:
  explicit MayAutoUnlock(std::optional<base::Lock>* lock)
      : lock_(lock->has_value() ? &lock->value() : nullptr) {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

  MayAutoUnlock(const MayAutoUnlock&) = delete;
  MayAutoUnlock& operator=(const MayAutoUnlock&) = delete;

  ~MayAutoUnlock() {
    if (lock_)
      lock_->Acquire();
  }

 private:
  // `lock_` is not a raw_ptr<...> for performance reasons: on-stack pointer +
  // based on analysis of sampling profiler data.
  RAW_PTR_EXCLUSION base::Lock* lock_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAY_AUTO_LOCK_H_
