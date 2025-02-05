// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_

// Forwarding header that imports types from ::performance_scenarios into
// ::blink::performance_scenarios.
//
// TODO(crbug.com/365586676): Convert all callers to use
// components/performance_manager/scenario_api and delete this.

#include "base/compiler_specific.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"  // IWYU pragma: export

namespace blink::performance_scenarios {

using InputScenario = ::performance_scenarios::InputScenario;
using LoadingScenario = ::performance_scenarios::LoadingScenario;
using RefCountedScenarioMapping =
    ::performance_scenarios::RefCountedScenarioMapping;
using ScenarioScope = ::performance_scenarios::ScenarioScope;
using ScenarioState = ::performance_scenarios::ScenarioState;
using ScopedReadOnlyScenarioMemory =
    ::performance_scenarios::ScopedReadOnlyScenarioMemory;
template <typename T>
using SharedAtomicRef = ::performance_scenarios::SharedAtomicRef<T>;

// If not inlined these function shims will lead to duplicate symbol errors.

ALWAYS_INLINE SharedAtomicRef<InputScenario> GetInputScenario(
    ScenarioScope scope) {
  return ::performance_scenarios::GetInputScenario(scope);
}

ALWAYS_INLINE SharedAtomicRef<LoadingScenario> GetLoadingScenario(
    ScenarioScope scope) {
  return ::performance_scenarios::GetLoadingScenario(scope);
}

}  // namespace blink::performance_scenarios

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_SCENARIOS_H_
