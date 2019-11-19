// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

FrameOrWorkerScheduler::LifecycleObserverHandle::LifecycleObserverHandle(
    FrameOrWorkerScheduler* scheduler,
    Observer* observer)
    : scheduler_(scheduler->GetWeakPtr()), observer_(observer) {}

FrameOrWorkerScheduler::LifecycleObserverHandle::~LifecycleObserverHandle() {
  if (scheduler_)
    scheduler_->RemoveLifecycleObserver(observer_);
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
  DCHECK(!SchedulingPolicy::IsFeatureSticky(feature));
  // We reset feature sets upon frame navigation, so having a document-bound
  // weak pointer ensures that the feature handle associated with previous
  // document can't influence the new one.
  return SchedulingAffectingFeatureHandle(feature, policy,
                                          GetDocumentBoundWeakPtr());
}

void FrameOrWorkerScheduler::RegisterStickyFeature(
    SchedulingPolicy::Feature feature,
    SchedulingPolicy policy) {
  DCHECK(SchedulingPolicy::IsFeatureSticky(feature));
  OnStartedUsingFeature(feature, policy);
}

std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>
FrameOrWorkerScheduler::AddLifecycleObserver(ObserverType type,
                                             Observer* observer) {
  DCHECK(observer);
  observer->OnLifecycleStateChanged(CalculateLifecycleState(type));
  lifecycle_observers_.Set(observer, type);
  return std::make_unique<LifecycleObserverHandle>(this, observer);
}

void FrameOrWorkerScheduler::RemoveLifecycleObserver(Observer* observer) {
  DCHECK(observer);
  const auto found = lifecycle_observers_.find(observer);
  DCHECK(lifecycle_observers_.end() != found);
  lifecycle_observers_.erase(found);
}

void FrameOrWorkerScheduler::NotifyLifecycleObservers() {
  for (const auto& observer : lifecycle_observers_) {
    observer.key->OnLifecycleStateChanged(
        CalculateLifecycleState(observer.value));
  }
}

base::WeakPtr<FrameOrWorkerScheduler>
FrameOrWorkerScheduler::GetDocumentBoundWeakPtr() {
  return nullptr;
}

base::WeakPtr<FrameOrWorkerScheduler> FrameOrWorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
