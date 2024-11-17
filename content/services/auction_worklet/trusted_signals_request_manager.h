// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_kvv2_signals.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

class AuctionV8Helper;
class TrustedSignals;

// Manages trusted signals requests and responses. Currently only batches
// requests.
//
// TODO(crbug.com/40207533): Cache responses as well.
class CONTENT_EXPORT TrustedSignalsRequestManager {
 public:
  // Delay between construction of a Request and automatically starting a
  // network request when `automatically_send_requests` is true.
  static constexpr base::TimeDelta kAutoSendDelay = base::Milliseconds(10);

  // Use same callback and result classes as TrustedSignals.
  using LoadSignalsCallback = TrustedSignals::LoadSignalsCallback;
  using Result = TrustedSignals::Result;

  enum class Type {
    kBiddingSignals,
    kScoringSignals,
  };

  // Represents a single pending request for TrustedSignals from a consumer.
  // Destroying it cancels the request. All live Requests must be destroyed
  // before the TrustedSignalsRequestManager used to create them.
  //
  // It is illegal to destroy other pending Requests when a Request's callback
  // is invoked.
  class Request {
   public:
    Request(Request&) = delete;
    Request& operator=(Request&) = delete;
    virtual ~Request() = default;

   protected:
    Request() = default;
  };

  // Creates a TrustedSignalsRequestManager object for a worklet with the
  // provided parameters.  `type` indicates if this is for bidding or scoring
  // signals. A single TrustedSignalsRequestManager object may only be used with
  // worklets with a single `trusted_signals_url` and running auctions for a
  // single `top_level_origin`.
  //
  // `url_loader_factory` must remain valid for the lifetime of the
  // TrustedSignalsRequestManager. Keeps an owning reference to `v8_helper`.
  //
  // If `automatically_send_requests` is true, network requests will be
  // automatically sent as if StartBatchedTrustedSignalsRequest() were invoked
  // after there's been an outstanding Request for kAutoSendDelay. Manually
  // calling StartBatchedTrustedSignalsRequest() will still start a
  // TrustedSignals request for any pending requests and reset the send
  // interval.
  //
  // If non-empty the bidder-only parameter,
  // "&`trusted_bidding_signals_slot_size_param`" is appended to the end of the
  // query string. It's expected to already be escaped if necessary.
  //
  // The `public_key` pointer indicates whether this trusted signals
  // request manager is intended for the KVv2 version. If the pointer is
  // non-null, it will trigger the KVv2 call flow and be used during the message
  // encryption and decryption process.
  //
  // TODO(crbug.com/40810962): Investigate improving the
  // `automatically_send_requests` logic.
  TrustedSignalsRequestManager(
      Type type,
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      bool automatically_send_requests,
      const url::Origin& top_level_origin,
      const GURL& trusted_signals_url,
      std::optional<uint16_t> experiment_group_id,
      const std::string& trusted_bidding_signals_slot_size_param,
      mojom::TrustedSignalsPublicKeyPtr public_key,
      AuctionV8Helper* v8_helper);

  explicit TrustedSignalsRequestManager(const TrustedSignalsRequestManager&) =
      delete;
  TrustedSignalsRequestManager& operator=(const TrustedSignalsRequestManager&) =
      delete;

  ~TrustedSignalsRequestManager();

  // Queues a bidding signals request. Does not start a network request until
  // StartBatchedTrustedSignalsRequest() is invoked. `this` must be of Type
  // kBiddingSignals.
  std::unique_ptr<Request> RequestBiddingSignals(
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>& keys,
      int32_t max_trusted_bidding_signals_url_length,
      LoadSignalsCallback load_signals_callback);

  // Queues a scoring signals request. Does not start a network request until
  // StartBatchedTrustedSignalsRequest() is invoked. `this` must be of Type
  // kScoringSignals.
  //
  // `ad_component_render_urls` are taken as a vector of std::strings so that
  // the format matches the one accepted by ScoringSignals::Result, which
  // minimizes conversions.
  std::unique_ptr<Request> RequestScoringSignals(
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls,
      int32_t max_trusted_scoring_signals_url_length,
      LoadSignalsCallback load_signals_callback);

  // Like `RequestBiddingSignals()`, but for trusted bidding signals KVv2
  // support. Requires `joining_origin` and `execution_mode` instead of
  // `max_trusted_bidding_signals_url_length`.
  std::unique_ptr<Request> RequestKVv2BiddingSignals(
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>& keys,
      const url::Origin& joining_origin,
      blink::mojom::InterestGroup::ExecutionMode execution_mode,
      LoadSignalsCallback load_signals_callback);

  // Like `RequestScoringSignals()`, but for trusted scoring signals KVv2
  // support. Requires `bidder_owner_origin` and `bidder_joining_origin` instead
  // of `max_trusted_scoring_signals_url_length`.
  std::unique_ptr<Request> RequestKVv2ScoringSignals(
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls,
      const url::Origin& bidder_owner_origin,
      const url::Origin& bidder_joining_origin,
      LoadSignalsCallback load_signals_callback);

  // Starts a single TrustedSignals request for all currently queued
  // Requests.
  void StartBatchedTrustedSignalsRequest();

  const GURL& trusted_signals_url() const { return trusted_signals_url_; }

  bool HasPublicKey();

 private:
  struct BatchedTrustedSignalsRequest;

  class RequestImpl : public Request {
   public:
    // Constructor for the BYOS version of trusted bidding signals, which builds
    // a GET request with a limit set by max_trusted_bidding_signals_url_length.
    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const std::string& interest_group_name,
                std::set<std::string> bidder_keys,
                int32_t max_trusted_bidding_signals_url_length,
                LoadSignalsCallback load_signals_callback);

    // Constructor for the BYOS version of trusted scoring signals, which builds
    // a GET request with a limit set by max_trusted_scoring_signals_url_length.
    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const GURL& render_url,
                std::set<std::string> ad_component_render_urls,
                int32_t max_trusted_scoring_signals_url_length,
                LoadSignalsCallback load_signals_callback);

    // Constructor for trusted bidding signals with KVv2 support, which builds a
    // POST request.
    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const std::string& interest_group_name,
                std::set<std::string> bidder_keys,
                const url::Origin& joining_origin,
                blink::mojom::InterestGroup::ExecutionMode execution_mode,
                LoadSignalsCallback load_signals_callback);

    // Constructor for trusted scoring signals with KVv2 support, which builds a
    // POST request.
    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const GURL& render_url,
                std::set<std::string> ad_component_render_urls,
                const url::Origin& bidder_owner_origin,
                const url::Origin& bidder_joining_origin,
                LoadSignalsCallback load_signals_callback);

    RequestImpl(RequestImpl&) = delete;
    RequestImpl& operator=(RequestImpl&) = delete;
    ~RequestImpl() override;

    void SetKVv2IsolationIndex(
        TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index);

   private:
    friend class TrustedSignalsRequestManager;

    // Used for requests for bidder signals. Must be non-null and non-empty for
    // bidder signals requests, null for scoring signals requests.
    std::optional<std::string> interest_group_name_;
    std::optional<std::set<std::string>> bidder_keys_;
    std::optional<url::Origin> joining_origin_;
    std::optional<blink::mojom::InterestGroup::ExecutionMode> execution_mode_;

    // Used for requests for scoring signals. `render_url_` must be non-null
    // and non-empty for scoring signals requests, and
    // `ad_component_render_urls_` non-null. Both must be null for bidding
    // signals requests.
    std::optional<GURL> render_url_;
    // Stored as a std::set for simpler
    std::optional<std::set<std::string>> ad_component_render_urls_;
    std::optional<url::Origin> bidder_owner_origin_;
    std::optional<url::Origin> bidder_joining_origin_;

    size_t max_trusted_signals_url_length_;

    LoadSignalsCallback load_signals_callback_;

    // The TrustedSignalsRequestManager that created `this`. Cleared on
    // completion.
    raw_ptr<TrustedSignalsRequestManager> trusted_signals_request_manager_ =
        nullptr;

    // If this request is currently assigned to a batched request, points to
    // that request. nullptr otherwise.
    raw_ptr<BatchedTrustedSignalsRequest> batched_request_ = nullptr;

    // When a request is added to
    // TrustedBiddingSignalsKVv2RequestHelperBuilder, it returns the assigned
    // partition ID and compression group ID for the request. These returned
    // IDs are stored in the request to locate the correct compression group
    // and partition in the response, avoiding the need to search the entire
    // map.
    std::optional<TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex>
        kvv2_isolation_index_;
  };

  // Use interest group name or render url as customized comparator for bidding
  // or scoring requests.
  struct CompareRequestImpl {
    bool operator()(const RequestImpl* r1, const RequestImpl* r2) const;
  };

  // Manages building and loading trusted signals URLs.
  class TrustedSignalsUrlBuilder;
  class TrustedBiddingSignalsUrlBuilder;
  class TrustedScoringSignalsUrlBuilder;

  // Manages a single TrustedSignals object, which is associated with one or
  // more Requests. Tracks all associated live Requests, and manages invoking
  // their callbacks. Only created when a TrustedSignals request is started.
  // Lives entirely on the UI thread.
  struct BatchedTrustedSignalsRequest {
   public:
    BatchedTrustedSignalsRequest();
    BatchedTrustedSignalsRequest(BatchedTrustedSignalsRequest&) = delete;
    BatchedTrustedSignalsRequest& operator=(BatchedTrustedSignalsRequest&) =
        delete;
    ~BatchedTrustedSignalsRequest();

    std::unique_ptr<TrustedSignals> trusted_signals;
    std::unique_ptr<TrustedKVv2Signals> trusted_kvv2_signals;

    // The batched Requests this is for.
    std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
        requests;
  };

  // Adds `request` to `queued_requests_`, and starts `timer_` if needed.
  void QueueRequest(RequestImpl* request);

  void OnSignalsLoaded(BatchedTrustedSignalsRequest* batched_request,
                       scoped_refptr<Result> result,
                       std::optional<std::string> error_msg);

  // Callback for the trusted signals KVv2 process, where the return value of
  // `DeliverKVv2CallbackOnUserThread` in `TrustedKVv2Signals` is an optional
  // `TrustedKVv2Signals::TrustedSignalsResultMap`.
  void OnKVv2SignalsLoaded(
      BatchedTrustedSignalsRequest* batched_request,
      std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
          result_map,
      std::optional<std::string> error_msg);

  // Called when a request is destroyed. If it's in `queued_requests_`, removes
  // it. If there's a BatchedTrustedSignalsRequest for it, disassociates the
  // request with it, cancelling the request if it's no longer needed.
  void OnRequestDestroyed(RequestImpl* request);

  void IssueRequests(TrustedSignalsUrlBuilder& url_builder);

  const Type type_;
  const raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  const bool automatically_send_requests_;
  const url::Origin top_level_origin_;
  const GURL trusted_signals_url_;
  const std::optional<uint16_t> experiment_group_id_;
  const std::string trusted_bidding_signals_slot_size_param_;
  // Fetched in the browser process based on the trusted bidding/scoring signals
  // coordinator, to be used for encrypting the trusted KVv2 signals request
  // body.
  const mojom::TrustedSignalsPublicKeyPtr public_key_;
  const scoped_refptr<AuctionV8Helper> v8_helper_;

  // All live requests that haven't yet been assigned to a
  // BatchedTrustedSignalsRequest.
  std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
      queued_requests_;

  std::set<std::unique_ptr<BatchedTrustedSignalsRequest>,
           base::UniquePtrComparator>
      batched_requests_;

  base::OneShotTimer timer_;

  mojo::Remote<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handler_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_
