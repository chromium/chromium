// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/tracing_agent_observer_manager.h"

#include "base/check.h"

namespace tracing {

namespace {
TracingAgentObserverManager* g_instance = nullptr;
}  // namespace

// static
void TracingAgentObserverManager::SetInstance(
    TracingAgentObserverManager* instance) {
  g_instance = instance;
}

// static
TracingAgentObserverManager* TracingAgentObserverManager::GetInstance() {
  return g_instance;
}

}  // namespace tracing
