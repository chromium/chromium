// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_request_manager.h"

#include <stdint.h>

#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

TrustedSignalsRequestManager::TrustedSignalsRequestManager(
    Type type,
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    bool automatically_send_requests,
    const url::Origin& top_level_origin,
    const GURL& trusted_signals_url,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param,
    AuctionV8Helper* v8_helper)
    : type_(type),
      url_loader_factory_(url_loader_factory),
      automatically_send_requests_(automatically_send_requests),
      top_level_origin_(top_level_origin),
      trusted_signals_url_(trusted_signals_url),
      experiment_group_id_(experiment_group_id),
      trusted_bidding_signals_slot_size_param_(
          trusted_bidding_signals_slot_size_param),
      v8_helper_(v8_helper),
      auction_network_events_handler_(
          std::move(auction_network_events_handler)) {
  // `trusted_bidding_signals_slot_size_param` are only supported for
  // Type::kBiddingSignals.
  DCHECK(trusted_bidding_signals_slot_size_param.empty() ||
         type_ == Type::kBiddingSignals);
}

TrustedSignalsRequestManager::~TrustedSignalsRequestManager() {
  // All outstanding Requests should have been destroyed before `this`.
  DCHECK(queued_requests_.empty());
  DCHECK(batched_requests_.empty());
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestBiddingSignals(
    const std::string& interest_group_name,
    const std::optional<std::vector<std::string>>& keys,
    int32_t max_trusted_bidding_signals_url_length,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kBiddingSignals, type_);

  std::unique_ptr<RequestImpl> request = std::make_unique<RequestImpl>(
      this, interest_group_name,
      keys ? std::set<std::string>(keys->begin(), keys->end())
           : std::set<std::string>(),
      max_trusted_bidding_signals_url_length, std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestScoringSignals(
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls,
    int32_t max_trusted_scoring_signals_url_length,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kScoringSignals, type_);

  std::unique_ptr<RequestImpl> request = std::make_unique<RequestImpl>(
      this, render_url,
      std::set<std::string>(ad_component_render_urls.begin(),
                            ad_component_render_urls.end()),
      max_trusted_scoring_signals_url_length, std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

bool TrustedSignalsRequestManager::RequestsURLSizeIsTooBig(
    std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
        requests,
    size_t limit) {
  std::string hostname = top_level_origin_.host();
  GURL signals_url;

  if (type_ == Type::kBiddingSignals) {
    std::set<std::string> interest_group_names;
    std::set<std::string> bidding_signals_keys;

    for (RequestImpl* request : requests) {
      interest_group_names.insert(request->interest_group_name_.value());
      if (request->bidder_keys_.has_value()) {
        bidding_signals_keys.insert(request->bidder_keys_->begin(),
                                    request->bidder_keys_->end());
      }
    }

    DCHECK(interest_group_names.size());
    signals_url = TrustedSignals::BuildTrustedBiddingSignalsURL(
        hostname, trusted_signals_url_, interest_group_names,
        bidding_signals_keys, experiment_group_id_,
        trusted_bidding_signals_slot_size_param_);
  } else {
    DCHECK_EQ(type_, Type::kScoringSignals);
    std::set<std::string> render_urls;
    std::set<std::string> ad_component_render_urls;

    for (RequestImpl* request : requests) {
      render_urls.insert(request->render_url_.value().spec());
      ad_component_render_urls.insert(
          request->ad_component_render_urls_->begin(),
          request->ad_component_render_urls_->end());
    }

    DCHECK(render_urls.size());
    signals_url = TrustedSignals::BuildTrustedScoringSignalsURL(
        hostname, trusted_signals_url_, render_urls, ad_component_render_urls,
        experiment_group_id_);
  }

  return signals_url.spec().size() > limit;
}

void TrustedSignalsRequestManager::IssueRequests(
    std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
        requests) {
  DCHECK(!requests.empty());
  BatchedTrustedSignalsRequest* batched_request =
      batched_requests_
          .emplace(std::make_unique<BatchedTrustedSignalsRequest>())
          .first->get();
  batched_request->requests = std::move(requests);

  if (type_ == Type::kBiddingSignals) {
    std::set<std::string> interest_group_names;
    std::set<std::string> bidding_signals_keys;
    for (RequestImpl* request : batched_request->requests) {
      interest_group_names.insert(request->interest_group_name_.value());
      if (request->bidder_keys_.has_value()) {
        bidding_signals_keys.insert(request->bidder_keys_->begin(),
                                    request->bidder_keys_->end());
        request->bidder_keys_.reset();
      }
      request->batched_request_ = batched_request;
    }

    DCHECK(interest_group_names.size());
    batched_request->trusted_signals = TrustedSignals::LoadBiddingSignals(
        url_loader_factory_, /*auction_network_events_handler=*/
        CreateNewAuctionNetworkEventsHandlerRemote(
            auction_network_events_handler_),
        std::move(interest_group_names), std::move(bidding_signals_keys),
        top_level_origin_.host(), trusted_signals_url_, experiment_group_id_,
        trusted_bidding_signals_slot_size_param_, v8_helper_,
        base::BindOnce(&TrustedSignalsRequestManager::OnSignalsLoaded,
                       base::Unretained(this), batched_request));
  } else {
    DCHECK_EQ(type_, Type::kScoringSignals);
    std::set<std::string> render_urls;
    std::set<std::string> ad_component_render_urls;

    for (RequestImpl* request : batched_request->requests) {
      render_urls.insert(request->render_url_->spec());
      ad_component_render_urls.insert(
          request->ad_component_render_urls_->begin(),
          request->ad_component_render_urls_->end());
      request->ad_component_render_urls_.reset();
      request->batched_request_ = batched_request;
    }

    DCHECK(render_urls.size());
    batched_request->trusted_signals = TrustedSignals::LoadScoringSignals(
        url_loader_factory_,
        /*auction_network_events_handler=*/
        CreateNewAuctionNetworkEventsHandlerRemote(
            auction_network_events_handler_),
        std::move(render_urls), std::move(ad_component_render_urls),
        top_level_origin_.host(), trusted_signals_url_, experiment_group_id_,
        v8_helper_,
        base::BindOnce(&TrustedSignalsRequestManager::OnSignalsLoaded,
                       base::Unretained(this), batched_request));
  }
}

void TrustedSignalsRequestManager::StartBatchedTrustedSignalsRequest() {
  if (queued_requests_.empty()) {
    // The timer should never be running when there are no pending requests.
    DCHECK(!timer_.IsRunning());
    return;
  }

  // No need to continue running the timer, if it's running.
  timer_.Stop();

  // Split the fetching URL by length limit pre-check if feature is enabled.
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeSplitTrustedSignalsFetchingURL)) {
    // Store requests and minimum length limit from last round in each for loop.
    std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
        merged_requests;
    size_t length_limit = std::numeric_limits<size_t>::max();

    // Each request is added to `putative_merged_requests`, and the putative URL
    // length is checked against a minimum limit. Whenever a request causes an
    // oversized URL, the current merged requests are issued and cleared. The
    // new request is then added to the set and awaits the next round of checks.
    for (auto& request : queued_requests_) {
      if (merged_requests.empty()) {
        merged_requests.insert(request);
        length_limit = request->max_trusted_signals_url_length_;

        continue;
      }

      std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
          putative_merged_requests = merged_requests;
      putative_merged_requests.insert(request);
      size_t putative_length_limit =
          std::min(length_limit, request->max_trusted_signals_url_length_);

      if (RequestsURLSizeIsTooBig(putative_merged_requests,
                                  putative_length_limit)) {
        IssueRequests(std::move(merged_requests));

        // After issuing the merged requests, place the current request in the
        // set and update the length limit.
        merged_requests = {request};
        length_limit = request->max_trusted_signals_url_length_;

        continue;
      }

      // If the current request would result in an oversized URL, update
      // merged_requests for the next round of checks.
      merged_requests = std::move(putative_merged_requests);
      length_limit = putative_length_limit;
    }

    // The merged requests should not be empty because it should contain at
    // least the request from the final loop.
    DCHECK(!merged_requests.empty());
    IssueRequests(std::move(merged_requests));
    queued_requests_.clear();

    return;
  }

  IssueRequests(std::move(queued_requests_));
  queued_requests_.clear();
  return;
}

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const std::string& interest_group_name,
    std::set<std::string> bidder_keys,
    int32_t max_trusted_bidding_signals_url_length,
    LoadSignalsCallback load_signals_callback)
    : interest_group_name_(interest_group_name),
      bidder_keys_(std::move(bidder_keys)),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {
  DCHECK(max_trusted_bidding_signals_url_length >= 0);
  if (max_trusted_bidding_signals_url_length == 0) {
    max_trusted_signals_url_length_ = std::numeric_limits<int>::max();
  } else {
    max_trusted_signals_url_length_ =
        base::checked_cast<size_t>(max_trusted_bidding_signals_url_length);
  }
}

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const GURL& render_url,
    std::set<std::string> ad_component_render_urls,
    int32_t max_trusted_scoring_signals_url_length,
    LoadSignalsCallback load_signals_callback)
    : render_url_(render_url),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {
  DCHECK(max_trusted_scoring_signals_url_length >= 0);
  if (max_trusted_scoring_signals_url_length == 0) {
    max_trusted_signals_url_length_ = std::numeric_limits<int>::max();
  } else {
    max_trusted_signals_url_length_ =
        base::checked_cast<size_t>(max_trusted_scoring_signals_url_length);
  }
}

TrustedSignalsRequestManager::RequestImpl::~RequestImpl() {
  if (trusted_signals_request_manager_) {
    trusted_signals_request_manager_->OnRequestDestroyed(this);
  }
}

TrustedSignalsRequestManager::BatchedTrustedSignalsRequest::
    BatchedTrustedSignalsRequest() = default;

TrustedSignalsRequestManager::BatchedTrustedSignalsRequest::
    ~BatchedTrustedSignalsRequest() = default;

bool TrustedSignalsRequestManager::CompareRequestImpl::operator()(
    const RequestImpl* r1,
    const RequestImpl* r2) const {
  if (r1->interest_group_name_.has_value() &&
      r2->interest_group_name_.has_value()) {
    DCHECK(!r1->render_url_.has_value() && !r2->render_url_.has_value());
    return std::tie(r1->interest_group_name_, r1) <
           std::tie(r2->interest_group_name_, r2);
  } else {
    DCHECK(r1->render_url_.has_value() && r2->render_url_.has_value());
    return std::tie(r1->render_url_, r1) < std::tie(r2->render_url_, r2);
  }
}

void TrustedSignalsRequestManager::OnSignalsLoaded(
    BatchedTrustedSignalsRequest* batched_request,
    scoped_refptr<Result> result,
    std::optional<std::string> error_msg) {
  DCHECK(batched_requests_.find(batched_request) != batched_requests_.end());
  for (RequestImpl* request : batched_request->requests) {
    DCHECK_EQ(request->batched_request_, batched_request);

    // Remove association with `this` and `batched_request` before invoking
    // callback, which may destroy the Request.
    request->trusted_signals_request_manager_ = nullptr;
    request->batched_request_ = nullptr;

    // It is illegal for this this to destroy another request, so
    // `batched_request->requests` should not be affected by invoking this,
    // other than the current element's pointer potentially now pointing to a
    // destroyed object.
    std::move(request->load_signals_callback_).Run(result, error_msg);
  }
  batched_requests_.erase(batched_requests_.find(batched_request));
}

void TrustedSignalsRequestManager::OnRequestDestroyed(RequestImpl* request) {
  // If the request is not assigned to a BatchedTrustedSignalsRequest, it's
  // still in `queued_requests_`, so remove it from that.
  if (!request->batched_request_) {
    size_t removed = queued_requests_.erase(request);
    DCHECK_EQ(removed, 1u);
    // If there are no more requests, stop the timer.
    if (queued_requests_.empty()) {
      timer_.Stop();
    }
    return;
  }

  // Otherwise, it should not be in `queued_requests_`.
  DCHECK_EQ(queued_requests_.count(request), 0u);

  // But it should be in the `requests` set of the
  // BatchedTrustedSignalsRequest it's pointing to.
  size_t removed = request->batched_request_->requests.erase(request);
  DCHECK_EQ(removed, 1u);

  // Cancel and delete the corresponding BatchedTrustedSignalsRequest if it's
  // no longer associated with any live requests.
  if (request->batched_request_->requests.empty()) {
    BatchedTrustedSignalsRequest* batched_request = request->batched_request_;
    request->batched_request_ = nullptr;
    batched_requests_.erase(batched_requests_.find(batched_request));
  }
}

void TrustedSignalsRequestManager::QueueRequest(RequestImpl* request) {
  // If the timer is not running, then either `automatically_send_requests_`
  // is false, or no requests should be in `queued_requests_`.
  DCHECK_EQ(timer_.IsRunning(),
            automatically_send_requests_ && !queued_requests_.empty());

  queued_requests_.insert(request);
  if (automatically_send_requests_ && !timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, kAutoSendDelay,
        base::BindOnce(
            &TrustedSignalsRequestManager::StartBatchedTrustedSignalsRequest,
            base::Unretained(this)));
  }
}

}  // namespace auction_worklet
