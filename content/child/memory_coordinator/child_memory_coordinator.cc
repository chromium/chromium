// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_coordinator.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "build/build_config.h"

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
  // The policies are destroyed after this body runs, and removing them would
  // notify consumers. This runs after the thread pool has shut down, so
  // consumers can't receive notifications anymore.
  policy_manager_.PrepareForShutdown();
  g_instance = nullptr;
}

void ChildMemoryCoordinator::InitializePolicies() {
  CHECK(base::FeatureList::GetInstance());
  // Ensures that no consumer is notified before the "disabled" overrides are
  // applied.
  CHECK(!policy_manager_.HasPolicies());

  registry_.Get().OnFeatureListInitialized();

  browser_memory_coordinator_bridge_registration_.emplace(
      policy_manager_, browser_memory_coordinator_bridge_);
  memory_pressure_policy_.emplace(policy_manager_);
  memory_pressure_policy_registration_.emplace(policy_manager_,
                                               *memory_pressure_policy_);
}

void ChildMemoryCoordinator::InitializeProcessTypePolicies(
    ProcessType process_type) {
  // The "disabled" overrides must be applied before any policy can notify
  // consumers.
  CHECK(memory_pressure_policy_registration_);
  CHECK(!process_type_policies_initialized_);
  process_type_policies_initialized_ = true;

  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      background_policy_.emplace(policy_manager_);
      break;
#if BUILDFLAG(IS_ANDROID)
    case PROCESS_TYPE_GPU:
      // On other platforms, the GPU process is never considered backgrounded.
      background_policy_.emplace(policy_manager_);
      break;
#endif
    default:
      return;
  }

  // The renderer and GPU processes feed the ProcessPriorityTracker with the
  // priority they receive from the browser. Notifies of the current priority,
  // if known.
  process_priority_observation_.Observe(ProcessPriorityTracker::GetInstance());
}

void ChildMemoryCoordinator::OnPriorityChanged(
    base::Process::Priority process_priority) {
  background_policy_->OnProcessPriorityChanged(process_priority);
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
