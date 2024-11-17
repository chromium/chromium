// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "base/synchronization/lock.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace blink::performance_scenarios {

// An observer that watches for changes to values held in
// ScopedReadOnlyScenarioMemory.
class BLINK_COMMON_EXPORT PerformanceScenarioObserver
    : public base::CheckedObserver {
 public:
  // Invoked whenever the given scenario changes for `scope`.
  virtual void OnLoadingScenarioChanged(ScenarioScope scope,
                                        LoadingScenario old_scenario,
                                        LoadingScenario new_scenario) {}
  virtual void OnInputScenarioChanged(ScenarioScope scope,
                                      InputScenario old_scenario,
                                      InputScenario new_scenario) {}
};

// Central list of PerformanceScenarioObservers for a scope, wrapping an
// ObserverListThreadSafe. The lifetime is managed by
// ScopedReadOnlyScenarioMemory on the main thread, but it's refcounted so
// GetForScope() can be called from any sequence. Callers on other sequences
// will extend the lifetime until they drop their reference.
//
// All methods can be called from any sequence.
class BLINK_COMMON_EXPORT PerformanceScenarioObserverList
    : public base::RefCountedThreadSafe<PerformanceScenarioObserverList> {
 public:
  // Returns the object that notifies observers for `scope`, or nullptr if no
  // ScopedReadOnlyScenarioMemory exists for `scope`.
  static scoped_refptr<PerformanceScenarioObserverList> GetForScope(
      ScenarioScope scope);

  PerformanceScenarioObserverList(const PerformanceScenarioObserverList&) =
      delete;
  PerformanceScenarioObserverList& operator=(
      const PerformanceScenarioObserverList&) = delete;

  // Adds `observer` to the list. Can be called on any sequence. The observer
  // will be notified on the calling sequence.
  void AddObserver(PerformanceScenarioObserver* observer);

  // Removes `observer` from the list. Can be called on any sequence.
  void RemoveObserver(PerformanceScenarioObserver* observer);

  // Notifies observers of scenarios that have changed for this scope since the
  // last call.
  void NotifyIfScenarioChanged(
      base::Location location = base::Location::Current());

  // Notifies observers for all scopes of scenarios that have changed since the
  // last call.
  static void NotifyAllScopes(
      base::Location location = base::Location::Current());

  // Lets ScopedReadOnlyScenarioMemory create and destroy the notifier for
  // `scope`.
  static void CreateForScope(base::PassKey<ScopedReadOnlyScenarioMemory>,
                             ScenarioScope scope);
  static void DestroyForScope(base::PassKey<ScopedReadOnlyScenarioMemory>,
                              ScenarioScope scope);

 private:
  friend class base::RefCountedThreadSafe<PerformanceScenarioObserverList>;

  explicit PerformanceScenarioObserverList(ScenarioScope scope);
  ~PerformanceScenarioObserverList();

  const ScenarioScope scope_;

  // The last scenario values that were notified.
  base::Lock loading_lock_;
  base::Lock input_lock_;
  LoadingScenario last_loading_scenario_ GUARDED_BY(loading_lock_);
  InputScenario last_input_scenario_ GUARDED_BY(input_lock_);

  using WrappedObserverList = base::ObserverListThreadSafe<
      PerformanceScenarioObserver,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  // WrappedObserverList must be held in a scoped_refptr because its destructor
  // is private, but the pointer should never be reassigned.
  const scoped_refptr<WrappedObserverList> observers_ =
      base::MakeRefCounted<WrappedObserverList>();
};

}  // namespace blink::performance_scenarios

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_
