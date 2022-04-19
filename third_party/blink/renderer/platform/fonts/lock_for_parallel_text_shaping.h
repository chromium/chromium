// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LOCK_FOR_PARALLEL_TEXT_SHAPING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LOCK_FOR_PARALLEL_TEXT_SHAPING_H_

#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace blink {

// This is empty lockable object for platforms not support parallel text
// shaping to avoid performance regression regarding lock. For platforms
// supports parallel text shaping will have following:
//  using LockForParallelTextShaping = base::Lock;
//  using AutoLockForParallelTextShaping = base::AutoLock;
//
class LOCKABLE LockForParallelTextShaping final {
 public:
  LockForParallelTextShaping() = default;
  ~LockForParallelTextShaping() = default;

  LockForParallelTextShaping(const LockForParallelTextShaping&) = delete;
  LockForParallelTextShaping& operator=(const LockForParallelTextShaping&) =
      delete;

  void Acquire() EXCLUSIVE_LOCK_FUNCTION() {}
  void Release() UNLOCK_FUNCTION() {}
  bool Try() EXCLUSIVE_TRYLOCK_FUNCTION(true) { return true; }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {}
};

using AutoLockForParallelTextShaping =
    base::internal::BasicAutoLock<LockForParallelTextShaping>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_LOCK_FOR_PARALLEL_TEXT_SHAPING_H_
