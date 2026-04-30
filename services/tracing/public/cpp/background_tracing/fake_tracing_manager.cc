// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/fake_tracing_manager.h"

#include <utility>

namespace tracing {

FakeTracingAgentObserverManager::FakeTracingAgentObserverManager() {
  TracingAgentObserverManager::SetInstance(this);
}

FakeTracingAgentObserverManager::~FakeTracingAgentObserverManager() {
  TracingAgentObserverManager::SetInstance(nullptr);
}

FakeNamedTriggerManager::FakeNamedTriggerManager() {
  SetInstance(this);
}

FakeNamedTriggerManager::~FakeNamedTriggerManager() {
  SetInstance(nullptr);
}

bool FakeNamedTriggerManager::DoEmitNamedTrigger(
    const std::string& trigger_name,
    std::optional<int32_t> value,
    uint64_t flow_id) {
  return NotifyObservers(trigger_name, value, flow_id);
}

}  // namespace tracing
