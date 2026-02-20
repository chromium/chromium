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
// static
mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
ChildMemoryCoordinator::BindAndPassReceiver() {
  return Get().browser_memory_coordinator_bridge_.BindAndPassReceiver();
}

}  // namespace content
