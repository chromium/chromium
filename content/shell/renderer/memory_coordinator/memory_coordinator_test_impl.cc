// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/memory_coordinator/memory_coordinator_test_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void MemoryCoordinatorTestImpl::Bind(
    mojo::PendingReceiver<mojom::MemoryCoordinatorTest> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MemoryCoordinatorTestImpl>(),
                              std::move(receiver));
}

MemoryCoordinatorTestImpl::MemoryCoordinatorTestImpl() = default;

MemoryCoordinatorTestImpl::~MemoryCoordinatorTestImpl() = default;

void MemoryCoordinatorTestImpl::RegisterConsumer(
    const std::string& name,
    base::MemoryConsumerTraits traits,
    mojo::PendingRemote<mojom::MemoryCoordinatorTestClient> client) {
  consumers_.insert(std::make_unique<TestMemoryConsumer>(this, name, traits,
                                                         std::move(client)));
}

void MemoryCoordinatorTestImpl::RemoveConsumer(TestMemoryConsumer* consumer) {
  auto it = consumers_.find(consumer);
  if (it != consumers_.end()) {
    consumers_.erase(it);
  }
}

// MemoryCoordinatorTestImpl::TestMemoryConsumer -------------------------------

MemoryCoordinatorTestImpl::TestMemoryConsumer::TestMemoryConsumer(
    MemoryCoordinatorTestImpl* parent,
    const std::string& name,
    base::MemoryConsumerTraits traits,
    mojo::PendingRemote<mojom::MemoryCoordinatorTestClient> client)
    : parent_(parent),
      client_(std::move(client)),
      registration_(name, traits, this) {
  client_.set_disconnect_handler(base::BindOnce(
      &TestMemoryConsumer::OnConnectionError, base::Unretained(this)));
  // Ensure any already assigned memory limit is honored.
  if (memory_limit() != base::MemoryConsumer::kDefaultMemoryLimit) {
    OnUpdateMemoryLimit();
  }
}

MemoryCoordinatorTestImpl::TestMemoryConsumer::~TestMemoryConsumer() = default;

void MemoryCoordinatorTestImpl::TestMemoryConsumer::OnUpdateMemoryLimit() {
  client_->OnUpdateMemoryLimit(base::MemoryConsumer::memory_limit());
}

void MemoryCoordinatorTestImpl::TestMemoryConsumer::OnReleaseMemory() {
  client_->OnReleaseMemory();
}

void MemoryCoordinatorTestImpl::TestMemoryConsumer::OnConnectionError() {
  parent_->RemoveConsumer(this);
}

}  // namespace content
