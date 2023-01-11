// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

class AuctionV8Helper;
class TrustedSignals;

// Manages trusted signals requests and responses. Currently only batches
// requests.
//
// TODO(https://crbug.com/1276639): Cache responses as well.
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
  // TODO(https://crbug.com/1279643): Investigate improving the
  // `automatically_send_requests` logic.
  TrustedSignalsRequestManager(
      Type type,
      network::mojom::URLLoaderFactory* url_loader_factory,
      bool automatically_send_requests,
      const url::Origin& top_level_origin,
      const GURL& trusted_signals_url,
      absl::optional<uint16_t> experiment_group_id,
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
      const absl::optional<std::vector<std::string>>& keys,
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
      LoadSignalsCallback load_signals_callback);

  // Starts a single TrustedSignals request for all currently queued Requests.
  void StartBatchedTrustedSignalsRequest();

  const GURL& trusted_signals_url() const { return trusted_signals_url_; }

 private:
  struct BatchedTrustedSignalsRequest;

  class RequestImpl : public Request {
   public:
    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const std::string& interest_group_name,
                std::set<std::string> bidder_keys,
                LoadSignalsCallback load_signals_callback);

    RequestImpl(TrustedSignalsRequestManager* trusted_signals_request_manager,
                const GURL& render_urls,
                std::set<std::string> ad_component_render_urls,
                LoadSignalsCallback load_signals_callback);

    RequestImpl(RequestImpl&) = delete;
    RequestImpl& operator=(RequestImpl&) = delete;
    ~RequestImpl() override;

   private:
    friend class TrustedSignalsRequestManager;

    // Used for requests for bidder signals. Must be non-null and non-empty for
    // bidder signals requests, null for scoring signals requests.
    absl::optional<std::string> interest_group_name_;
    absl::optional<std::set<std::string>> bidder_keys_;

    // Used for requests for scoring signals. `render_url_` must be non-null
    // and non-empty for scoring signals requests, and
    // `ad_component_render_urls_` non-null. Both must be null for bidding
    // signals requests.
    absl::optional<GURL> render_url_;
    // Stored as a std::set for simpler
    absl::optional<std::set<std::string>> ad_component_render_urls_;

    LoadSignalsCallback load_signals_callback_;

    // The TrustedSignalsRequestManager that created `this`. Cleared on
    // completion.
    raw_ptr<TrustedSignalsRequestManager> trusted_signals_request_manager_ =
        nullptr;

    // If this request is currently assigned to a batched request, points to
    // that request. nullptr otherwise.
    raw_ptr<BatchedTrustedSignalsRequest> batched_request_ = nullptr;
  };

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

    // The batched Requests this is for.
    std::set<RequestImpl*> requests;
  };

  // Adds `request` to `queued_requests_`, and starts `timer_` if needed.
  void QueueRequest(RequestImpl* request);

  void OnSignalsLoaded(BatchedTrustedSignalsRequest* batched_request,
                       scoped_refptr<Result> result,
                       absl::optional<std::string> error_msg);

  // Called when a request is destroyed. If it's in `queued_requests_`, removes
  // it. If there's a BatchedTrustedSignalsRequest for it, disassociates the
  // request with it, cancelling the request if it's no longer needed.
  void OnRequestDestroyed(RequestImpl* request);

  const Type type_;
  const raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  const bool automatically_send_requests_;
  const url::Origin top_level_origin_;
  const GURL trusted_signals_url_;
  const absl::optional<uint16_t> experiment_group_id_;
  const scoped_refptr<AuctionV8Helper> v8_helper_;

  // All live requests that haven't yet been assigned to a
  // BatchedTrustedSignalsRequest.
  std::set<RequestImpl*> queued_requests_;

  std::set<std::unique_ptr<BatchedTrustedSignalsRequest>,
           base::UniquePtrComparator>
      batched_requests_;

  base::OneShotTimer timer_;

  base::WeakPtrFactory<TrustedSignalsRequestManager> weak_ptr_factory{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_REQUEST_MANAGER_H_
