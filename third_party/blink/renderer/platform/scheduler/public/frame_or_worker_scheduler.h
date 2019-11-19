// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_

#include "base/memory/weak_ptr.h"
#include "base/util/type_safety/strong_alias.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {
class FrameScheduler;

// This is the base class of FrameScheduler and WorkerScheduler.
class PLATFORM_EXPORT FrameOrWorkerScheduler {
  USING_FAST_MALLOC(FrameOrWorkerScheduler);

 public:
  // Observer type that regulates conditions to invoke callbacks.
  enum class ObserverType { kLoader, kWorkerScheduler };

  // Observer interface to receive scheduling policy change events.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notified when throttling state is changed. May be called consecutively
    // with the same value.
    virtual void OnLifecycleStateChanged(
        scheduler::SchedulingLifecycleState) = 0;
  };

  class PLATFORM_EXPORT LifecycleObserverHandle {
    USING_FAST_MALLOC(LifecycleObserverHandle);

   public:
    LifecycleObserverHandle(FrameOrWorkerScheduler* scheduler,
                            Observer* observer);
    ~LifecycleObserverHandle();

   private:
    base::WeakPtr<FrameOrWorkerScheduler> scheduler_;
    Observer* observer_;

    DISALLOW_COPY_AND_ASSIGN(LifecycleObserverHandle);
  };

  // RAII handle which should be kept alive as long as the feature is active
  // and the policy should be applied.
  class PLATFORM_EXPORT SchedulingAffectingFeatureHandle {
    DISALLOW_NEW();

   public:
    SchedulingAffectingFeatureHandle() = default;
    SchedulingAffectingFeatureHandle(SchedulingAffectingFeatureHandle&&);
    inline ~SchedulingAffectingFeatureHandle() { reset(); }

    SchedulingAffectingFeatureHandle& operator=(
        SchedulingAffectingFeatureHandle&&);

    inline void reset() {
      if (scheduler_)
        scheduler_->OnStoppedUsingFeature(feature_, policy_);
      scheduler_ = nullptr;
    }

   private:
    friend class FrameOrWorkerScheduler;

    SchedulingAffectingFeatureHandle(SchedulingPolicy::Feature feature,
                                     SchedulingPolicy policy,
                                     base::WeakPtr<FrameOrWorkerScheduler>);

    SchedulingPolicy::Feature feature_ = SchedulingPolicy::Feature::kMaxValue;
    SchedulingPolicy policy_;
    base::WeakPtr<FrameOrWorkerScheduler> scheduler_;

    DISALLOW_COPY_AND_ASSIGN(SchedulingAffectingFeatureHandle);
  };

  virtual ~FrameOrWorkerScheduler();

  using Preempted = util::StrongAlias<class PreemptedTag, bool>;
  // Stops any tasks from running while we yield and run a nested loop.
  virtual void SetPreemptedForCooperativeScheduling(Preempted) = 0;

  // Notifies scheduler that this execution context has started using a feature
  // which impacts scheduling decisions.
  // When the feature stops being used, this handle should be destroyed.
  //
  // Usage:
  // handle = scheduler->RegisterFeature(
  //     kYourFeature, { SchedulingPolicy::DisableSomething() });
  SchedulingAffectingFeatureHandle RegisterFeature(
      SchedulingPolicy::Feature feature,
      SchedulingPolicy policy) WARN_UNUSED_RESULT;

  // Register a feature which is used for the rest of the lifetime of
  // the document and can't be unregistered.
  // The policy is reset when the main frame navigates away from the current
  // document.
  void RegisterStickyFeature(SchedulingPolicy::Feature feature,
                             SchedulingPolicy policy);

  // Adds an Observer instance to be notified on scheduling policy changed.
  // When an Observer is added, the initial state will be notified synchronously
  // through the Observer interface.
  // A RAII handle is returned and observer is unregistered when the handle is
  // destroyed.
  std::unique_ptr<LifecycleObserverHandle> AddLifecycleObserver(ObserverType,
                                                                Observer*)
      WARN_UNUSED_RESULT;

  virtual FrameScheduler* ToFrameScheduler() { return nullptr; }

 protected:
  FrameOrWorkerScheduler();

  void NotifyLifecycleObservers();

  virtual scheduler::SchedulingLifecycleState CalculateLifecycleState(
      ObserverType) const {
    return scheduler::SchedulingLifecycleState::kNotThrottled;
  }

  virtual void OnStartedUsingFeature(SchedulingPolicy::Feature feature,
                                     const SchedulingPolicy& policy) = 0;
  virtual void OnStoppedUsingFeature(SchedulingPolicy::Feature feature,
                                     const SchedulingPolicy& policy) = 0;

  virtual base::WeakPtr<FrameOrWorkerScheduler> GetDocumentBoundWeakPtr();

  base::WeakPtr<FrameOrWorkerScheduler> GetWeakPtr();

 private:
  void RemoveLifecycleObserver(Observer* observer);

  // Observers are not owned by the scheduler.
  HashMap<Observer*, ObserverType> lifecycle_observers_;
  base::WeakPtrFactory<FrameOrWorkerScheduler> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_OR_WORKER_SCHEDULER_H_
