// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"

namespace content {

namespace {

BrowserMemoryCoordinatorImpl* g_instance = nullptr;

}  // namespace

// static
BrowserMemoryCoordinator& BrowserMemoryCoordinator::Get() {
  return BrowserMemoryCoordinatorImpl::Get();
}

// static
BrowserMemoryCoordinatorImpl& BrowserMemoryCoordinatorImpl::Get() {
  CHECK(g_instance);
  return *g_instance;
}

BrowserMemoryCoordinatorImpl::BrowserMemoryCoordinatorImpl() {
  CHECK(!g_instance);
  g_instance = this;
}

BrowserMemoryCoordinatorImpl::~BrowserMemoryCoordinatorImpl() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BrowserMemoryCoordinatorImpl::InitializePolicies() {
  CHECK(base::FeatureList::GetInstance());
  CHECK(!zero_limit_policy_);

  if (base::FeatureList::IsEnabled(kMemoryCoordinatorZeroLimit)) {
    zero_limit_policy_.emplace(policy_manager_);
  }
}

MemoryCoordinatorPolicyManager& BrowserMemoryCoordinatorImpl::policy_manager() {
  return policy_manager_;
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void BrowserMemoryCoordinatorImpl::AddDiagnosticObserver(
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

void BrowserMemoryCoordinatorImpl::RemoveDiagnosticObserver(
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

bool BrowserMemoryCoordinatorImpl::Bind(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
  auto [it, inserted] = hosts_.try_emplace(child_process_id);
  if (!inserted) {
    return false;
  }

  it->second = std::make_unique<ChildMemoryConsumerRegistryHost>(
      policy_manager_, process_type, child_process_id, std::move(receiver),
      base::BindOnce(&BrowserMemoryCoordinatorImpl::OnHostDisconnected,
                     base::Unretained(this), child_process_id));

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  if (diagnostic_observer_count_ > 0) {
    it->second->EnableDiagnosticsReporting();
  }
#endif
  return true;
}

void BrowserMemoryCoordinatorImpl::OnHostDisconnected(
    ChildProcessId child_process_id) {
  size_t removed = hosts_.erase(child_process_id);
  CHECK_EQ(removed, 1u);
}

}  // namespace content
