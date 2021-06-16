// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class TestAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  TestAuctionProcessManager() = default;

  TestAuctionProcessManager(const TestAuctionProcessManager&) = delete;
  const TestAuctionProcessManager& operator=(const TestAuctionProcessManager&) =
      delete;

  ~TestAuctionProcessManager() override = default;

  void LoadBidderWorkletAndGenerateBid(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      auction_worklet::mojom::BiddingInterestGroupPtr bidding_interest_group,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const url::Origin& browser_signal_top_window_origin,
      const url::Origin& browser_signal_seller_origin,
      base::Time auction_start_time,
      LoadBidderWorkletAndGenerateBidCallback callback) override {
    NOTREACHED();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const GURL& script_source_url,
      LoadSellerWorkletCallback callback) override {
    NOTREACHED();
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
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    receiver_set_.Add(this, std::move(auction_worklet_service_receiver));
  }

  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class AuctionProcessManagerTest : public testing::Test {
 protected:
  // Request a worklet service and expect the request to complete synchronously.
  // There's no async version, since async calls are only triggered by deleting
  // another handle.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType worklet_type,
      const url::Origin& origin) {
    auto process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    EXPECT_TRUE(auction_process_manager_.RequestWorkletService(
        worklet_type, origin, process_handle.get(), NeverInvokedClosure()));
    EXPECT_TRUE(process_handle->GetService());
    return process_handle;
  }

  base::OnceClosure NeverInvokedClosure() {
    return base::BindOnce(
        []() { ADD_FAILURE() << "This should not be called"; });
  }

  base::test::TaskEnvironment task_environment_;
  TestAuctionProcessManager auction_process_manager_;

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
};

TEST_F(AuctionProcessManagerTest, Basic) {
  auto seller = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  EXPECT_TRUE(seller->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

// Make sure requests for different origins don't share processes, nor do
// sellers and bidders.
TEST_F(AuctionProcessManagerTest, MultipleRequestsForDifferentProcesses) {
  auto seller_a = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  auto seller_b = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginB);
  auto buyer_a = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginA);
  auto buyer_b = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginB);

  EXPECT_EQ(4u, auction_process_manager_.NumReceivers());
  EXPECT_NE(seller_a->GetService(), seller_b->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_b->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_b->GetService());
  EXPECT_NE(buyer_a->GetService(), buyer_b->GetService());
}

TEST_F(AuctionProcessManagerTest, MultipleRequestsForSameProcess) {
  auto seller_a1 = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  EXPECT_TRUE(seller_a1->GetService());
  auto seller_a2 = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  EXPECT_EQ(seller_a1->GetService(), seller_a2->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  std::unique_ptr<AuctionProcessManager::ProcessHandle> buyer_a1 =
      GetServiceExpectSuccess(AuctionProcessManager::WorkletType::kBidder,
                              kOriginA);
  EXPECT_TRUE(buyer_a1->GetService());
  EXPECT_NE(seller_a1->GetService(), buyer_a1->GetService());

  std::unique_ptr<AuctionProcessManager::ProcessHandle> buyer_a2 =
      GetServiceExpectSuccess(AuctionProcessManager::WorkletType::kBidder,
                              kOriginA);
  std::unique_ptr<AuctionProcessManager::ProcessHandle> buyer_a3 =
      GetServiceExpectSuccess(AuctionProcessManager::WorkletType::kBidder,
                              kOriginA);
  EXPECT_EQ(2u, auction_process_manager_.NumReceivers());
  EXPECT_EQ(buyer_a1->GetService(), buyer_a2->GetService());
  EXPECT_EQ(buyer_a1->GetService(), buyer_a3->GetService());
}

TEST_F(AuctionProcessManagerTest, SellerLimit) {
  // This test assumes kMaxActiveSellerWorklets is greater than 1.
  DCHECK_GT(AuctionProcessManager::kMaxActiveSellerWorklets, 1u);

  // Make kMaxActiveSellerWorklets seller worklet requests for kOriginA.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers_a;
  for (size_t i = 0; i < AuctionProcessManager::kMaxActiveSellerWorklets; ++i) {
    sellers_a.emplace_back(GetServiceExpectSuccess(
        AuctionProcessManager::WorkletType::kSeller, kOriginA));
    EXPECT_EQ(sellers_a.back()->GetService(), sellers_a.front()->GetService());
    EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
  }

  // Make seller requests for kOriginA and kOriginB, which should both be
  // blocked due to the seller worklet limit being reached.

  base::RunLoop run_loop_delayed_a;
  auto seller_delayed_a =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kSeller, kOriginA,
      seller_delayed_a.get(), run_loop_delayed_a.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
  EXPECT_FALSE(run_loop_delayed_a.AnyQuitCalled());
  EXPECT_FALSE(seller_delayed_a->GetService());

  base::RunLoop run_loop_delayed_b;
  auto seller_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kSeller, kOriginB,
      seller_delayed_b.get(), run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(seller_delayed_b->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Free up a seller slot. `seller_delayed_a` should get the same process as
  // the other requests asynchronously. That request gets the socket first
  // because requests are currently handled alphabetically by by origin. If
  // multiple requests are for same origin, order is random.
  sellers_a.pop_back();
  EXPECT_FALSE(run_loop_delayed_a.AnyQuitCalled());
  EXPECT_FALSE(seller_delayed_a->GetService());

  run_loop_delayed_a.Run();
  EXPECT_TRUE(seller_delayed_a->GetService());
  EXPECT_EQ(seller_delayed_a->GetService(), sellers_a.front()->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // `seller_delayed_b` is still blocked on the two active sellers.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(seller_delayed_b->GetService());

  // Free up another seller slot. `seller_delayed_b` should get a different
  // process asynchronously.
  sellers_a.pop_front();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(seller_delayed_b->GetService());

  run_loop_delayed_b.Run();
  EXPECT_TRUE(seller_delayed_b->GetService());
  EXPECT_NE(seller_delayed_b->GetService(), seller_delayed_a->GetService());
  EXPECT_EQ(2u, auction_process_manager_.NumReceivers());
}

TEST_F(AuctionProcessManagerTest, BidderLimit) {
  // This test assumes kMaxBidderProcesses is greater than 1.
  DCHECK_GT(AuctionProcessManager::kMaxBidderProcesses, 1u);

  // Make 2*kMaxBidderProcesses bidder worklet requests for each of
  // kMaxBidderProcesses different origins. All requests should succeed
  // immediately.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      bidders[AuctionProcessManager::kMaxBidderProcesses];
  for (size_t origin_index = 0;
       origin_index < AuctionProcessManager::kMaxBidderProcesses;
       ++origin_index) {
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://%zu.test", origin_index)));
    for (size_t i = 0; i < 2 * AuctionProcessManager::kMaxBidderProcesses;
         ++i) {
      bidders[origin_index].emplace_back(GetServiceExpectSuccess(
          AuctionProcessManager::WorkletType::kBidder, origin));
      // All requests for the same origin share a process.
      EXPECT_EQ(bidders[origin_index].back()->GetService(),
                bidders[origin_index].front()->GetService());
      EXPECT_EQ(origin_index + 1, auction_process_manager_.NumReceivers());
    }

    // Each origin should have a different process.
    for (size_t origin_index2 = 0; origin_index2 < origin_index;
         ++origin_index2) {
      EXPECT_NE(bidders[origin_index].front()->GetService(),
                bidders[origin_index2].front()->GetService());
    }
  }

  // Make two bidder requests for kOriginA and one one for kOriginB, which
  // should all be blocked due to the bidder process limit being reached.

  base::RunLoop run_loop_delayed_a1;
  auto bidder_delayed_a1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginA,
      bidder_delayed_a1.get(), run_loop_delayed_a1.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_a1->GetService());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_a2;
  auto bidder_delayed_a2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginA,
      bidder_delayed_a2.get(), run_loop_delayed_a2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_a2->GetService());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_b;
  auto bidder_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginB,
      bidder_delayed_b.get(), run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.NumReceivers());

  // Release bidders for first origin one at a time, until only one is left. The
  // pending requests for kOriginA and kOriginB should remain stalled.
  while (bidders[0].size() > 1u) {
    bidders[0].pop_front();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
    EXPECT_FALSE(bidder_delayed_a1->GetService());
    EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
    EXPECT_FALSE(bidder_delayed_a2->GetService());
    EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
    EXPECT_FALSE(bidder_delayed_b->GetService());
    EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
              auction_process_manager_.NumReceivers());
  }

  // Remove the final bidder for the first origin. It should queue a callback to
  // resume the kOriginA requests (prioritized alphabetically), but nothing
  // should happen until the callbacks are invoked.
  bidders[0].pop_front();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_a1->GetService());
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());

  // The two kOriginA callbacks should be invoked when the message loop next
  // spins. The two kOriginA requests should now have been assigned the same
  // service, while the kOriginB request is still pending.
  run_loop_delayed_a1.Run();
  run_loop_delayed_a2.Run();
  EXPECT_TRUE(bidder_delayed_a1->GetService());
  EXPECT_TRUE(bidder_delayed_a2->GetService());
  EXPECT_EQ(bidder_delayed_a1->GetService(), bidder_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.NumReceivers());

  // Freeing one of the two kOriginA bidders should have no effect.
  bidder_delayed_a2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());

  // Freeing the other one should queue a task to give the kOriginB requests a
  // process.
  bidder_delayed_a1.reset();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());

  run_loop_delayed_b.Run();
  EXPECT_TRUE(bidder_delayed_b->GetService());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.NumReceivers());
}

TEST_F(AuctionProcessManagerTest, DestroyHandlesWithPendingSellerRequests) {
  // Make kMaxActiveSellerWorklets seller worklet requests.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < AuctionProcessManager::kMaxActiveSellerWorklets; ++i) {
    sellers.emplace_back(GetServiceExpectSuccess(
        AuctionProcessManager::WorkletType::kSeller, kOriginA));
    EXPECT_EQ(sellers.back()->GetService(), sellers.front()->GetService());
    EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
  }

  // Make a pending seller request.
  auto pending_seller1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kSeller, kOriginA,
      pending_seller1.get(), NeverInvokedClosure()));
  EXPECT_EQ(1u, auction_process_manager_.GetPendingSellerRequestsForTesting());

  // Destroy the pending request. Its callback should not be invoked.
  pending_seller1.reset();
  EXPECT_EQ(0u, auction_process_manager_.GetPendingSellerRequestsForTesting());
  base::RunLoop().RunUntilIdle();

  // Make two more pending seller requests.
  auto pending_seller2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kSeller, kOriginA,
      pending_seller2.get(), NeverInvokedClosure()));
  auto pending_seller3 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  base::RunLoop pending_seller3_run_loop;
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kSeller, kOriginB,
      pending_seller3.get(), pending_seller3_run_loop.QuitClosure()));
  EXPECT_EQ(2u, auction_process_manager_.GetPendingSellerRequestsForTesting());

  // Delete a seller. This should result in a posted task to give
  // `pending_seller2` a process.
  sellers.pop_front();
  EXPECT_EQ(1u, auction_process_manager_.GetPendingSellerRequestsForTesting());

  // Destroy `pending_seller2` before it gets passed a process.
  pending_seller2.reset();

  // `pending_seller3` should get a process instead.
  pending_seller3_run_loop.Run();
  EXPECT_TRUE(pending_seller3->GetService());
  EXPECT_EQ(0u, auction_process_manager_.GetPendingSellerRequestsForTesting());
}

TEST_F(AuctionProcessManagerTest, DestroyHandlesWithPendingBidderRequests) {
  // Make kMaxBidderProcesses requests for bidder worklets with different
  // origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    bidders.emplace_back(GetServiceExpectSuccess(
        AuctionProcessManager::WorkletType::kBidder, origin));
  }

  // Make a pending bidder request.
  auto pending_bidder1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginA,
      pending_bidder1.get(), NeverInvokedClosure()));
  EXPECT_EQ(1u, auction_process_manager_.GetPendingBidderRequestsForTesting());

  // Destroy the pending request. Its callback should not be invoked.
  pending_bidder1.reset();
  EXPECT_EQ(0u, auction_process_manager_.GetPendingBidderRequestsForTesting());
  base::RunLoop().RunUntilIdle();

  // Make two more pending bidder requests.
  auto pending_bidder2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginA,
      pending_bidder2.get(), NeverInvokedClosure()));
  auto pending_bidder3 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  base::RunLoop pending_bidder3_run_loop;
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginB,
      pending_bidder3.get(), pending_bidder3_run_loop.QuitClosure()));
  EXPECT_EQ(2u, auction_process_manager_.GetPendingBidderRequestsForTesting());

  // Delete a bidder. This should result in a posted task to give
  // `pending_bidder2` a process.
  bidders.pop_front();
  EXPECT_EQ(1u, auction_process_manager_.GetPendingBidderRequestsForTesting());

  // Destroy `pending_bidder2` before it gets passed a process.
  pending_bidder2.reset();

  // `pending_bidder3` should get a process instead.
  pending_bidder3_run_loop.Run();
  EXPECT_TRUE(pending_bidder3->GetService());
  EXPECT_EQ(0u, auction_process_manager_.GetPendingSellerRequestsForTesting());
}

// Check that process is automatically re-created on crash. Likely not the most
// important behavior in the world, as auctions aren't restarted on crash, and
// worklet handles should be freed on process crash fairly promptly, but best to
// be safe.
TEST_F(AuctionProcessManagerTest, ProcessCrash) {
  auto seller = GetServiceExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  EXPECT_TRUE(seller->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Close pipes. No new pipe should be created.
  auction_process_manager_.ClosePipes();
  EXPECT_EQ(0u, auction_process_manager_.NumReceivers());

  // Request the seller worklet's service again. A new pipe will automatically
  // be created.
  EXPECT_TRUE(seller->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

}  // namespace
}  // namespace content
