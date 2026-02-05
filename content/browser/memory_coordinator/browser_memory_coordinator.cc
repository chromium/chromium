// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator.h"

#include <utility>

#include "base/functional/bind.h"

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
}

BrowserMemoryCoordinator::~BrowserMemoryCoordinator() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BrowserMemoryCoordinator::Bind(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
  auto host = std::make_unique<ChildMemoryConsumerRegistryHost>(
      registry_.Get(), process_type, child_process_id);
  auto* host_ptr = host.get();
  mojo::ReceiverId id = hosts_.Add(std::move(host), std::move(receiver));
  host_ptr->SetDisconnectHandler(base::BindOnce(
      [](mojo::UniqueReceiverSet<mojom::ChildMemoryConsumerRegistryHost>* hosts,
         mojo::ReceiverId id) { hosts->Remove(id); },
      &hosts_, id));
}

}  // namespace content
