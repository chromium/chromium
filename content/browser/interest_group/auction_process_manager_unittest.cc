// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

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

// Alias constants to improve readability.
const size_t kMaxActiveSellerWorklets =
    AuctionProcessManager::kMaxActiveSellerWorklets;
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
          bidder_worklet,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      auction_worklet::mojom::BiddingInterestGroupPtr bidding_interest_group)
      override {
    NOTREACHED();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet,
      bool should_pause_on_start,
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

// Check that the seller process limit is respected, that sellers shared
// processes when their origins match, and that sellers receive processes in
// FIFO order.
TEST_F(AuctionProcessManagerTest, SellerLimitExceeded) {
  // The list of operations below assumes 3 sellers are allowed to run at once -
  // the interesting cases move slightly if this changes, but should be easy to
  // adapt the list to different values.
  CHECK_EQ(kMaxActiveSellerWorklets, 3u);

  struct Operation {
    enum class Op {
      // Request a handle. If there are less than kMaxActiveSellerWorklets
      // handles already, expects a process to immediately assigned.
      kRequestHandle,

      // Destroy a handle with the given index. If the index is less than
      // kMaxActiveSellerWorklets, then expect a ProcessHandle to have its
      // callback invoked, if there are more than kMaxActiveSellerWorklets
      // already.
      kDestroyHandle,

      // Same as destroy handle, but additionally destroys the next handle that
      // would have been assigned the next available process slot, and makes
      // sure the handle after that one gets a process instead.
      kDestroyHandleAndNextInQueue,
    };

    Op op;

    // Used for kRequestHandle* operations.
    absl::optional<url::Origin> origin;

    // Used for kDestroyHandle and kDestroyHandleAndNextInQueue operations.
    absl::optional<size_t> index;

    // The number of total handles expected after this operation. This can be
    // inferred by sum of requested handles requests less handles destroyed
    // handles, but having it explicitly in the struct makes sure the test cases
    // are testing what they're expected to.
    size_t expected_total_handles;
  };

  const Operation kOperationList[] = {
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       1u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       2u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       3u /* expected_total_handles */},

      // Check destroying middle, last, and first handle when there are no
      // queued requests. Keep three live requests, to there remain first,
      // middle, and last ProcessHandles.
      {Operation::Op::kDestroyHandle, absl::nullopt, 1u /* index */,
       2u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       3u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 2u /* index */,
       2u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       3u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 0u /* index */,
       2u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       3u /* expected_total_handles */},

      // Queue 3 more requests, but delete the last and first of them, to test
      // deleting queued requests.
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       4u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       5u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       6u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 5u /* index */,
       5u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 3u /* index */,
       4u /* expected_total_handles */},

      // Make requests for different origins, to make sure processes aren't
      // shared, and check that FIFO ordering is respected across origins.
      {Operation::Op::kRequestHandle, kOriginB, absl::nullopt,
       5u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginB, absl::nullopt,
       6u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginA, absl::nullopt,
       7u /* expected_total_handles */},
      {Operation::Op::kRequestHandle, kOriginB, absl::nullopt,
       8u /* expected_total_handles */},

      // Destroy the first handle and the first kOriginB request in the queue
      // immediately afterwards. The second kOriginB request should get a
      // process.
      {Operation::Op::kDestroyHandleAndNextInQueue, absl::nullopt,
       0u /* index */, 6u /* expected_total_handles */},

      // Destroy three requests with assigned processes. Make sure to destroy
      // the first, last, and middle request with an assigned process.
      {Operation::Op::kDestroyHandle, absl::nullopt, 2u /* index */,
       5u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 0u /* index */,
       4u /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt, 1u /* index */,
       3u /* expected_total_handles */},
  };

  struct ProcessHandleData {
    explicit ProcessHandleData(const url::Origin& origin) : origin(origin) {}

    url::Origin origin;
    std::unique_ptr<AuctionProcessManager::ProcessHandle> process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  };

  std::vector<ProcessHandleData> data;

  for (const auto& operation : kOperationList) {
    size_t original_size = data.size();
    switch (operation.op) {
      case Operation::Op::kRequestHandle:
        data.emplace_back(ProcessHandleData(*operation.origin));
        ASSERT_EQ(original_size < kMaxActiveSellerWorklets,
                  auction_process_manager_.RequestWorkletService(
                      AuctionProcessManager::WorkletType::kSeller,
                      *operation.origin, data.back().process_handle.get(),
                      data.back().run_loop->QuitClosure()));
        break;

      case Operation::Op::kDestroyHandle:
        ASSERT_GT(data.size(), *operation.index);
        data.erase(data.begin() + *operation.index);
        // If destroying one of the first kMaxActiveSellerWorklets handles, and
        // there were more than kMaxActiveSellerWorklets handles before, the
        // first of the handles waiting on a process should get a process.
        if (*operation.index < kMaxActiveSellerWorklets &&
            original_size > kMaxActiveSellerWorklets) {
          data[kMaxActiveSellerWorklets - 1].run_loop->Run();
          EXPECT_TRUE(
              data[kMaxActiveSellerWorklets - 1].process_handle->GetService());
        }
        break;

      case Operation::Op::kDestroyHandleAndNextInQueue:
        ASSERT_GT(data.size(), *operation.index);
        ASSERT_GT(data.size(), kMaxActiveSellerWorklets + 1);

        data.erase(data.begin() + *operation.index);
        // Next socket in line shouldn't have a process assigned to it yet.
        EXPECT_FALSE(
            data[kMaxActiveSellerWorklets - 1].process_handle->GetService());

        // Delete next socket in line.
        data.erase(data.begin() + kMaxActiveSellerWorklets - 1);
        EXPECT_FALSE(
            data[kMaxActiveSellerWorklets - 1].process_handle->GetService());

        // New next socket in line also shouldn't have a process assigned to it
        // yet.
        data[kMaxActiveSellerWorklets - 1].run_loop->Run();

        // Wait for the next socket in line to get a socket.
        EXPECT_TRUE(
            data[kMaxActiveSellerWorklets - 1].process_handle->GetService());
        EXPECT_TRUE(
            data[kMaxActiveSellerWorklets - 1].process_handle->GetService());
        break;
    }

    EXPECT_EQ(operation.expected_total_handles, data.size());

    // The first kMaxActiveSellerWorklets ProcessHandles should all have
    // assigned processes, which should be the same only when the origin is the
    // same.
    for (size_t i = 0; i < data.size() && i < kMaxActiveSellerWorklets; ++i) {
      EXPECT_TRUE(data[i].process_handle->GetService());
      for (size_t j = 0; j < i; ++j) {
        EXPECT_EQ(data[i].origin == data[j].origin,
                  data[i].process_handle->GetService() ==
                      data[j].process_handle->GetService());
      }
    }

    // Make sure all pending tasks have been run.
    base::RunLoop().RunUntilIdle();

    // All other requests should not have been assigned processes yet.
    for (size_t i = kMaxActiveSellerWorklets; i < data.size(); ++i) {
      EXPECT_FALSE(data[i].run_loop->AnyQuitCalled());
      EXPECT_FALSE(data[i].process_handle->GetService());
    }
  }
}

// Test adding and removing bidders, exceeding the limit. This test does not
// cover the case of multiple bidders sharing the same process, which is covered
// by the next test.
TEST_F(AuctionProcessManagerTest, BidderLimitExceeded) {
  // The list of operations below assumes at least 3 bidders are allowed to run
  // at once.
  CHECK_GE(kMaxBidderProcesses, 3u);

  // Operations applied to the process manager. All requests use unique origins,
  // so no need to specify that.
  struct Operation {
    enum class Op {
      // Request the specified number of handle. If there are less than
      // kMaxBidderProcesses handles already, expects a process to be
      // immediately assigned. All requests use different origins from every
      // other request.
      kRequestHandles,

      // Destroy a handle with the given index. If the index is less than
      // kMaxBidderProcesses, then expect a ProcessHandle to have its
      // callback invoked, if there are more than kMaxBidderProcesses already.
      kDestroyHandle,

      // Same as destroy handle, but additionally destroys the next handle that
      // would have been assigned the next available process slot, and makes
      // sure the handle after that one gets a process instead.
      kDestroyHandleAndNextInQueue,
    };

    Op op;

    // Number of handles to request for kRequestHandles operations.
    absl::optional<size_t> num_handles;

    // Used for kDestroyHandle and kDestroyHandleAndNextInQueue operations.
    absl::optional<size_t> index;

    // The number of total handles expected after this operation. This can be
    // inferred by sum of requested handles requests less handles destroyed
    // handles, but having it explcitly in the struct makes sure the test cases
    // are testing what they're expected to.
    size_t expected_total_handles;
  };

  const Operation kOperationList[] = {
      {Operation::Op::kRequestHandles, kMaxBidderProcesses /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses /* expected_total_handles */},

      // Check destroying intermediate, last, and first handle when there are no
      // queued requests. Keep exactly kMaxBidderProcesses requests, to ensure
      // there are in fact first, last, and intermediate requests (as long as
      // kMaxBidderProcesses is at least 3).
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       1u /* index */, kMaxBidderProcesses - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       0u /* index */, kMaxBidderProcesses - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       kMaxBidderProcesses - 1 /* index */,
       kMaxBidderProcesses - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses /* expected_total_handles */},

      // Queue 3 more requests, but delete the last and first of them, to test
      // deleting queued requests.
      {Operation::Op::kRequestHandles, 3 /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses + 3 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       kMaxBidderProcesses /* index */,
       kMaxBidderProcesses + 2 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       kMaxBidderProcesses + 1 /* index */,
       kMaxBidderProcesses + 1 /* expected_total_handles */},

      // Request 4 more processes.
      {Operation::Op::kRequestHandles, 4 /* num_handles*/,
       absl::nullopt /* index */,
       kMaxBidderProcesses + 5 /* expected_total_handles */},

      // Destroy the first handle and the first pending in the queue immediately
      // afterwards. The next pending request should get a process.
      {Operation::Op::kDestroyHandleAndNextInQueue,
       absl::nullopt /* num_handles*/, 0u /* index */,
       kMaxBidderProcesses + 3 /* expected_total_handles */},

      // Destroy three more requests that have been asssigned processes, being
      // sure to destroy the first, last, and some request request with nether,
      // amongst requests with assigned processes.
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       kMaxBidderProcesses - 1 /* index */,
       kMaxBidderProcesses + 2 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       0u /* index */, kMaxBidderProcesses + 1 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       1u /* index */, kMaxBidderProcesses /* expected_total_handles */},
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
          ASSERT_EQ(original_size < kMaxBidderProcesses,
                    auction_process_manager_.RequestWorkletService(
                        AuctionProcessManager::WorkletType::kBidder,
                        distinct_origin, data.back().process_handle.get(),
                        data.back().run_loop->QuitClosure()));
        }
        break;

      case Operation::Op::kDestroyHandle: {
        size_t original_size = data.size();

        ASSERT_GT(data.size(), *operation.index);
        data.erase(data.begin() + *operation.index);
        // If destroying one of the first kMaxBidderProcesses handles, and
        // there were more than kMaxBidderProcesses handles before, the
        // first of the handles waiting on a process should get a process.
        if (*operation.index < kMaxBidderProcesses &&
            original_size > kMaxBidderProcesses) {
          data[kMaxBidderProcesses - 1].run_loop->Run();
          EXPECT_TRUE(
              data[kMaxBidderProcesses - 1].process_handle->GetService());
        }
        break;
      }

      case Operation::Op::kDestroyHandleAndNextInQueue: {
        ASSERT_GT(data.size(), *operation.index);
        ASSERT_GT(data.size(), kMaxBidderProcesses + 1);

        data.erase(data.begin() + *operation.index);
        data.erase(data.begin() + kMaxBidderProcesses);
        data[kMaxBidderProcesses - 1].run_loop->Run();
        EXPECT_TRUE(data[kMaxBidderProcesses - 1].process_handle->GetService());
        break;
      }
    }

    EXPECT_EQ(operation.expected_total_handles, data.size());

    // The first kMaxBidderProcesses ProcessHandles should all have
    // assigned processes, which should all be distinct.
    for (size_t i = 0; i < data.size() && i < kMaxBidderProcesses; ++i) {
      EXPECT_TRUE(data[i].process_handle->GetService());
      for (size_t j = 0; j < i; ++j) {
        EXPECT_NE(data[i].process_handle->GetService(),
                  data[j].process_handle->GetService());
      }
    }

    // Make sure all pending tasks have been run.
    base::RunLoop().RunUntilIdle();

    // All other requests should not have been assigned processes yet.
    for (size_t i = kMaxBidderProcesses; i < data.size(); ++i) {
      EXPECT_FALSE(data[i].run_loop->AnyQuitCalled());
      EXPECT_FALSE(data[i].process_handle->GetService());
    }
  }
}

// Check the bidder process sharing logic - specifically, that bidder requests
// share processes when origins match, and that bidders sharing processes don't
// count towards the process limit.
TEST_F(AuctionProcessManagerTest, BidderProcessSharing) {
  // This test assumes kMaxBidderProcesses is greater than 1.
  DCHECK_GT(kMaxBidderProcesses, 1u);

  // Make 2*kMaxBidderProcesses bidder worklet requests for each of
  // kMaxBidderProcesses different origins. All requests should succeed
  // immediately.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      bidders[kMaxBidderProcesses];
  for (size_t origin_index = 0; origin_index < kMaxBidderProcesses;
       ++origin_index) {
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://%zu.test", origin_index)));
    for (size_t i = 0; i < 2 * kMaxBidderProcesses; ++i) {
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
  EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_a2;
  auto bidder_delayed_a2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginA,
      bidder_delayed_a2.get(), run_loop_delayed_a2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_a2->GetService());
  EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_b;
  auto bidder_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      AuctionProcessManager::WorkletType::kBidder, kOriginB,
      bidder_delayed_b.get(), run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(bidder_delayed_b->GetService());
  EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());

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
    EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());
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
  EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());

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
  EXPECT_EQ(kMaxBidderProcesses, auction_process_manager_.NumReceivers());
}

TEST_F(AuctionProcessManagerTest, DestroyHandlesWithPendingSellerRequests) {
  // Make kMaxActiveSellerWorklets seller worklet requests.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < kMaxActiveSellerWorklets; ++i) {
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
  for (size_t i = 0; i < kMaxBidderProcesses; ++i) {
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
