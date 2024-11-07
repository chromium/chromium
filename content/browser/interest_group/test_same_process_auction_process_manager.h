// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_SAME_PROCESS_AUCTION_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_SAME_PROCESS_AUCTION_PROCESS_MANAGER_H_

#include <memory>
#include <vector>

#include "content/browser/interest_group/auction_process_manager.h"

namespace auction_worklet {
class AuctionWorkletServiceImpl;
}

namespace content {

class TrustedSignalsCacheImpl;

// AuctionProcessManager that allows running auctions in-process, using real
// worklets.
class TestSameProcessAuctionProcessManager
    : public DedicatedAuctionProcessManager {
 public:
  explicit TestSameProcessAuctionProcessManager(
      TrustedSignalsCacheImpl* trusted_signals_cache = nullptr);
  TestSameProcessAuctionProcessManager(
      const TestSameProcessAuctionProcessManager&) = delete;
  TestSameProcessAuctionProcessManager& operator=(
      const TestSameProcessAuctionProcessManager&) = delete;
  ~TestSameProcessAuctionProcessManager() override;

  // Resume all worklets paused waiting for debugger on startup.
  void ResumeAllPaused();

  // Number of live bidder/seller worklets across all created services.
  int NumBidderWorklets() const;
  int NumSellerWorklets() const;

 private:
  WorkletProcess::ProcessContext CreateProcessInternal(
      WorkletProcess& worklet_process) override;
  void OnNewProcessAssigned(const ProcessHandle* handle) override;

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_SAME_PROCESS_AUCTION_PROCESS_MANAGER_H_
