// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_same_process_auction_process_manager.h"

#include <memory>
#include <vector>

#include "base/process/process.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

TestSameProcessAuctionProcessManager::TestSameProcessAuctionProcessManager(
    TrustedSignalsCacheImpl* trusted_signals_cache)
    : DedicatedAuctionProcessManager(trusted_signals_cache) {}
TestSameProcessAuctionProcessManager::~TestSameProcessAuctionProcessManager() =
    default;

void TestSameProcessAuctionProcessManager::ResumeAllPaused() {
  for (const auto& svc : auction_worklet_services_) {
    for (const auto& v8_helper : svc->AuctionV8HelpersForTesting()) {
      v8_helper->v8_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<auction_worklet::AuctionV8Helper> v8_helper) {
                v8_helper->ResumeAllForTesting();
              },
              v8_helper));
    }
  }
}

int TestSameProcessAuctionProcessManager::NumBidderWorklets() const {
  int total = 0;
  for (const auto& svc : auction_worklet_services_) {
    total += svc->NumBidderWorkletsForTesting();
  }
  return total;
}

int TestSameProcessAuctionProcessManager::NumSellerWorklets() const {
  int total = 0;
  for (const auto& svc : auction_worklet_services_) {
    total += svc->NumSellerWorkletsForTesting();
  }
  return total;
}

AuctionProcessManager::WorkletProcess::ProcessContext
TestSameProcessAuctionProcessManager::CreateProcessInternal(
    WorkletProcess& worklet_process) {
  // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
  // production code. Don't bother to delete the service on pipe close,
  // though; just keep it in a vector instead.
  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  auction_worklet_services_.push_back(
      auction_worklet::AuctionWorkletServiceImpl::CreateForService(
          service.InitWithNewPipeAndPassReceiver()));
  return WorkletProcess::ProcessContext(std::move(service));
}

void TestSameProcessAuctionProcessManager::OnNewProcessAssigned(
    const ProcessHandle* handle) {
  handle->OnBaseProcessLaunchedForTesting(base::Process::Current());
}

}  // namespace content
