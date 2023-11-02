// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ref_counted_lock.h"

#include "gpu/config/gpu_finch_features.h"

namespace gpu {

RefCountedLock::RefCountedLock() = default;

RefCountedLock::~RefCountedLock() = default;

base::Lock* RefCountedLock::GetDrDcLockPtr() {
  return &lock_;
}

void RefCountedLock::AssertAcquired() {
  lock_.AssertAcquired();
}

RefCountedLockHelperDrDc::RefCountedLockHelperDrDc(
    scoped_refptr<RefCountedLock> lock)
    : lock_(std::move(lock)) {
  // |lock_| should be present if Android Media code needs to be thread-safe and
  // not present otherwise.
  DCHECK_EQ(features::NeedThreadSafeAndroidMedia(), !!lock_);
}

RefCountedLockHelperDrDc::~RefCountedLockHelperDrDc() = default;

}  // namespace gpu
