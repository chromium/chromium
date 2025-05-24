// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_bidding_signals_kvv1_requester.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/cpp/trusted_signals_url_builder.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/in_progress_auction_download.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

TrustedBiddingSignalsKVV1Requester::BatchedRequest IssueRequests(
    network::mojom::URLLoaderFactory& url_loader_factory,
    auction_worklet::TrustedBiddingSignalsUrlBuilder& url_builder,
    auction_worklet::mojom::AuctionNetworkEventsHandler& network_events_handler,
    std::vector<TrustedBiddingSignalsKVV1Requester::Request*> merged_requests) {
  auction_worklet::mojom::InProgressAuctionDownloadPtr download =
      auction_worklet::AuctionDownloader::StartDownload(
          url_loader_factory, url_builder.ComposeURL(),
          auction_worklet::AuctionDownloader::MimeType::kJson,
          network_events_handler);
  std::vector<size_t> ids;
  for (const TrustedBiddingSignalsKVV1Requester::Request* request :
       merged_requests) {
    ids.push_back(request->request_id());
  }
  merged_requests.clear();
  url_builder.Reset();
  return TrustedBiddingSignalsKVV1Requester::BatchedRequest(
      std::move(ids), std::move(download));
}

}  // namespace

TrustedBiddingSignalsKVV1Requester::Request::Request(
    const std::string& interest_group_name,
    std::set<std::string> keys,
    int32_t max_trusted_bidding_signals_url_length,
    TrustedBiddingSignalsKVV1Requester* requester,
    size_t request_id)
    : interest_group_name_(interest_group_name),
      keys_(std::move(keys)),
      max_trusted_bidding_signals_url_length_(
          max_trusted_bidding_signals_url_length == 0
              ? std::numeric_limits<int>::max()
              : max_trusted_bidding_signals_url_length),
      requester_(requester),
      request_id_(request_id) {
  DCHECK(requester);
  DCHECK(max_trusted_bidding_signals_url_length_ > 0);
}

TrustedBiddingSignalsKVV1Requester::Request::~Request() {
  if (requester_) {
    requester_->OnRequestDestroyed(this);
  }
}

TrustedBiddingSignalsKVV1Requester::BatchedRequest::BatchedRequest(
    std::vector<size_t> request_ids,
    auction_worklet::mojom::InProgressAuctionDownloadPtr download)
    : request_ids(std::move(request_ids)), download(std::move(download)) {}

TrustedBiddingSignalsKVV1Requester::BatchedRequest::~BatchedRequest() = default;

TrustedBiddingSignalsKVV1Requester::BatchedRequest::BatchedRequest(
    BatchedRequest&& other) = default;

TrustedBiddingSignalsKVV1Requester::TrustedBiddingSignalsKVV1Requester() =
    default;

TrustedBiddingSignalsKVV1Requester::~TrustedBiddingSignalsKVV1Requester() {
  CHECK(queued_requests_.empty());
}

std::unique_ptr<TrustedBiddingSignalsKVV1Requester::Request>
TrustedBiddingSignalsKVV1Requester::RequestBiddingSignals(
    const std::string& interest_group_name,
    const std::optional<std::vector<std::string>>& keys,
    int32_t max_trusted_bidding_signals_url_length) {
  auto request = base::WrapUnique(new Request(
      interest_group_name,
      keys ? std::set<std::string>(keys->begin(), keys->end())
           : std::set<std::string>(),
      max_trusted_bidding_signals_url_length, this, next_request_id_));
  queued_requests_.insert(request.get());
  ++next_request_id_;
  return request;
}

std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
TrustedBiddingSignalsKVV1Requester::StartBatchedRequest(
    network::mojom::URLLoaderFactory& url_loader_factory,
    auction_worklet::mojom::AuctionNetworkEventsHandler& network_events_handler,
    std::string top_level_host,
    GURL trusted_signals_url,
    std::optional<uint16_t> experiment_group_id,
    std::string trusted_bidding_signals_slot_size_param) {
  if (queued_requests_.empty()) {
    return {};
  }

  std::vector<BatchedRequest> output;

  std::vector<Request*> merged_requests;
  auction_worklet::TrustedBiddingSignalsUrlBuilder url_builder(
      std::move(top_level_host), std::move(trusted_signals_url),
      std::move(experiment_group_id),
      std::move(trusted_bidding_signals_slot_size_param));
  for (auto& request : queued_requests_) {
    if (!TryToAddRequest(url_builder, merged_requests, request)) {
      output.emplace_back(IssueRequests(url_loader_factory, url_builder,
                                        network_events_handler,
                                        std::move(merged_requests)));
      merged_requests.clear();
      TryToAddRequest(url_builder, merged_requests, request);
    }
    // We're about to clear the `request` from `queued_requests_` so we no
    // longer need to keep track of the `requester_`.
    request->requester_ = nullptr;
  }
  output.emplace_back(IssueRequests(url_loader_factory, url_builder,
                                    network_events_handler,
                                    std::move(merged_requests)));
  queued_requests_.clear();
  return output;
}

bool TrustedBiddingSignalsKVV1Requester::CompareRequest::operator()(
    const Request* r1,
    const Request* r2) const {
  return std::tie(r1->interest_group_name_, r1) <
         std::tie(r2->interest_group_name_, r2);
}

// static
bool TrustedBiddingSignalsKVV1Requester::TryToAddRequest(
    auction_worklet::TrustedBiddingSignalsUrlBuilder& bidding_url_builder,
    std::vector<Request*>& merged_requests,
    Request* request) {
  bool success = bidding_url_builder.TryToAddRequest(
      request->interest_group_name_, request->keys_,
      request->max_trusted_bidding_signals_url_length_);
  if (success) {
    merged_requests.push_back(request);
  }
  return success;
}

void TrustedBiddingSignalsKVV1Requester::OnRequestDestroyed(Request* request) {
  bool erased = queued_requests_.erase(request);
  DCHECK(erased);
}

}  // namespace content
