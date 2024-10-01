// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/common/features.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using RequestWorkletServiceOutcome =
    AuctionProcessManager::RequestWorkletServiceOutcome;

// Alias constants to improve readability.
const size_t kMaxSellerProcesses = AuctionProcessManager::kMaxSellerProcesses;
const size_t kMaxBidderProcesses = AuctionProcessManager::kMaxBidderProcesses;

class TestAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  TestAuctionProcessManager() = default;

  TestAuctionProcessManager(const TestAuctionProcessManager&) = delete;
  const TestAuctionProcessManager& operator=(const TestAuctionProcessManager&) =
      delete;

  ~TestAuctionProcessManager() override = default;

  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& script_source_url,
      const std::optional<GURL>& bidding_wasm_helper_url,
      const std::optional<GURL>& trusted_bidding_signals_url,
      const std::string& trusted_bidding_signals_slot_size_param,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
          permissions_policy_state,
      std::optional<uint16_t> experiment_id,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    NOTREACHED_IN_MIGRATION();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet,
      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& script_source_url,
      const std::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
          permissions_policy_state,
      std::optional<uint16_t> experiment_id,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    NOTREACHED_IN_MIGRATION();
  }

  size_t NumReceivers() {
    // Flush so that any closed pipes are removed. No need to worry about
    // pending creation requests, since this class is called into directly,
    // rather than over a Mojo pipe.
    receiver_set_.FlushForTesting();
    return receiver_set_.size();
  }

  void ClosePipes() {
    receiver_set_.Clear();
    // No wait to flush a closed pipe from the end that was closed. Run until
    // the other side has noticed the pipe was closed instead.
    base::RunLoop().RunUntilIdle();
  }

 private:
  scoped_refptr<WorkletProcess> LaunchProcess(
      const ProcessHandle* process_handle,
      const std::string& display_name) override {
    mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
    receiver_set_.Add(this, service.InitWithNewPipeAndPassReceiver());
    RenderProcessHost* host =
        process_handle->site_instance_for_testing()->GetProcess();
    return base::MakeRefCounted<WorkletProcess>(
        this, /*render_process_host=*/host, std::move(service),
        process_handle->worklet_type(), process_handle->origin(),
        /*uses_shared_process=*/false);
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return frame_site_instance->GetRelatedSiteInstance(worklet_origin.GetURL());
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class AuctionProcessManagerTest
    : public testing::TestWithParam<AuctionProcessManager::WorkletType> {
 protected:
  AuctionProcessManagerTest()
      : site_instance_(SiteInstance::Create(&test_browser_context_)) {}

  void SetUp() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &rph_factory_);
    SiteIsolationPolicy::DisableFlagCachingForTesting();
  }

  void TearDown() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
  }

  // Request a worklet service and expect the request to complete synchronously.
  // There's no async version, since async calls are only triggered by deleting
  // another handle.
  std::unique_ptr<AuctionProcessManager::ProcessHandle>
  GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType worklet_type,
                                const url::Origin& origin) {
    auto process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    EXPECT_TRUE(auction_process_manager_.RequestWorkletService(
        worklet_type, origin, site_instance_, process_handle.get(),
        NeverInvokedClosure()));
    EXPECT_TRUE(process_handle->GetService());
    return process_handle;
  }

  // Requests a process of type GetParam().
  std::unique_ptr<AuctionProcessManager::ProcessHandle> GetServiceExpectSuccess(
      const url::Origin& origin) {
    return GetServiceOfTypeExpectSuccess(GetParam(), origin);
  }

  // Returns the maximum number of processes of type GetParam().
  size_t GetMaxProcesses() const {
    switch (GetParam()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return kMaxSellerProcesses;
      case AuctionProcessManager::WorkletType::kBidder:
        return kMaxBidderProcesses;
    }
  }

  // Returns the number of pending requests of GetParam() type.
  size_t GetPendingRequestsOfParamType() const {
    switch (GetParam()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return auction_process_manager_.GetPendingSellerRequestsForTesting();
      case AuctionProcessManager::WorkletType::kBidder:
        return auction_process_manager_.GetPendingBidderRequestsForTesting();
    }
  }

  base::OnceClosure NeverInvokedClosure() {
    return base::BindOnce(
        []() { ADD_FAILURE() << "This should not be called"; });
  }

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext test_browser_context_;
  MockRenderProcessHostFactory rph_factory_;
  scoped_refptr<SiteInstance> site_instance_;
  TestAuctionProcessManager auction_process_manager_;

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AuctionProcessManagerTest,
    testing::Values(AuctionProcessManager::WorkletType::kSeller,
                    AuctionProcessManager::WorkletType::kBidder));

TEST_P(AuctionProcessManagerTest, Basic) {
  auto seller = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(seller->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

// Make sure requests for different origins don't share processes, nor do
// sellers and bidders.
//
// This test doesn't use the parameterization, but using TEST_F() for a single
// test would require another test fixture, and so would add more complexity
// than it's worth, for only a single unit test.
TEST_P(AuctionProcessManagerTest, MultipleRequestsForDifferentProcesses) {
  auto seller_a = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  auto seller_b = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginB);
  auto buyer_a = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginA);
  auto buyer_b = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginB);

  EXPECT_EQ(4u, auction_process_manager_.NumReceivers());
  EXPECT_NE(seller_a->GetService(), seller_b->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_b->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_b->GetService());
  EXPECT_NE(buyer_a->GetService(), buyer_b->GetService());
}

TEST_P(AuctionProcessManagerTest, MultipleRequestsForSameProcess) {
  // Request 3 processes of the same type for the same origin. All requests
  // should get the same process.
  auto process_a1 = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(process_a1->GetService());
  auto process_a2 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a2->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
  auto process_a3 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a3->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Request process of the other type with the same origin. It should get a
  // different process.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> other_process_a1;
  switch (GetParam()) {
    case AuctionProcessManager::WorkletType::kSeller:
      other_process_a1 = GetServiceOfTypeExpectSuccess(
          AuctionProcessManager::WorkletType::kBidder, kOriginA);
      break;
    case AuctionProcessManager::WorkletType::kBidder:
      other_process_a1 = GetServiceOfTypeExpectSuccess(
          AuctionProcessManager::WorkletType::kSeller, kOriginA);
      break;
  }
  EXPECT_EQ(2u, auction_process_manager_.NumReceivers());
  EXPECT_NE(process_a1->GetService(), other_process_a1->GetService());
}

// Test requesting and releasing worklet processes, exceeding the limit. This
// test does not cover the case of multiple requests sharing the same process,
// which is covered by the next test.
TEST_P(AuctionProcessManagerTest, LimitExceeded) {
  // The list of operations below assumes at least 3 processes are allowed at
  // once.
  CHECK_GE(GetMaxProcesses(), 3u);

  // Operations applied to the process manager. All requests use unique origins,
  // so no need to specify that.
  struct Operation {
    enum class Op {
      // Request the specified number of handle. If there are less than
      // GetMaxProcesses() handles already, expects a process to be immediately
      // assigned. All requests use different origins from every other request.
      kRequestHandles,

      // Destroy a handle with the given index. If the index is less than
      // GetMaxProcesses(), then expect a ProcessHandle to have its callback
      // invoked, if there are more than GetMaxProcesses() already.
      kDestroyHandle,

      // Same as destroy handle, but additionally destroys the next handle that
      // would have been assigned the next available process slot, and makes
      // sure the handle after that one gets a process instead.
      kDestroyHandleAndNextInQueue,
    };

    Op op;

    // Number of handles to request for kRequestHandles operations.
    std::optional<size_t> num_handles;

    // Used for kDestroyHandle and kDestroyHandleAndNextInQueue operations.
    std::optional<size_t> index;

    // The number of total handles expected after this operation. This can be
    // inferred by sum of requested handles requests less handles destroyed
    // handles, but having it explcitly in the struct makes sure the test cases
    // are testing what they're expected to.
    size_t expected_total_handles;

    // If `num_handles` is set, this represents whether each request caused us
    // to hit the limit for the number of processes.
    std::vector<bool> hit_limit_after_requesting_handles = {};
  };

  const Operation kOperationList[] = {
      {Operation::Op::kRequestHandles,
       /*num_handles=*/GetMaxProcesses(),
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false, false, false}},

      // Check destroying intermediate, last, and first handle when there are no
      // queued requests. Keep exactly GetMaxProcesses() requests, to ensure
      // there are in fact first, last, and intermediate requests (as long as
      // GetMaxProcesses() is at least 3).
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/1u, /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/0u, /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() - 1,
       /*expected_total_handles=*/GetMaxProcesses() - 1},
      {Operation::Op::kRequestHandles,
       /*num_handles=*/1,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/
       GetMaxProcesses(), /*hit_limit_after_requesting_handles=*/
       {false}},

      // Queue 3 more requests, but delete the last and first of them, to test
      // deleting queued requests.
      {Operation::Op::kRequestHandles,
       /*num_handles=*/3,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/GetMaxProcesses() +
           3, /*hit_limit_after_requesting_handles=*/
       {true, true, true}},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses(),
       /*expected_total_handles=*/GetMaxProcesses() + 2},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() + 1,
       /*expected_total_handles=*/GetMaxProcesses() + 1},

      // Request 4 more processes.
      {Operation::Op::kRequestHandles,
       /*num_handles=*/4,
       /*index=*/std::nullopt,
       /*expected_total_handles=*/GetMaxProcesses() +
           5, /*hit_limit_after_requesting_handles=*/
       {true, true, true, true}},

      // Destroy the first handle and the first pending in the queue immediately
      // afterwards. The next pending request should get a process.
      {Operation::Op::kDestroyHandleAndNextInQueue,
       /*num_handles=*/std::nullopt, /*index=*/0u,
       /*expected_total_handles=*/GetMaxProcesses() + 3},

      // Destroy three more requests that have been asssigned processes, being
      // sure to destroy the first, last, and some request request with nether,
      // amongst requests with assigned processes.
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/GetMaxProcesses() - 1,
       /*expected_total_handles=*/GetMaxProcesses() + 2},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/0u, /*expected_total_handles=*/GetMaxProcesses() + 1},
      {Operation::Op::kDestroyHandle, /*num_handles=*/std::nullopt,
       /*index=*/1u, /*expected_total_handles=*/GetMaxProcesses()},
  };

  struct ProcessHandleData {
    std::unique_ptr<AuctionProcessManager::ProcessHandle> process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  };

  std::vector<ProcessHandleData> data;

  // Used to create distinct origins for each handle
  int num_origins = 0;
  for (const auto& operation : kOperationList) {
    switch (operation.op) {
      case Operation::Op::kRequestHandles:
        for (size_t i = 0; i < *operation.num_handles; ++i) {
          size_t original_size = data.size();
          data.emplace_back(ProcessHandleData());
          url::Origin distinct_origin = url::Origin::Create(
              GURL(base::StringPrintf("https://%i.test", ++num_origins)));
          base::HistogramTester histogram_tester;
          ASSERT_EQ(original_size < GetMaxProcesses(),
                    auction_process_manager_.RequestWorkletService(
                        GetParam(), distinct_origin, site_instance_,
                        data.back().process_handle.get(),
                        data.back().run_loop->QuitClosure()));
          RequestWorkletServiceOutcome expected_result =
              operation.hit_limit_after_requesting_handles[i]
                  ? RequestWorkletServiceOutcome::kHitProcessLimit
                  : RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess;
          histogram_tester.ExpectUniqueSample(
              base::StrCat(
                  {"Ads.InterestGroup.Auction.",
                   GetParam() == AuctionProcessManager::WorkletType::kSeller
                       ? "Seller."
                       : "Buyer.",
                   "RequestWorkletServiceOutcome"}),
              expected_result,
              /*expected_bucket_count=*/1);
        }
        break;

      case Operation::Op::kDestroyHandle: {
        size_t original_size = data.size();

        ASSERT_GT(data.size(), *operation.index);
        data.erase(data.begin() + *operation.index);
        // If destroying one of the first GetMaxProcesses() handles, and
        // there were more than GetMaxProcesses() handles before, the
        // first of the handles waiting on a process should get a process.
        if (*operation.index < GetMaxProcesses() &&
            original_size > GetMaxProcesses()) {
          data[GetMaxProcesses() - 1].run_loop->Run();
          EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        }
        break;
      }

      case Operation::Op::kDestroyHandleAndNextInQueue: {
        ASSERT_GT(data.size(), *operation.index);
        ASSERT_GT(data.size(), GetMaxProcesses() + 1);

        data.erase(data.begin() + *operation.index);
        data.erase(data.begin() + GetMaxProcesses());
        data[GetMaxProcesses() - 1].run_loop->Run();
        EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        break;
      }
    }

    EXPECT_EQ(operation.expected_total_handles, data.size());

    // The first GetMaxProcesses() ProcessHandles should all have
    // assigned processes, which should all be distinct.
    for (size_t i = 0; i < data.size() && i < GetMaxProcesses(); ++i) {
      EXPECT_TRUE(data[i].process_handle->GetService());
      for (size_t j = 0; j < i; ++j) {
        EXPECT_NE(data[i].process_handle->GetService(),
                  data[j].process_handle->GetService());
      }
    }

    // Make sure all pending tasks have been run.
    base::RunLoop().RunUntilIdle();

    // All other requests should not have been assigned processes yet.
    for (size_t i = GetMaxProcesses(); i < data.size(); ++i) {
      EXPECT_FALSE(data[i].run_loop->AnyQuitCalled());
      EXPECT_FALSE(data[i].process_handle->GetService());
    }
  }
}

// Check the process sharing logic - specifically, that requests share processes
// when origins match, and that handles that share a process only count once
// towrads the process limit the process limit.
TEST_P(AuctionProcessManagerTest, ProcessSharing) {
  // This test assumes GetMaxProcesses() is greater than 1.
  DCHECK_GT(GetMaxProcesses(), 1u);

  // Make 2*GetMaxProcesses() requests for each of GetMaxProcesses() different
  // origins. All requests should succeed immediately.
  std::vector<std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>>
      processes(GetMaxProcesses());
  for (size_t origin_index = 0; origin_index < GetMaxProcesses();
       ++origin_index) {
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://%zu.test", origin_index)));
    base::HistogramTester histogram_tester;
    for (size_t i = 0; i < 2 * GetMaxProcesses(); ++i) {
      processes[origin_index].emplace_back(GetServiceExpectSuccess(origin));
      // All requests for the same origin share a process.
      EXPECT_EQ(processes[origin_index].back()->GetService(),
                processes[origin_index].front()->GetService());
      EXPECT_EQ(origin_index + 1, auction_process_manager_.NumReceivers());
    }
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Ads.InterestGroup.Auction.",
                      GetParam() == AuctionProcessManager::WorkletType::kSeller
                          ? "Seller."
                          : "Buyer.",
                      "RequestWorkletServiceOutcome"}),
        RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 1);
    histogram_tester.ExpectBucketCount(
        base::StrCat({"Ads.InterestGroup.Auction.",
                      GetParam() == AuctionProcessManager::WorkletType::kSeller
                          ? "Seller."
                          : "Buyer.",
                      "RequestWorkletServiceOutcome"}),
        RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess,
        2 * GetMaxProcesses() - 1);

    // Each origin should have a different process.
    for (size_t origin_index2 = 0; origin_index2 < origin_index;
         ++origin_index2) {
      EXPECT_NE(processes[origin_index].front()->GetService(),
                processes[origin_index2].front()->GetService());
    }
  }

  // Make two process requests for kOriginA and one one for kOriginB, which
  // should all be blocked due to the process limit being reached.

  base::RunLoop run_loop_delayed_a1;
  auto process_delayed_a1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, site_instance_, process_delayed_a1.get(),
      run_loop_delayed_a1.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_a2;
  auto process_delayed_a2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, site_instance_, process_delayed_a2.get(),
      run_loop_delayed_a2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_b;
  auto process_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginB, site_instance_, process_delayed_b.get(),
      run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  // Release processes for first origin one at a time, until only one is left.
  // The pending requests for kOriginA and kOriginB should remain stalled.
  while (processes[0].size() > 1u) {
    processes[0].pop_front();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a1->GetService());
    EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a2->GetService());
    EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_b->GetService());
    EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());
  }

  // Remove the final process for the first origin. It should queue a callback
  // to resume the kOriginA requests (prioritized alphabetically), but nothing
  // should happen until the callbacks are invoked.
  processes[0].pop_front();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // The two kOriginA callbacks should be invoked when the message loop next
  // spins. The two kOriginA requests should now have been assigned the same
  // service, while the kOriginB request is still pending.
  run_loop_delayed_a1.Run();
  run_loop_delayed_a2.Run();
  EXPECT_TRUE(process_delayed_a1->GetService());
  EXPECT_TRUE(process_delayed_a2->GetService());
  EXPECT_EQ(process_delayed_a1->GetService(), process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  // Freeing one of the two kOriginA processes should have no effect.
  process_delayed_a2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // Freeing the other one should queue a task to give the kOriginB requests a
  // process.
  process_delayed_a1.reset();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  run_loop_delayed_b.Run();
  EXPECT_TRUE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());
}

TEST_P(AuctionProcessManagerTest, DestroyHandlesWithPendingRequests) {
  // Make GetMaxProcesses() requests for worklets with different origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> processes;
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    processes.emplace_back(GetServiceExpectSuccess(origin));
  }

  // Make a pending request.
  auto pending_process1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, site_instance_, pending_process1.get(),
      NeverInvokedClosure()));
  EXPECT_EQ(1u, GetPendingRequestsOfParamType());

  // Destroy the pending request. Its callback should not be invoked.
  pending_process1.reset();
  EXPECT_EQ(0u, GetPendingRequestsOfParamType());
  base::RunLoop().RunUntilIdle();

  // Make two more pending process requests.
  auto pending_process2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, site_instance_, pending_process2.get(),
      NeverInvokedClosure()));
  auto pending_process3 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  base::RunLoop pending_process3_run_loop;
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginB, site_instance_, pending_process3.get(),
      pending_process3_run_loop.QuitClosure()));
  EXPECT_EQ(2u, GetPendingRequestsOfParamType());

  // Delete a process. This should result in a posted task to give
  // `pending_process2` a process.
  processes.pop_front();
  EXPECT_EQ(1u, GetPendingRequestsOfParamType());

  // Destroy `pending_process2` before it gets passed a process.
  pending_process2.reset();

  // `pending_process3` should get a process instead.
  pending_process3_run_loop.Run();
  EXPECT_TRUE(pending_process3->GetService());
  EXPECT_EQ(0u, auction_process_manager_.GetPendingSellerRequestsForTesting());
}

// Check that process crash is handled properly, by creating a new process.
TEST_P(AuctionProcessManagerTest, ProcessCrash) {
  auto process = GetServiceExpectSuccess(kOriginA);
  auction_worklet::mojom::AuctionWorkletService* service =
      process->GetService();
  EXPECT_TRUE(service);
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Close pipes. No new pipe should be created.
  auction_process_manager_.ClosePipes();
  EXPECT_EQ(0u, auction_process_manager_.NumReceivers());

  // Requesting a new process will create a new pipe.
  auto process2 = GetServiceExpectSuccess(kOriginA);
  auction_worklet::mojom::AuctionWorkletService* service2 =
      process2->GetService();
  EXPECT_TRUE(service2);
  EXPECT_NE(service, service2);
  EXPECT_NE(process, process2);
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

TEST_P(AuctionProcessManagerTest, DisconnectBeforeDelete) {
  // Exercise the codepath where the mojo pipe to a service is broken when
  // a handle to its process is still alive, to make sure this is handled
  // correctly (rather than hitting a DCHECK on incorrect refcounting).
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceExpectSuccess(kOriginA);
  auction_process_manager_.ClosePipes();
  task_environment_.RunUntilIdle();
  handle_a1.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(AuctionProcessManagerTest, ProcessDeleteBeforeHandle) {
  // Exercise the codepath where a RenderProcessHostDestroyed is received, to
  // make sure it doesn't crash.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceExpectSuccess(kOriginA);
  for (std::unique_ptr<MockRenderProcessHost>& proc :
       *rph_factory_.GetProcesses()) {
    proc.reset();
  }
  task_environment_.RunUntilIdle();
  handle_a1.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(AuctionProcessManagerTest, PidLookup) {
  auto handle = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);

  base::ProcessId expected_pid = base::Process::Current().Pid();

  // Request PID twice. Should happen asynchronously, but only use one RPC.
  base::RunLoop run_loop0, run_loop1;
  bool got_pid0 = false, got_pid1 = false;
  std::optional<base::ProcessId> pid0 =
      handle->GetPid(base::BindLambdaForTesting(
          [&run_loop0, &got_pid0, expected_pid](base::ProcessId pid) {
            EXPECT_EQ(expected_pid, pid);
            got_pid0 = true;
            run_loop0.Quit();
          }));
  EXPECT_FALSE(pid0.has_value());
  std::optional<base::ProcessId> pid1 =
      handle->GetPid(base::BindLambdaForTesting(
          [&run_loop1, &got_pid1, expected_pid](base::ProcessId pid) {
            EXPECT_EQ(expected_pid, pid);
            got_pid1 = true;
            run_loop1.Quit();
          }));
  EXPECT_FALSE(pid1.has_value());

  for (std::unique_ptr<MockRenderProcessHost>& proc :
       *rph_factory_.GetProcesses()) {
    proc->SimulateReady();
  }

  run_loop0.Run();
  EXPECT_TRUE(got_pid0);
  run_loop1.Run();
  EXPECT_TRUE(got_pid1);

  // Next attempt should be synchronous.
  std::optional<base::ProcessId> pid2 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid2 case";
      }));
  ASSERT_TRUE(pid2.has_value());
  EXPECT_EQ(expected_pid, pid2.value());
}

TEST_F(AuctionProcessManagerTest, PidLookupAlreadyRunning) {
  // "Launch" the appropriate process before we even ask for it, and mark its
  // launch as completed. |frame_site_instance| will help keep it alive.
  scoped_refptr<SiteInstance> frame_site_instance =
      site_instance_->GetRelatedSiteInstance(kOriginA.GetURL());
  frame_site_instance->GetProcess()->Init();
  for (std::unique_ptr<MockRenderProcessHost>& proc :
       *rph_factory_.GetProcesses()) {
    proc->SimulateReady();
  }

  auto handle = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);

  base::ProcessId expected_pid = base::Process::Current().Pid();

  // Request PID twice. Should happen asynchronously, but only use one RPC.
  std::optional<base::ProcessId> pid0 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid0 case";
      }));
  ASSERT_TRUE(pid0.has_value());
  EXPECT_EQ(expected_pid, pid0.value());
  std::optional<base::ProcessId> pid1 =
      handle->GetPid(base::BindOnce([](base::ProcessId pid) {
        ADD_FAILURE() << "Should not get to callback in pid1 case";
      }));
  ASSERT_TRUE(pid1.has_value());
  EXPECT_EQ(expected_pid, pid1.value());
}

class PartialSiteIsolationContentBrowserClient
    : public TestContentBrowserClient {
 public:
  bool ShouldEnableStrictSiteIsolation() override { return false; }

  bool ShouldDisableSiteIsolation(
      SiteIsolationMode site_isolation_mode) override {
    switch (site_isolation_mode) {
      case SiteIsolationMode::kStrictSiteIsolation:
        return true;
      case SiteIsolationMode::kPartialSiteIsolation:
        return false;
    }
  }
};

// A base class for AuctionProcessManager tests that sets up the basic test
// environment. Since this class creates SiteInstances and (implicitly)
// BrowsingInstances, it's important that it knows whether to use
// kOriginKeyedProcessesByDefault at the time it's constructed.
class InRendererAuctionProcessManagerTestBase : public ::testing::Test {
 public:
  explicit InRendererAuctionProcessManagerTestBase(
      bool disable_origin_keyed_processes_by_default) {
    // Note: if we're going to disable kOriginKeyedProcessesByDefault, it's
    // important to do it here before we create any SiteInstances, since that
    // will create BrowsingInstances, and each BrowsingInstance will create
    // a default isolation state based on kOriginKeyedProcessesByDefault.
    if (disable_origin_keyed_processes_by_default) {
      feature_list_.InitAndDisableFeature(
          features::kOriginKeyedProcessesByDefault);
    }

    SiteInstance::StartIsolatingSite(
        &test_browser_context_, kIsolatedOrigin.GetURL(),
        ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
    // Created these after StartIsolatingSite so they are affected by it.
    site_instance1_ = SiteInstance::Create(&test_browser_context_);
    site_instance2_ = SiteInstance::Create(&test_browser_context_);
  }

  void SetUp() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &rph_factory_);
    SiteIsolationPolicy::DisableFlagCachingForTesting();
  }

  void TearDown() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
  }

  std::unique_ptr<AuctionProcessManager::ProcessHandle>
  GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType worklet_type,
                                scoped_refptr<SiteInstance> site_instance,
                                const url::Origin& origin) {
    auto process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    EXPECT_TRUE(auction_process_manager_.RequestWorkletService(
        worklet_type, origin, site_instance, process_handle.get(),
        NeverInvokedClosure()));
    EXPECT_TRUE(process_handle->GetService());
    return process_handle;
  }

  base::OnceClosure NeverInvokedClosure() {
    return base::BindOnce(
        []() { ADD_FAILURE() << "This should not be called"; });
  }

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext test_browser_context_;
  MockRenderProcessHostFactory rph_factory_;

  // `site_instance1_` and `site_instance2_` are in different browsing
  // instances.
  scoped_refptr<SiteInstance> site_instance1_, site_instance2_;
  InRendererAuctionProcessManager auction_process_manager_;

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
  const url::Origin kIsolatedOrigin =
      url::Origin::Create(GURL("https://bank.test"));

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test class for AuctionProcessManager tests that require desktop-like
// behavior, i.e. site-per-process is enabled, and
// kOriginKeyedProcessesByDefault and process sharing for non-default
// SiteInstances is allowed.
class InRendererAuctionProcessManagerTest
    : public InRendererAuctionProcessManagerTestBase {
 public:
  InRendererAuctionProcessManagerTest()
      : InRendererAuctionProcessManagerTestBase(
            /*disable_origin_keyed_processes_by_default=*/false) {}
  void SetUp() override {
    InRendererAuctionProcessManagerTestBase::SetUp();
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kSitePerProcess);
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

// A test class for AuctionProcessManager tests that require Android-like
// behavior, i.e. site-per-process is disabled, kOriginKeyedProcessesByDefault
// is disabled, and process sharing is set for default SiteInstances only.
class InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault
    : public InRendererAuctionProcessManagerTestBase {
 public:
  InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault()
      : InRendererAuctionProcessManagerTestBase(
            /*disable_origin_keyed_processes_by_default=*/true) {
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kProcessSharingWithDefaultSiteInstances},
        /*disabled_features=*/{
            features::kProcessSharingWithStrictSiteInstances});
  }

  void SetUp() override {
    InRendererAuctionProcessManagerTestBase::SetUp();
    original_browser_client_ =
        content::SetBrowserClientForTesting(&browser_client_);
    scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
        switches::kSitePerProcess);
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(original_browser_client_);
    InRendererAuctionProcessManagerTestBase::TearDown();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list;
  PartialSiteIsolationContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_browser_client_;
};

TEST_F(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       AndroidLike) {
  base::HistogramTester histogram_tester;

  // Launch some services in different origins and browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginA);
  int id_a1 = handle_a1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kOriginA);
  int id_a2 = handle_a2->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginB);
  int id_b1 = handle_b1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kOriginB);
  int id_b2 = handle_b2->GetRenderProcessHostForTesting()->GetID();

  // Non-site-isolation requiring origins can share processes, but not across
  // different browsing instances.
  EXPECT_NE(id_a1, id_a2);
  EXPECT_EQ(id_a1, id_b1);
  EXPECT_NE(id_a1, id_b2);
  EXPECT_NE(id_a2, id_b1);
  EXPECT_EQ(id_a2, id_b2);
  EXPECT_NE(id_b1, id_b2);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kUsedSharedProcess, 4);

  // Site-isolation requiring origins are distinct from non-isolated ones, but
  // can share across browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kIsolatedOrigin);
  int id_i1 = handle_i1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kIsolatedOrigin);
  int id_i2 = handle_i2->GetRenderProcessHostForTesting()->GetID();

  EXPECT_EQ(id_i1, id_i2);
  EXPECT_NE(id_i1, id_a1);
  EXPECT_NE(id_i1, id_a2);
  EXPECT_NE(id_i1, id_b1);
  EXPECT_NE(id_i1, id_b2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 1);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 1);
}

TEST_F(InRendererAuctionProcessManagerTest, DesktopLike) {
  base::HistogramTester histogram_tester;

  // Launch some services in different origins and browsing instances.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginA);
  int id_a1 = handle_a1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kOriginA);
  int id_a2 = handle_a2->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginB);
  int id_b1 = handle_b1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_b2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kOriginB);
  int id_b2 = handle_b2->GetRenderProcessHostForTesting()->GetID();

  // Since we are site-per-process, things should be grouped by origin.
  EXPECT_EQ(id_a1, id_a2);
  EXPECT_NE(id_a1, id_b1);
  EXPECT_NE(id_a1, id_b2);
  EXPECT_NE(id_a2, id_b1);
  EXPECT_NE(id_a2, id_b2);
  EXPECT_EQ(id_b1, id_b2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 2);

  // Stuff that's also isolated by explicit requests gets the same treatment.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kIsolatedOrigin);
  int id_i1 = handle_i1->GetRenderProcessHostForTesting()->GetID();

  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_i2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance2_, kIsolatedOrigin);
  int id_i2 = handle_i2->GetRenderProcessHostForTesting()->GetID();

  EXPECT_EQ(id_i1, id_i2);
  EXPECT_NE(id_i1, id_a1);
  EXPECT_NE(id_i1, id_a2);
  EXPECT_NE(id_i1, id_b1);
  EXPECT_NE(id_i1, id_b2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess, 3);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.Auction.Seller.RequestWorkletServiceOutcome",
      RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess, 3);
}

TEST_F(InRendererAuctionProcessManagerTest_NoOriginKeyedProcessesByDefault,
       PolicyChange) {
  // Launch site in default instance.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a1 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginA);
  EXPECT_FALSE(
      handle_a1->site_instance_for_testing()->RequiresDedicatedProcess());
  RenderProcessHost* shared_process =
      handle_a1->GetRenderProcessHostForTesting();

  // Change policy so that A can no longer use shared instances.
  SiteInstance::StartIsolatingSite(
      &test_browser_context_, kOriginA.GetURL(),
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  site_instance1_ = SiteInstance::Create(&test_browser_context_);

  // Launch another A-origin worklet, this should get a different process.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a2 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginA);
  EXPECT_TRUE(
      handle_a2->site_instance_for_testing()->RequiresDedicatedProcess());
  EXPECT_NE(handle_a2->GetRenderProcessHostForTesting(), shared_process);

  // Destroy shared process and try to get another A one --- should reuse the
  // same non-shared process.
  handle_a1.reset();
  std::unique_ptr<AuctionProcessManager::ProcessHandle> handle_a3 =
      GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType::kSeller,
                                    site_instance1_, kOriginA);
  EXPECT_TRUE(
      handle_a3->site_instance_for_testing()->RequiresDedicatedProcess());
  EXPECT_EQ(handle_a2->GetRenderProcessHostForTesting(),
            handle_a3->GetRenderProcessHostForTesting());
  // Checking GetRenderProcessHostForTesting isn't enough since SiteInstance
  // can share it, too.
  EXPECT_EQ(handle_a2->worklet_process_for_testing(),
            handle_a3->worklet_process_for_testing());
}

}  // namespace
}  // namespace content
