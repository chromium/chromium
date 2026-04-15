// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "v8/include/v8-isolate.h"

namespace blink {

FrameOrWorkerScheduler::LifecycleObserverHandle::LifecycleObserverHandle(
    FrameOrWorkerScheduler* scheduler,
    ObserverType observer_type,
    OnLifecycleStateChangedCallback callback)
    : scheduler_(scheduler->GetWeakPtr()),
      observer_type_(observer_type),
      callback_(std::move(callback)) {}

FrameOrWorkerScheduler::LifecycleObserverHandle::~LifecycleObserverHandle() {
  if (scheduler_)
    scheduler_->RemoveLifecycleObserver(this);
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::
    SchedulingAffectingFeatureHandle(
        SchedulingPolicy::Feature feature,
        SchedulingPolicy policy,
        SourceLocation* source_location,
        base::WeakPtr<FrameOrWorkerScheduler> scheduler)
    : feature_(feature),
      policy_(policy),
      feature_and_js_location_(feature, source_location),
      scheduler_(std::move(scheduler)) {
  if (!scheduler_)
    return;
  scheduler_->OnStartedUsingNonStickyFeature(feature_, policy_, source_location,
                                             this);
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::
    SchedulingAffectingFeatureHandle(SchedulingAffectingFeatureHandle&& other)
    : feature_(other.feature_),
      policy_(std::move(other.policy_)),
      feature_and_js_location_(other.feature_and_js_location_),
      scheduler_(std::move(other.scheduler_)) {
  other.scheduler_ = nullptr;
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle&
FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::operator=(
    SchedulingAffectingFeatureHandle&& other) {
  feature_ = other.feature_;
  policy_ = std::move(other.policy_);
  feature_and_js_location_ = other.feature_and_js_location_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
  return *this;
}

SchedulingPolicy
FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::GetPolicy() const {
  return policy_;
}

SchedulingPolicy::Feature
FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::GetFeature() const {
  return feature_;
}

const FeatureAndJSLocationBlockingBFCache& FrameOrWorkerScheduler::
    SchedulingAffectingFeatureHandle::GetFeatureAndJSLocationBlockingBFCache()
        const {
  return feature_and_js_location_;
}

FrameOrWorkerScheduler::FrameOrWorkerScheduler() {}

FrameOrWorkerScheduler::~FrameOrWorkerScheduler() {
  weak_factory_.InvalidateWeakPtrs();
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
FrameOrWorkerScheduler::RegisterFeature(SchedulingPolicy::Feature feature,
                                        SchedulingPolicy policy) {
  DCHECK(!scheduler::IsFeatureSticky(feature));
  // Check if V8 is currently running an isolate.
  // CaptureSourceLocation() detects the location of JS blocking BFCache if JS
  // is running.
  if (v8::Isolate::TryGetCurrent()) {
    return SchedulingAffectingFeatureHandle(feature, policy,
                                            CaptureSourceLocation(),
                                            GetFrameOrWorkerSchedulerWeakPtr());
  }
  return SchedulingAffectingFeatureHandle(feature, policy, nullptr,
                                          GetFrameOrWorkerSchedulerWeakPtr());
}

void FrameOrWorkerScheduler::RegisterStickyFeature(
    SchedulingPolicy::Feature feature,
    SchedulingPolicy policy) {
  DCHECK(scheduler::IsFeatureSticky(feature));
  auto* source_location = CaptureSourceLocation();
  if (source_location && !source_location->IsUnknown()) {
    OnStartedUsingStickyFeature(feature, policy, source_location);
  } else {
    OnStartedUsingStickyFeature(feature, policy, nullptr);
  }
}

std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>
FrameOrWorkerScheduler::AddLifecycleObserver(
    ObserverType type,
    OnLifecycleStateChangedCallback callback) {
  callback.Run(CalculateLifecycleState(type));
  auto handle = base::WrapUnique(
      new LifecycleObserverHandle(this, type, std::move(callback)));
  lifecycle_observers_.AddObserver(handle.get());
  return handle;
}

void FrameOrWorkerScheduler::RemoveLifecycleObserver(
    LifecycleObserverHandle* handle) {
  CHECK(handle);
  DCHECK(lifecycle_observers_.HasObserver(handle));
  lifecycle_observers_.RemoveObserver(handle);
}

void FrameOrWorkerScheduler::NotifyLifecycleObservers() {
  for (auto& observer : lifecycle_observers_) {
    observer.callback_.Run(CalculateLifecycleState(observer.observer_type_));
  }
}

base::WeakPtr<FrameOrWorkerScheduler> FrameOrWorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
