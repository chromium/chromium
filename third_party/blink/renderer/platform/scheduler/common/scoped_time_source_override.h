// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCOPED_TIME_SOURCE_OVERRIDE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCOPED_TIME_SOURCE_OVERRIDE_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink::scheduler {

// The class is a client's handle to TimeSourceOverrideManager that performs
// actual override.
// `CreateDefault()` sets the time source that will be used on any thread by
// default (so should be used by frames and will affect other threads such as
// compositor). `CreateForCurrentThread()` only has effect on the thread it has
// been called on (so can be used on workers without affecting the main page).
class PLATFORM_EXPORT ScopedTimeSourceOverride {
 public:
  class TimeSource {
   public:
    // The methods are marked const only for compatibility with other interfaces
    // that declare same methods.
    virtual base::TimeTicks NowTicks() const = 0;
    virtual base::Time Date() const = 0;
    virtual ~TimeSource() = default;
  };

  static std::unique_ptr<ScopedTimeSourceOverride> CreateDefault(
      TimeSource& time_source);
  static std::unique_ptr<ScopedTimeSourceOverride> CreateForCurrentThread(
      TimeSource& time_source);

  ScopedTimeSourceOverride() = delete;
  ScopedTimeSourceOverride(const ScopedTimeSourceOverride& r) = delete;
  ScopedTimeSourceOverride(ScopedTimeSourceOverride&& r) = delete;
  ~ScopedTimeSourceOverride();

 private:
  explicit ScopedTimeSourceOverride(bool is_default);

  const bool is_default_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCOPED_TIME_SOURCE_OVERRIDE_H_
