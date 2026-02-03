// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PERFORMANCE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PERFORMANCE_HELPER_H_

namespace blink::scheduler {

// Manages CPU throttling in coordination with MainThreadSchedulerImpl's
// UseCases.
class PLATFORM_EXPORT PerformanceHelper {
 public:
  using UpdateStateCallback =
      base::RepeatingCallback<void(bool prefer_efficiency)>;

  struct Params {
    base::TimeDelta loading_boost;
    base::TimeDelta scrolling_boost;
    base::TimeDelta input_boost;
    UpdateStateCallback callback;
  };

  enum class BoostType {
    kScroll,       // A main-thread blocking scroll.
    kTapOrTyping,  // Discrete input needing a short burst of responsiveness.
    kPageLoad      // Page load or navigation.
  };

  // Fires the callback one last time with prefer_efficiency = false to undo any
  // throttling.
  ~PerformanceHelper();

  // Initialization. Moves the callback and durations into the helper.
  void Configure(Params params);

  // Indicates that we need to be boosted. Does not call the callback.
  void Add(BoostType type,
           base::TimeTicks now = base::TimeTicks::LowResolutionNow());

  // Compares the current time against the expiration deadline. Calls the
  // callback if the deadline's expired.
  void Check(base::TimeTicks cur = base::TimeTicks::LowResolutionNow());

 private:
  base::TimeDelta GetDuration(BoostType) const;

  Params params_;
  base::TimeTicks expires_at_;
  // Stores the last value passed to the callback for de-bouncing.
  bool boosted_ = true;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PERFORMANCE_HELPER_H_
