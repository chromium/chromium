// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

// This class manages the states for cooperative scheduling and decides whether
// or not to run a nested loop for reentrant JS execution in cross-site frames.
class PLATFORM_EXPORT CooperativeSchedulingManager {
  USING_FAST_MALLOC(CooperativeSchedulingManager);

 public:
  // Reentrant JS execution is not allowed unless there is an instance of this
  // scoper alive. This is to ensure that reentrant JS execution can only happen
  // in C++ stacks with a simple, known state.
  class PLATFORM_EXPORT AllowedStackScope {
    STACK_ALLOCATED();

   public:
    explicit AllowedStackScope(CooperativeSchedulingManager*);
    ~AllowedStackScope();

   private:
    CooperativeSchedulingManager* const cooperative_scheduling_manager_;
  };

  // Returns an shared instance for the current thread.
  static CooperativeSchedulingManager* Instance();

  CooperativeSchedulingManager();
  CooperativeSchedulingManager(const CooperativeSchedulingManager&) = delete;
  CooperativeSchedulingManager& operator=(const CooperativeSchedulingManager&) =
      delete;
  virtual ~CooperativeSchedulingManager() = default;

  // Returns true if reentry is allowed in the current C++ stack.
  bool InAllowedStackScope() const { return allowed_stack_scope_depth_ > 0; }

  // Calls to this should be inserted where nested loops can be run safely.
  // Typically this is is where Blink has not modified any global state that the
  // nested code could touch.
  void Safepoint();

  // The caller is the owner of the |clock|. The |clock| must outlive the
  // CooperativeSchedulingManager.
  void SetTickClockForTesting(const base::TickClock* clock);

  void set_feature_enabled(bool enabled) { feature_enabled_ = enabled; }

 protected:
  virtual void RunNestedLoop();

 private:
  void EnterAllowedStackScope();
  void LeaveAllowedStackScope();
  void SafepointSlow();

  int allowed_stack_scope_depth_ = 0;
  bool running_nested_loop_ = false;
  base::TimeTicks wait_until_;
  raw_ptr<const base::TickClock> clock_;
  bool feature_enabled_ = true;
};

inline void CooperativeSchedulingManager::Safepoint() {
  if (!InAllowedStackScope())
    return;

  if (clock_->NowTicks() < wait_until_)
    return;

  SafepointSlow();
}

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_
