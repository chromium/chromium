// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_

// Forwarding header that imports types from ::performance_scenarios into
// ::blink::performance_scenarios.
//
// TODO(crbug.com/365586676): Convert all callers to use
// components/performance_manager/scenario_api and delete this.

#include "components/performance_manager/scenario_api/performance_scenario_observer.h"  // IWYU pragma: export

// PerformanceScenarioObserver uses types from this header in its signature so
// they must be forwarded too.
#include "third_party/blink/public/common/performance/performance_scenarios.h"  // IWYU pragma: export

namespace blink::performance_scenarios {

using PerformanceScenarioObserver =
    ::performance_scenarios::PerformanceScenarioObserver;
using PerformanceScenarioObserverList =
    ::performance_scenarios::PerformanceScenarioObserverList;

}  // namespace blink::performance_scenarios

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIO_OBSERVER_H_
