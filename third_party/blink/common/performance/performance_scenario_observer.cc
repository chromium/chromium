// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/performance/performance_scenario_observer.h"

#include <atomic>
#include <utility>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace blink::performance_scenarios {

namespace {

// The global pointers to PerformanceScenarioObserverLists are written from one
// thread, but read from several, so the pointers must be accessed under a lock.
// (As well as the pointed-to object having an atomic refcount.)
class LockedObserverListPtr {
 public:
  LockedObserverListPtr() = default;
  ~LockedObserverListPtr() = default;

  LockedObserverListPtr(const LockedObserverListPtr&) = delete;
  LockedObserverListPtr operator=(const LockedObserverListPtr&) = delete;

  // Returns a copy of the pointer.
  scoped_refptr<PerformanceScenarioObserverList> Get() {
    base::AutoLock lock(lock_);
    return observer_list_;
  }

  // Writes `observer_list` to the pointer, and returns the previous value.
  scoped_refptr<PerformanceScenarioObserverList> Exchange(
      scoped_refptr<PerformanceScenarioObserverList> observer_list) {
    base::AutoLock lock(lock_);
    return std::exchange(observer_list_, std::move(observer_list));
  }

 private:
  base::Lock lock_;
  scoped_refptr<PerformanceScenarioObserverList> observer_list_
      GUARDED_BY(lock_);
};

LockedObserverListPtr& GetLockedObserverListPtrForScope(ScenarioScope scope) {
  static base::NoDestructor<LockedObserverListPtr>
      current_process_observer_list;
  static base::NoDestructor<LockedObserverListPtr> global_observer_list;
  switch (scope) {
    case ScenarioScope::kCurrentProcess:
      return *current_process_observer_list;
    case ScenarioScope::kGlobal:
      return *global_observer_list;
  }
  NOTREACHED();
}

}  // namespace

// static
scoped_refptr<PerformanceScenarioObserverList>
PerformanceScenarioObserverList::GetForScope(ScenarioScope scope) {
  return GetLockedObserverListPtrForScope(scope).Get();
}

void PerformanceScenarioObserverList::AddObserver(
    PerformanceScenarioObserver* observer) {
  observers_->AddObserver(observer);
}

void PerformanceScenarioObserverList::RemoveObserver(
    PerformanceScenarioObserver* observer) {
  observers_->RemoveObserver(observer);
}

void PerformanceScenarioObserverList::NotifyIfScenarioChanged(
    base::Location location) {
  {
    base::AutoLock lock(loading_lock_);
    LoadingScenario loading_scenario =
        GetLoadingScenario(scope_)->load(std::memory_order_relaxed);
    if (loading_scenario != last_loading_scenario_) {
      observers_->Notify(location,
                         &PerformanceScenarioObserver::OnLoadingScenarioChanged,
                         scope_, last_loading_scenario_, loading_scenario);
      last_loading_scenario_ = loading_scenario;
    }
  }
  {
    base::AutoLock lock(input_lock_);
    InputScenario input_scenario =
        GetInputScenario(scope_)->load(std::memory_order_relaxed);
    if (input_scenario != last_input_scenario_) {
      observers_->Notify(location,
                         &PerformanceScenarioObserver::OnInputScenarioChanged,
                         scope_, last_input_scenario_, input_scenario);
      last_input_scenario_ = input_scenario;
    }
  }
}

// static
void PerformanceScenarioObserverList::NotifyAllScopes(base::Location location) {
  if (auto current_process_observers =
          GetForScope(ScenarioScope::kCurrentProcess)) {
    current_process_observers->NotifyIfScenarioChanged(location);
  }
  if (auto global_observers = GetForScope(ScenarioScope::kGlobal)) {
    global_observers->NotifyIfScenarioChanged(location);
  }
}

// static
void PerformanceScenarioObserverList::CreateForScope(
    base::PassKey<ScopedReadOnlyScenarioMemory>,
    ScenarioScope scope) {
  auto old_ptr = GetLockedObserverListPtrForScope(scope).Exchange(
      base::WrapRefCounted(new PerformanceScenarioObserverList(scope)));
  CHECK(!old_ptr);
}

// static
void PerformanceScenarioObserverList::DestroyForScope(
    base::PassKey<ScopedReadOnlyScenarioMemory>,
    ScenarioScope scope) {
  // Drop the main owning reference. Callers of GetForScope() might still have
  // references, but no new caller can obtain a reference.
  auto old_ptr = GetLockedObserverListPtrForScope(scope).Exchange(nullptr);
  CHECK(old_ptr);
}

PerformanceScenarioObserverList::PerformanceScenarioObserverList(
    ScenarioScope scope)
    : scope_(scope),
      last_loading_scenario_(
          GetLoadingScenario(scope)->load(std::memory_order_relaxed)),
      last_input_scenario_(
          GetInputScenario(scope)->load(std::memory_order_relaxed)) {}

PerformanceScenarioObserverList::~PerformanceScenarioObserverList() = default;

}  // namespace blink::performance_scenarios
