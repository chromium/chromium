// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_MEMORY_COORDINATOR_MEMORY_COORDINATOR_TEST_IMPL_H_
#define CONTENT_SHELL_RENDERER_MEMORY_COORDINATOR_MEMORY_COORDINATOR_TEST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "content/shell/common/memory_coordinator/memory_coordinator_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class MemoryCoordinatorTestImpl : public mojom::MemoryCoordinatorTest {
 public:
  static void Bind(
      mojo::PendingReceiver<mojom::MemoryCoordinatorTest> receiver);

  MemoryCoordinatorTestImpl();

  MemoryCoordinatorTestImpl(const MemoryCoordinatorTestImpl&) = delete;
  MemoryCoordinatorTestImpl& operator=(const MemoryCoordinatorTestImpl&) =
      delete;

  ~MemoryCoordinatorTestImpl() override;

  // mojom::MemoryCoordinatorTest:
  void RegisterConsumer(
      const std::string& name,
      base::MemoryConsumerTraits traits,
      mojo::PendingRemote<mojom::MemoryCoordinatorTestClient> client) override;

 private:
  class TestMemoryConsumer final : public base::MemoryConsumer {
   public:
    TestMemoryConsumer(
        MemoryCoordinatorTestImpl* parent,
        const std::string& name,
        base::MemoryConsumerTraits traits,
        mojo::PendingRemote<mojom::MemoryCoordinatorTestClient> client);
    ~TestMemoryConsumer() override;

    // base::MemoryConsumer:
    void OnUpdateMemoryLimit() override;
    void OnReleaseMemory() override;

   private:
    void OnConnectionError();

    raw_ptr<MemoryCoordinatorTestImpl> parent_;
    mojo::Remote<mojom::MemoryCoordinatorTestClient> client_;
    base::MemoryConsumerRegistration registration_;
  };

  void RemoveConsumer(TestMemoryConsumer* consumer);

  base::flat_set<std::unique_ptr<TestMemoryConsumer>, base::UniquePtrComparator>
      consumers_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_MEMORY_COORDINATOR_MEMORY_COORDINATOR_TEST_IMPL_H_
