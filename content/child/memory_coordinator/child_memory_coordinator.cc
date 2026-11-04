// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_coordinator.h"

#include "base/check.h"

namespace content {

namespace {

ChildMemoryCoordinator* g_instance = nullptr;

}  // namespace

// static
ChildMemoryCoordinator& ChildMemoryCoordinator::Get() {
  CHECK(g_instance);
  return *g_instance;
}

ChildMemoryCoordinator::ChildMemoryCoordinator() {
  CHECK(!g_instance);
  g_instance = this;
}

ChildMemoryCoordinator::~ChildMemoryCoordinator() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void ChildMemoryCoordinator::InitializePolicies() {
  // Ensures that no consumer is notified before the "disabled" overrides are
  // applied.
  CHECK(!policy_manager_.HasPolicies());

  registry_.Get().OnFeatureListInitialized();

  browser_memory_coordinator_bridge_registration_.emplace(
      policy_manager_, browser_memory_coordinator_bridge_);
  memory_pressure_listener_policy_.emplace(policy_manager_);
  memory_pressure_listener_policy_registration_.emplace(
      policy_manager_, *memory_pressure_listener_policy_);
}

// static
mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
ChildMemoryCoordinator::BindAndPassReceiver() {
  ChildMemoryCoordinator& coordinator = Get();
  // The bridge must be registered with the policy manager before it can
  // receive updates from the browser process.
  CHECK(coordinator.browser_memory_coordinator_bridge_registration_);
  return coordinator.browser_memory_coordinator_bridge_.BindAndPassReceiver();
}

}  // namespace content
