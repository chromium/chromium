// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_FOR_TEST_H_
#define GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_FOR_TEST_H_

#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class GPU_GLES2_EXPORT RefCountedLockForTest : public RefCountedLock {
 public:
  RefCountedLockForTest();

  // Disallow copy and assign.
  RefCountedLockForTest(const RefCountedLockForTest&) = delete;
  RefCountedLockForTest& operator=(const RefCountedLockForTest&) = delete;

  base::Lock* GetDrDcLockPtr() override;
  void AssertAcquired() override;

 protected:
  ~RefCountedLockForTest() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_REF_COUNTED_LOCK_FOR_TEST_H_
