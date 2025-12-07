// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_BIDDING_SIGNALS_KVV1_REQUESTER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_BIDDING_SIGNALS_KVV1_REQUESTER_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/cpp/trusted_signals_url_builder.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/in_progress_auction_download.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Manages batching trusted bidding KVv1 URL requests. A
// `TrustedBiddingSignalsKVV1Requester` can be used to accrue requests that
// share a `top_level_host`, `trusted_signals_url`, `experiment_group_id`, and
// `trusted_bidding_signals_slot_size_param`. Each request can be added to
// TrustedBiddingSignalsKVV1Requester with RequestBiddingSignals(), and the
// requests will be batched and started with StartBatchedRequest(), which may
// generate multiple batched URL requests if the combined URL length for a batch
// exceeds any `max_trusted_bidding_signals_url_length`.
class CONTENT_EXPORT TrustedBiddingSignalsKVV1Requester {
 public:
  // Represents a single pending request for trusted bidding signals from a
  // consumer. Destroying it cancels the request if StartBatchedRequest() has
  // not yet been called. Requests must be destroyed before the
  // TrustedBiddingSignalsKVV1Requester used to create them.
  // TrustedBiddingSignalsKVV1Requester ensures each Request has a unique
  // `request_id`.
  class CONTENT_EXPORT Request {
   public:
    ~Request();

    // Return an ID unique to this Request.
    size_t request_id() const { return request_id_; }

   private:
    friend class TrustedBiddingSignalsKVV1Requester;

    // Make a Request. `request_id` should be unique to this
    // Request.
    Request(const std::string& interest_group_name,
            std::set<std::string> keys,
            int32_t max_trusted_bidding_signals_url_length,
            TrustedBiddingSignalsKVV1Requester* requester,
            size_t request_id);

    const std::string interest_group_name_;
    const std::set<std::string> keys_;
    const int32_t max_trusted_bidding_signals_url_length_;
    raw_ptr<TrustedBiddingSignalsKVV1Requester> requester_;
    const size_t request_id_;
  };

  struct CONTENT_EXPORT BatchedRequest {
   public:
    BatchedRequest(
        std::vector<size_t> request_ids,
        auction_worklet::mojom::InProgressAuctionDownloadPtr download);

    ~BatchedRequest();

    BatchedRequest(BatchedRequest&& other);

    std::vector<size_t> request_ids;
    auction_worklet::mojom::InProgressAuctionDownloadPtr download;
  };

  TrustedBiddingSignalsKVV1Requester();

  ~TrustedBiddingSignalsKVV1Requester();

  // Queues a request. Does not start the request until StartBatchedRequest() is
  // invoked.
  std::unique_ptr<Request> RequestBiddingSignals(
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>& keys,
      int32_t max_trusted_bidding_signals_url_length);

  // Starts all currently queued Requests.
  // Returns a list of the in-progress requests (InProgressAuctionDownloadPtr)
  // along with the `request_id`s associated with each request.
  // Clears internal state, so this class can be reused to batch new requests.
  // If this function is called when there are no queued requests, it will
  // return an empty vector.
  std::vector<BatchedRequest> StartBatchedRequest(
      network::mojom::URLLoaderFactory& url_loader_factory,
      auction_worklet::mojom::AuctionNetworkEventsHandler&
          network_events_handler,
      std::string top_level_host,
      GURL trusted_signals_url,
      std::optional<uint16_t> experiment_group_id,
      std::string trusted_bidding_signals_slot_size_param);

 private:
  // Requests are sorted by interest group name to increase the chance that
  // requests for the same IG are included in the same batch. Requests for the
  // same group can likely be merged together without an increase in URL length,
  // since we expect most parameters to be the same.
  struct CompareRequest {
    bool operator()(const Request* r1, const Request* r2) const;
  };

  static bool TryToAddRequest(
      auction_worklet::TrustedBiddingSignalsUrlBuilder& bidding_url_builder,
      std::vector<Request*>& merged_requests,
      Request* request);

  // Removes a request from the queue when the Request is destroyed.
  void OnRequestDestroyed(Request* request);

  std::set<raw_ptr<Request>, CompareRequest> queued_requests_;

  size_t next_request_id_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_BIDDING_SIGNALS_KVV1_REQUESTER_H_
