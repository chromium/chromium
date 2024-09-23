// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_H_

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// Ref counted wrapper for base::Lock.
class GPU_GLES2_EXPORT RefCountedLock
    : public base::RefCountedThreadSafe<RefCountedLock> {
 public:
  RefCountedLock();

  // Disallow copy and assign.
  RefCountedLock(const RefCountedLock&) = delete;
  RefCountedLock& operator=(const RefCountedLock&) = delete;

  virtual base::Lock* GetDrDcLockPtr();
  virtual void AssertAcquired();

 protected:
  virtual ~RefCountedLock();

 private:
  friend class base::RefCountedThreadSafe<RefCountedLock>;

  base::Lock lock_;
};

// Helper class for handling RefCountedLock for drdc usage.
class GPU_GLES2_EXPORT RefCountedLockHelperDrDc {
 public:
  explicit RefCountedLockHelperDrDc(scoped_refptr<RefCountedLock> lock);
  ~RefCountedLockHelperDrDc();

  base::Lock* GetDrDcLockPtr() const {
    return lock_ ? lock_->GetDrDcLockPtr() : nullptr;
  }

  const scoped_refptr<RefCountedLock>& GetDrDcLock() { return lock_; }

  void AssertAcquiredDrDcLock() const {
    if (lock_)
      lock_->AssertAcquired();
  }

  std::optional<base::AutoLockMaybe> GetScopedDrDcLock() const {
    return std::optional<base::AutoLockMaybe>(GetDrDcLockPtr());
  }

 private:
  mutable scoped_refptr<RefCountedLock> lock_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_H_
