// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "content/browser/memory_coordinator/browser_memory_coordinator.h"
#include "content/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/memory_coordinator/memory_coordinator_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestPolicy : public MemoryCoordinatorPolicy,
                   public MemoryCoordinatorPolicyManager::Observer {
 public:
  explicit TestPolicy(MemoryCoordinatorPolicyManager& manager)
      : MemoryCoordinatorPolicy(manager) {}

  void UpdateConsumersWithFilter(
      MemoryCoordinatorPolicyManager::ConsumerFilter filter,
      std::optional<int> percentage,
      bool release_memory) {
    manager().UpdateConsumers(this, filter, percentage, release_memory);
  }

  bool WaitUntilRegistered(const std::string& name) {
    uint32_t consumer_id = base::PersistentHash(name);
    return base::test::RunUntil(
        [&]() { return registered_consumers_.contains(consumer_id); });
  }

  // MemoryCoordinatorPolicyManager::Observer:
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override {
    auto [it, inserted] = registered_consumers_.insert(consumer_id);
    CHECK(inserted);
  }

  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override {
    size_t removed = registered_consumers_.erase(consumer_id);
    CHECK_EQ(removed, 1u);
  }

 private:
  base::flat_set<uint32_t> registered_consumers_;
};

class MemoryCoordinatorBrowserTest : public ContentBrowserTest {
 public:
  class ChildConsumerClient : public mojom::MemoryCoordinatorTestClient {
   public:
    explicit ChildConsumerClient(
        mojo::PendingReceiver<mojom::MemoryCoordinatorTestClient> receiver)
        : receiver_(this, std::move(receiver)) {}

    ~ChildConsumerClient() override = default;

    // mojom::MemoryCoordinatorTestClient:
    MOCK_METHOD(void, OnUpdateMemoryLimit, (int32_t), (override));
    MOCK_METHOD(void, OnReleaseMemory, (), (override));

   private:
    mojo::Receiver<mojom::MemoryCoordinatorTestClient> receiver_;
  };

  MemoryCoordinatorBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

    GetProcess()->BindReceiver(
        memory_coordinator_test_.BindNewPipeAndPassReceiver());
  }

  std::unique_ptr<ChildConsumerClient> RegisterChildConsumer(
      const std::string& name,
      base::MemoryConsumerTraits traits) {
    mojo::PendingRemote<mojom::MemoryCoordinatorTestClient> client_remote;
    auto receiver = client_remote.InitWithNewPipeAndPassReceiver();
    memory_coordinator_test_->RegisterConsumer(name, traits,
                                               std::move(client_remote));
    return std::make_unique<ChildConsumerClient>(std::move(receiver));
  }

  RenderProcessHost* GetProcess() {
    return shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  }

 protected:
  mojo::Remote<mojom::MemoryCoordinatorTest> memory_coordinator_test_;
};

IN_PROC_BROWSER_TEST_F(MemoryCoordinatorBrowserTest, ChildProcessRegistration) {
  MemoryCoordinatorPolicyManager& manager =
      BrowserMemoryCoordinator::Get().policy_manager_for_testing();
  TestPolicy policy(manager);
  MemoryCoordinatorPolicyRegistration registration(manager, policy);

  // 1. Register two consumers with different traits in the renderer process.
  base::MemoryConsumerTraits traits_a = {
      .estimated_memory_usage =
          base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      .release_gc_references =
          base::MemoryConsumerTraits::ReleaseGCReferences::kYes};

  base::MemoryConsumerTraits traits_b = {
      .estimated_memory_usage =
          base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
      .release_gc_references =
          base::MemoryConsumerTraits::ReleaseGCReferences::kYes};

  std::unique_ptr<ChildConsumerClient> consumer_a =
      RegisterChildConsumer("ConsumerA", traits_a);
  std::unique_ptr<ChildConsumerClient> consumer_b =
      RegisterChildConsumer("ConsumerB", traits_b);

  ASSERT_TRUE(policy.WaitUntilRegistered("ConsumerA"));
  ASSERT_TRUE(policy.WaitUntilRegistered("ConsumerB"));

  // 2. Notify only ConsumerA using a trait unique to it
  // (EstimatedMemoryUsage::kSmall).
  {
    base::RunLoop run_loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, run_loop.QuitClosure());

    EXPECT_CALL(*consumer_a, OnUpdateMemoryLimit(50))
        .WillOnce(base::test::RunOnceClosure(barrier));
    EXPECT_CALL(*consumer_a, OnReleaseMemory())
        .WillOnce(base::test::RunOnceClosure(barrier));
    EXPECT_CALL(*consumer_b, OnUpdateMemoryLimit(50)).Times(0);
    EXPECT_CALL(*consumer_b, OnReleaseMemory()).Times(0);

    policy.UpdateConsumersWithFilter(
        [](uint32_t consumer_id,
           std::optional<base::MemoryConsumerTraits> traits,
           ProcessType process_type, ChildProcessId child_process_id) {
          return traits &&
                 traits->estimated_memory_usage ==
                     base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall;
        },
        /*percentage=*/50, /*release_memory=*/true);
    run_loop.Run();
  }

  // 3. Notify both consumers using a shared trait (ReleaseGCReferences::kYes).
  {
    base::RunLoop run_loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, run_loop.QuitClosure());

    EXPECT_CALL(*consumer_a, OnUpdateMemoryLimit(25))
        .WillOnce(base::test::RunOnceClosure(barrier));
    EXPECT_CALL(*consumer_b, OnUpdateMemoryLimit(25))
        .WillOnce(base::test::RunOnceClosure(barrier));

    policy.UpdateConsumersWithFilter(
        [](uint32_t consumer_id,
           std::optional<base::MemoryConsumerTraits> traits,
           ProcessType process_type, ChildProcessId child_process_id) {
          return traits &&
                 traits->release_gc_references ==
                     base::MemoryConsumerTraits::ReleaseGCReferences::kYes;
        },
        /*percentage=*/25, /*release_memory=*/false);
    run_loop.Run();
  }
}

}  // namespace content
