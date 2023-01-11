// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_request_manager.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

TrustedSignalsRequestManager::TrustedSignalsRequestManager(
    Type type,
    network::mojom::URLLoaderFactory* url_loader_factory,
    bool automatically_send_requests,
    const url::Origin& top_level_origin,
    const GURL& trusted_signals_url,
    absl::optional<uint16_t> experiment_group_id,
    AuctionV8Helper* v8_helper)
    : type_(type),
      url_loader_factory_(url_loader_factory),
      automatically_send_requests_(automatically_send_requests),
      top_level_origin_(top_level_origin),
      trusted_signals_url_(trusted_signals_url),
      experiment_group_id_(experiment_group_id),
      v8_helper_(v8_helper) {}

TrustedSignalsRequestManager::~TrustedSignalsRequestManager() {
  // All outstanding Requests should have been destroyed before `this`.
  DCHECK(queued_requests_.empty());
  DCHECK(batched_requests_.empty());
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestBiddingSignals(
    const std::string& interest_group_name,
    const absl::optional<std::vector<std::string>>& keys,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kBiddingSignals, type_);

  std::unique_ptr<RequestImpl> request = std::make_unique<RequestImpl>(
      this, interest_group_name,
      keys ? std::set<std::string>(keys->begin(), keys->end())
           : std::set<std::string>(),
      std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestScoringSignals(
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kScoringSignals, type_);

  std::unique_ptr<RequestImpl> request = std::make_unique<RequestImpl>(
      this, render_url,
      std::set<std::string>(ad_component_render_urls.begin(),
                            ad_component_render_urls.end()),
      std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

void TrustedSignalsRequestManager::StartBatchedTrustedSignalsRequest() {
  if (queued_requests_.empty()) {
    // The timer should never be running when there are no pending requests.
    DCHECK(!timer_.IsRunning());
    return;
  }

  // No need to continue running the timer, if it's running.
  timer_.Stop();

  BatchedTrustedSignalsRequest* batched_request =
      batched_requests_
          .emplace(std::make_unique<BatchedTrustedSignalsRequest>())
          .first->get();
  batched_request->requests = std::move(queued_requests_);
  if (type_ == Type::kBiddingSignals) {
    // Append all interest group names and keys into a single set, and clear
    // them from each request, as they're no longer needed. Consumers provide
    // their own values again when they request data from the
    // TrustedSignals::Results returned by `this`.
    std::set<std::string> interest_group_names;
    std::set<std::string> keys;
    for (RequestImpl* request : batched_request->requests) {
      interest_group_names.emplace(
          std::move(request->interest_group_name_).value());
      keys.insert(request->bidder_keys_->begin(), request->bidder_keys_->end());
      request->bidder_keys_.reset();
      request->batched_request_ = batched_request;
    }
    batched_request->trusted_signals = TrustedSignals::LoadBiddingSignals(
        url_loader_factory_, std::move(interest_group_names), std::move(keys),
        top_level_origin_.host(), trusted_signals_url_, experiment_group_id_,
        v8_helper_,
        base::BindOnce(&TrustedSignalsRequestManager::OnSignalsLoaded,
                       base::Unretained(this), batched_request));
    return;
  }

  DCHECK_EQ(type_, Type::kScoringSignals);
  // Append urls into two sets, and clear each request's URLs, as they're no
  // longer needed.
  std::set<std::string> render_urls;
  std::set<std::string> ad_component_render_urls;
  for (RequestImpl* request : batched_request->requests) {
    render_urls.insert(request->render_url_->spec());
    ad_component_render_urls.insert(request->ad_component_render_urls_->begin(),
                                    request->ad_component_render_urls_->end());
    request->render_url_.reset();
    request->ad_component_render_urls_.reset();
    request->batched_request_ = batched_request;
  }
  batched_request->trusted_signals = TrustedSignals::LoadScoringSignals(
      url_loader_factory_, std::move(render_urls),
      std::move(ad_component_render_urls), top_level_origin_.host(),
      trusted_signals_url_, experiment_group_id_, v8_helper_,
      base::BindOnce(&TrustedSignalsRequestManager::OnSignalsLoaded,
                     base::Unretained(this), batched_request));
}

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const std::string& interest_group_name,
    std::set<std::string> bidder_keys,
    LoadSignalsCallback load_signals_callback)
    : interest_group_name_(interest_group_name),
      bidder_keys_(std::move(bidder_keys)),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {}

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const GURL& render_url,
    std::set<std::string> ad_component_render_urls,
    LoadSignalsCallback load_signals_callback)
    : render_url_(render_url),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {}

TrustedSignalsRequestManager::RequestImpl::~RequestImpl() {
  if (trusted_signals_request_manager_)
    trusted_signals_request_manager_->OnRequestDestroyed(this);
}

TrustedSignalsRequestManager::BatchedTrustedSignalsRequest::
    BatchedTrustedSignalsRequest() = default;

TrustedSignalsRequestManager::BatchedTrustedSignalsRequest::
    ~BatchedTrustedSignalsRequest() = default;

void TrustedSignalsRequestManager::OnSignalsLoaded(
    BatchedTrustedSignalsRequest* batched_request,
    scoped_refptr<Result> result,
    absl::optional<std::string> error_msg) {
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
    if (queued_requests_.empty())
      timer_.Stop();
    return;
  }

  // Otherwise, it should not be in `queued_requests_`.
  DCHECK_EQ(queued_requests_.count(request), 0u);

  // But it should be in the `requests` set of the BatchedTrustedSignalsRequest
  // it's pointing to.
  size_t removed = request->batched_request_->requests.erase(request);
  DCHECK_EQ(removed, 1u);

  // Cancel and delete the corresponding BatchedTrustedSignalsRequest if it's
  // no longer associated with any live requests.
  if (request->batched_request_->requests.empty())
    batched_requests_.erase(
        batched_requests_.find(request->batched_request_.get()));
}

void TrustedSignalsRequestManager::QueueRequest(RequestImpl* request) {
  // If the timer is not running, then either `automatically_send_requests_` is
  // false, or no requests should be in `queued_requests_`.
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
