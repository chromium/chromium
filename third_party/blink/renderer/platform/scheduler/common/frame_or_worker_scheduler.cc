// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/not_fatal_until.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

// Returns whether features::kRegisterJSSourceLocationBlockingBFCache is
// enabled.
bool IsRegisterJSSourceLocationBlockingBFCache() {
  return base::FeatureList::IsEnabled(
      blink::features::kRegisterJSSourceLocationBlockingBFCache);
}

}  // namespace

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
        std::unique_ptr<SourceLocation> source_location,
        base::WeakPtr<FrameOrWorkerScheduler> scheduler)
    : feature_(feature),
      policy_(policy),
      feature_and_js_location_(feature, source_location.get()),
      scheduler_(std::move(scheduler)) {
  if (!scheduler_)
    return;
  scheduler_->OnStartedUsingNonStickyFeature(feature_, policy_,
                                             std::move(source_location), this);
}

FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle::
    SchedulingAffectingFeatureHandle(SchedulingAffectingFeatureHandle&& other)
    : feature_(other.feature_),
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
  if (IsRegisterJSSourceLocationBlockingBFCache()) {
    // Check if V8 is currently running an isolate.
    // CaptureSourceLocation() detects the location of JS blocking BFCache if JS
    // is running.
    if (v8::Isolate::TryGetCurrent()) {
      return SchedulingAffectingFeatureHandle(
          feature, policy, CaptureSourceLocation(),
          GetFrameOrWorkerSchedulerWeakPtr());
    }
  }
  return SchedulingAffectingFeatureHandle(feature, policy, nullptr,
                                          GetFrameOrWorkerSchedulerWeakPtr());
}

void FrameOrWorkerScheduler::RegisterStickyFeature(
    SchedulingPolicy::Feature feature,
    SchedulingPolicy policy) {
  DCHECK(scheduler::IsFeatureSticky(feature));
  if (IsRegisterJSSourceLocationBlockingBFCache() &&
      v8::Isolate::TryGetCurrent()) {
    // CaptureSourceLocation() detects the location of JS blocking BFCache if JS
    // is running.
    OnStartedUsingStickyFeature(feature, policy, CaptureSourceLocation());
  } else {
    OnStartedUsingStickyFeature(feature, policy, nullptr);
  }
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
  CHECK(lifecycle_observers_.end() != found, base::NotFatalUntil::M130);
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
