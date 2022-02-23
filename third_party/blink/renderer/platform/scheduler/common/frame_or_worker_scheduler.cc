// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

#include <memory>
#include <utility>

#include "base/callback.h"

namespace blink {

FrameOrWorkerScheduler::LifecycleObserverHandle::LifecycleObserverHandle(
    FrameOrWorkerScheduler* scheduler)
    : scheduler_(scheduler->GetWeakPtr()) {}

FrameOrWorkerScheduler::LifecycleObserverHandle::~LifecycleObserverHandle() {
  if (scheduler_)
    scheduler_->RemoveLifecycleObserver(this);
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::
    SchedulingAffectingFeatureHandle(
        SchedulingPolicy::Feature feature,
        SchedulingPolicy policy,
        base::WeakPtr<FrameOrWorkerScheduler> scheduler)
    : feature_(feature), policy_(policy), scheduler_(std::move(scheduler)) {
  if (!scheduler_)
    return;
  scheduler_->OnStartedUsingFeature(feature_, policy_);
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::
    SchedulingAffectingFeatureHandle(SchedulingAffectingFeatureHandle&& other)
    : feature_(other.feature_), scheduler_(std::move(other.scheduler_)) {
  other.scheduler_ = nullptr;
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle&
FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::operator=(
    SchedulingAffectingFeatureHandle&& other) {
  feature_ = other.feature_;
  policy_ = std::move(other.policy_);
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
  return *this;
}

FrameOrWorkerScheduler::FrameOrWorkerScheduler() {}

FrameOrWorkerScheduler::~FrameOrWorkerScheduler() {
  weak_factory_.InvalidateWeakPtrs();
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
FrameOrWorkerScheduler::RegisterFeature(SchedulingPolicy::Feature feature,
                                        SchedulingPolicy policy) {
  DCHECK(!scheduler::IsFeatureSticky(feature));
  return SchedulingAffectingFeatureHandle(
      feature, policy, GetSchedulingAffectingFeatureWeakPtr());
}

void FrameOrWorkerScheduler::RegisterStickyFeature(
    SchedulingPolicy::Feature feature,
    SchedulingPolicy policy) {
  DCHECK(scheduler::IsFeatureSticky(feature));
  OnStartedUsingFeature(feature, policy);
}

std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>
FrameOrWorkerScheduler::AddLifecycleObserver(
    ObserverType type,
    OnLifecycleStateChangedCallback callback) {
  callback.Run(CalculateLifecycleState(type));
  auto handle = std::make_unique<LifecycleObserverHandle>(this);
  lifecycle_observers_.Set(
      handle.get(), std::make_unique<ObserverState>(type, std::move(callback)));
  return handle;
}

void FrameOrWorkerScheduler::RemoveLifecycleObserver(
    LifecycleObserverHandle* handle) {
  DCHECK(handle);
  const auto found = lifecycle_observers_.find(handle);
  DCHECK(lifecycle_observers_.end() != found);
  lifecycle_observers_.erase(found);
}

void FrameOrWorkerScheduler::NotifyLifecycleObservers() {
  for (const auto& observer : lifecycle_observers_) {
    observer.value->GetCallback().Run(
        CalculateLifecycleState(observer.value->GetObserverType()));
  }
}

base::WeakPtr<FrameOrWorkerScheduler> FrameOrWorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

FrameOrWorkerScheduler::ObserverState::ObserverState(
    FrameOrWorkerScheduler::ObserverType observer_type,
    FrameOrWorkerScheduler::OnLifecycleStateChangedCallback callback)
    : observer_type_(observer_type), callback_(callback) {}

FrameOrWorkerScheduler::ObserverState::~ObserverState() = default;

}  // namespace blink
