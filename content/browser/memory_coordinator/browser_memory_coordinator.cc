// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

namespace {

BrowserMemoryCoordinator* g_instance = nullptr;

}  // namespace

// static
BrowserMemoryCoordinator& BrowserMemoryCoordinator::Get() {
  CHECK(g_instance);
  return *g_instance;
}

BrowserMemoryCoordinator::BrowserMemoryCoordinator() {
  CHECK(!g_instance);
  g_instance = this;

  policy_manager_.AddPolicy(&memory_pressure_listener_policy_);
}

BrowserMemoryCoordinator::~BrowserMemoryCoordinator() {
  policy_manager_.RemovePolicy(&memory_pressure_listener_policy_);

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void BrowserMemoryCoordinator::AddDiagnosticObserver(
    MemoryCoordinatorPolicyManager::DiagnosticObserver* observer) {
  policy_manager_.AddDiagnosticObserver(observer);
  ++diagnostic_observer_count_;

  // This is the first diagnostic observer added. Must enable diagnostics.
  if (diagnostic_observer_count_ == 1u) {
    for (auto& [id, host] : hosts_) {
      host->EnableDiagnosticsReporting();
    }
  }
}

void BrowserMemoryCoordinator::RemoveDiagnosticObserver(
    MemoryCoordinatorPolicyManager::DiagnosticObserver* observer) {
  policy_manager_.RemoveDiagnosticObserver(observer);
  CHECK_GT(diagnostic_observer_count_, 0u);
  --diagnostic_observer_count_;

  // This is the last diagnostic observer removed. Must disable diagnostics.
  if (diagnostic_observer_count_ == 0u) {
    for (auto& [id, host] : hosts_) {
      host->DisableDiagnosticsReporting();
    }
  }
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

void BrowserMemoryCoordinator::Bind(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
  auto [it, inserted] = hosts_.try_emplace(child_process_id);
  if (!inserted) {
    mojo::ReportBadMessage("Duplicate MemoryCoordinator host registration");
    return;
  }

  it->second = std::make_unique<ChildMemoryConsumerRegistryHost>(
      policy_manager_, process_type, child_process_id, std::move(receiver),
      base::BindOnce(&BrowserMemoryCoordinator::OnHostDisconnected,
                     base::Unretained(this), child_process_id));

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  if (diagnostic_observer_count_ > 0) {
    it->second->EnableDiagnosticsReporting();
  }
#endif
}

void BrowserMemoryCoordinator::NotifyReleaseMemoryForTesting() {
  policy_manager_.NotifyReleaseMemoryForTesting();
}

void BrowserMemoryCoordinator::NotifyUpdateMemoryLimitForTesting(
    int percentage) {
  policy_manager_.NotifyUpdateMemoryLimitForTesting(percentage);
}

void BrowserMemoryCoordinator::OnHostDisconnected(
    ChildProcessId child_process_id) {
  size_t removed = hosts_.erase(child_process_id);
  CHECK_EQ(removed, 1u);
}

}  // namespace content
