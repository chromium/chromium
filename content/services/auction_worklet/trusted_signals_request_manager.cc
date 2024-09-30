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
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_kvv2_signals.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

// Manages building and loading trusted signals URLs. Provides a shared
// interface for bidding signals URLs and scoring signals URLs.
class TrustedSignalsRequestManager::TrustedSignalsUrlBuilder {
 public:
  TrustedSignalsUrlBuilder& operator=(const TrustedSignalsUrlBuilder&) = delete;
  TrustedSignalsUrlBuilder(const TrustedSignalsUrlBuilder&) = delete;

  virtual ~TrustedSignalsUrlBuilder() = default;

  // Build a URL using the requests that have been added via `AddRequest`.
  virtual GURL Build() = 0;

  // Try including a new request in the URL. Return false if the request would
  // make the URL too big. If `split_fetch_` is false, this will always return
  // true.
  virtual bool TryToAddRequest(RequestImpl* request) = 0;

  // Reset the builder so that it can be used to build another URL.
  void Reset() {
    interest_group_names_.clear();
    bidding_signals_keys_.clear();
    render_urls_.clear();
    ad_component_render_urls_.clear();
    merged_requests_.clear();
    length_limit_ = std::numeric_limits<size_t>::max();
    added_first_request_ = false;
  }

  // Extract the requests that were included via `AddRequest`.
  std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
  TakeMergedRequests() {
    return std::move(merged_requests_);
  }

  // Extract the attributes needed to build and create trusted bidding signals.
  std::set<std::string> TakeInterestGroupNames() {
    // We should never try to build a bidding signals URL without
    // any interest group names.
    DCHECK(interest_group_names_.size());
    return std::move(interest_group_names_);
  }
  std::set<std::string> TakeBiddingSignalsKeys() {
    return std::move(bidding_signals_keys_);
  }

  // Extract the attributes needed to build and create trusted scoring signals.
  std::set<std::string> TakeRenderUrls() {
    // We should never try to build a scoring signals URL without any
    // render URLs.
    DCHECK(render_urls_.size());
    return std::move(render_urls_);
  }
  std::set<std::string> TakeAdComponentRenderUrls() {
    return std::move(ad_component_render_urls_);
  }

 protected:
  TrustedSignalsUrlBuilder(std::string hostname,
                           GURL trusted_signals_url,
                           std::optional<uint16_t> experiment_group_id,
                           bool split_fetch)
      : hostname_(std::move(hostname)),
        trusted_signals_url_(std::move(trusted_signals_url)),
        experiment_group_id_(experiment_group_id),
        split_fetch_(split_fetch) {}

  bool AddRequestIfUrlNotTooBig(RequestImpl* request) {
    if (!split_fetch_ || !added_first_request_ ||
        Build().spec().size() <=
            std::min(length_limit_, request->max_trusted_signals_url_length_)) {
      merged_requests_.insert(request);
      length_limit_ =
          std::min(length_limit_, request->max_trusted_signals_url_length_);
      added_first_request_ = true;
      return true;
    }
    return false;
  }

 private:
  friend class TrustedBiddingSignalsUrlBuilder;
  friend class TrustedScoringSignalsUrlBuilder;

  const std::string hostname_;

  const GURL trusted_signals_url_;

  const std::optional<uint16_t> experiment_group_id_;

  // Whether the URL should be split based on length limits.
  const bool split_fetch_;

  // The maximum allowed length of a URL with this group of `merged_requests_`.
  size_t length_limit_ = std::numeric_limits<size_t>::max();

  // Whether a request has been added to `merged_requests_` yet.
  bool added_first_request_ = false;

  // Parameters for building a bidding signals URL.
  std::set<std::string> interest_group_names_;
  std::set<std::string> bidding_signals_keys_;

  // Parameters for building a scoring signals URL.
  std::set<std::string> render_urls_;
  std::set<std::string> ad_component_render_urls_;

  std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
      merged_requests_;
};

class TrustedSignalsRequestManager::TrustedBiddingSignalsUrlBuilder
    : public TrustedSignalsRequestManager::TrustedSignalsUrlBuilder {
 public:
  TrustedBiddingSignalsUrlBuilder(
      std::string hostname,
      GURL trusted_signals_url,
      std::optional<uint16_t> experiment_group_id,
      std::string trusted_bidding_signals_slot_size_param,
      bool split_fetch)
      : TrustedSignalsUrlBuilder(std::move(hostname),
                                 std::move(trusted_signals_url),
                                 experiment_group_id,
                                 split_fetch),
        trusted_bidding_signals_slot_size_param_(
            std::move(trusted_bidding_signals_slot_size_param)) {}

  TrustedBiddingSignalsUrlBuilder& operator=(
      const TrustedBiddingSignalsUrlBuilder&) = delete;
  TrustedBiddingSignalsUrlBuilder(const TrustedBiddingSignalsUrlBuilder&) =
      delete;

  ~TrustedBiddingSignalsUrlBuilder() override = default;

  // TrustedSignalsUrlBuilder implementation.
  GURL Build() override {
    return TrustedSignals::BuildTrustedBiddingSignalsURL(
        hostname_, trusted_signals_url_, interest_group_names_,
        bidding_signals_keys_, experiment_group_id_,
        trusted_bidding_signals_slot_size_param_);
  }

  bool TryToAddRequest(RequestImpl* request) override {
    // Keep track of iterators of newly inserted values so they can easily be
    // removed if the URL gets too big.
    auto name_inserted =
        interest_group_names_.insert(request->interest_group_name_.value());
    std::vector<std::set<std::string>::iterator> inserted_keys;
    if (request->bidder_keys_.has_value()) {
      for (const std::string& key : *request->bidder_keys_) {
        auto inserted = bidding_signals_keys_.insert(key);
        if (inserted.second) {
          inserted_keys.push_back(inserted.first);
        }
      }
    }
    if (!AddRequestIfUrlNotTooBig(request)) {
      if (name_inserted.second) {
        interest_group_names_.erase(name_inserted.first);
      }
      for (const std::set<std::string>::iterator& key : inserted_keys) {
        bidding_signals_keys_.erase(key);
      }
      return false;
    }
    return true;
  }

 private:
  const std::string trusted_bidding_signals_slot_size_param_;
};

class TrustedSignalsRequestManager::TrustedScoringSignalsUrlBuilder
    : public TrustedSignalsRequestManager::TrustedSignalsUrlBuilder {
 public:
  TrustedScoringSignalsUrlBuilder(std::string hostname,
                                  GURL trusted_signals_url,
                                  std::optional<uint16_t> experiment_group_id,
                                  bool split_fetch)
      : TrustedSignalsUrlBuilder(std::move(hostname),
                                 std::move(trusted_signals_url),
                                 experiment_group_id,
                                 split_fetch) {}

  TrustedScoringSignalsUrlBuilder& operator=(
      const TrustedScoringSignalsUrlBuilder&) = delete;
  TrustedScoringSignalsUrlBuilder(const TrustedScoringSignalsUrlBuilder&) =
      delete;

  ~TrustedScoringSignalsUrlBuilder() override = default;

  // TrustedSignalsUrlBuilder implementation.
  GURL Build() override {
    return TrustedSignals::BuildTrustedScoringSignalsURL(
        hostname_, trusted_signals_url_, render_urls_,
        ad_component_render_urls_, experiment_group_id_);
  }

  bool TryToAddRequest(RequestImpl* request) override {
    // Keep track of iterators of newly inserted values so they can easily be
    // removed if the URL gets too big.
    auto added_render_url =
        render_urls_.insert(request->render_url_.value().spec());
    std::vector<std::set<std::string>::iterator> inserted_component_urls;
    for (const std::string& render_url : *request->ad_component_render_urls_) {
      auto inserted = ad_component_render_urls_.insert(render_url);
      if (inserted.second) {
        inserted_component_urls.push_back(inserted.first);
      }
    }
    if (!AddRequestIfUrlNotTooBig(request)) {
      if (added_render_url.second) {
        render_urls_.erase(added_render_url.first);
      }
      for (const std::set<std::string>::iterator& component_url :
           inserted_component_urls) {
        ad_component_render_urls_.erase(component_url);
      }
      return false;
    }
    return true;
  }
};

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
    mojom::TrustedSignalsPublicKeyPtr public_key,
    AuctionV8Helper* v8_helper)
    : type_(type),
      url_loader_factory_(url_loader_factory),
      automatically_send_requests_(automatically_send_requests),
      top_level_origin_(top_level_origin),
      trusted_signals_url_(trusted_signals_url),
      experiment_group_id_(experiment_group_id),
      trusted_bidding_signals_slot_size_param_(
          trusted_bidding_signals_slot_size_param),
      public_key_(std::move(public_key)),
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
  DCHECK(!public_key_);

  auto request = std::make_unique<RequestImpl>(
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

  auto request = std::make_unique<RequestImpl>(
      this, render_url,
      std::set<std::string>(ad_component_render_urls.begin(),
                            ad_component_render_urls.end()),
      max_trusted_scoring_signals_url_length, std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestKVv2BiddingSignals(
    const std::string& interest_group_name,
    const std::optional<std::vector<std::string>>& keys,
    const url::Origin& joining_origin,
    blink::mojom::InterestGroup::ExecutionMode execution_mode,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kBiddingSignals, type_);
  DCHECK(public_key_);

  auto request = std::make_unique<RequestImpl>(
      this, interest_group_name,
      keys ? std::set<std::string>(keys->begin(), keys->end())
           : std::set<std::string>(),
      joining_origin, execution_mode, std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

std::unique_ptr<TrustedSignalsRequestManager::Request>
TrustedSignalsRequestManager::RequestKVv2ScoringSignals(
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls,
    const url::Origin& bidder_owner_origin,
    const url::Origin& bidder_joining_origin,
    LoadSignalsCallback load_signals_callback) {
  DCHECK_EQ(Type::kScoringSignals, type_);

  auto request = std::make_unique<RequestImpl>(
      this, render_url,
      std::set<std::string>(ad_component_render_urls.begin(),
                            ad_component_render_urls.end()),
      bidder_owner_origin, bidder_joining_origin,
      std::move(load_signals_callback));
  QueueRequest(request.get());
  return request;
}

void TrustedSignalsRequestManager::IssueRequests(
    TrustedSignalsUrlBuilder& url_builder) {
  std::set<raw_ptr<RequestImpl, SetExperimental>, CompareRequestImpl>
      merged_requests = url_builder.TakeMergedRequests();
  DCHECK(!merged_requests.empty());
  BatchedTrustedSignalsRequest* batched_request =
      batched_requests_
          .emplace(std::make_unique<BatchedTrustedSignalsRequest>())
          .first->get();
  batched_request->requests = std::move(merged_requests);
  for (RequestImpl* request : batched_request->requests) {
    request->batched_request_ = batched_request;
  }

  if (type_ == Type::kBiddingSignals) {
    batched_request->trusted_signals = TrustedSignals::LoadBiddingSignals(
        url_loader_factory_, /*auction_network_events_handler=*/
        CreateNewAuctionNetworkEventsHandlerRemote(
            auction_network_events_handler_),
        url_builder.TakeInterestGroupNames(),
        url_builder.TakeBiddingSignalsKeys(), top_level_origin_.host(),
        trusted_signals_url_, experiment_group_id_,
        trusted_bidding_signals_slot_size_param_, v8_helper_,
        base::BindOnce(&TrustedSignalsRequestManager::OnSignalsLoaded,
                       base::Unretained(this), batched_request));
  } else {
    batched_request->trusted_signals = TrustedSignals::LoadScoringSignals(
        url_loader_factory_,
        /*auction_network_events_handler=*/
        CreateNewAuctionNetworkEventsHandlerRemote(
            auction_network_events_handler_),
        url_builder.TakeRenderUrls(), url_builder.TakeAdComponentRenderUrls(),
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

  // Trusted Signals KVv2 feature call flow.
  if (public_key_) {
    DCHECK(base::FeatureList::IsEnabled(
        blink::features::kFledgeTrustedSignalsKVv2Support));

    BatchedTrustedSignalsRequest* batched_request =
        batched_requests_
            .emplace(std::make_unique<BatchedTrustedSignalsRequest>())
            .first->get();
    batched_request->requests = std::move(queued_requests_);

    if (type_ == Type::kBiddingSignals) {
      // Append all interest group names and keys into a single set, and clear
      // them from each request, as they're no longer needed.
      std::set<std::string> interest_group_names;
      std::set<std::string> bidding_keys;

      auto helper_builder(
          std::make_unique<TrustedBiddingSignalsKVv2RequestHelperBuilder>(
              top_level_origin_.host(), experiment_group_id_,
              public_key_->Clone(), trusted_bidding_signals_slot_size_param_));

      for (auto& request : batched_request->requests) {
        CHECK(request->interest_group_name_.has_value());
        CHECK(request->bidder_keys_.has_value());
        CHECK(request->joining_origin_.has_value());
        CHECK(request->execution_mode_.has_value());

        request->SetKVv2IsolationIndex(helper_builder->AddTrustedSignalsRequest(
            request->interest_group_name_.value(),
            request->bidder_keys_.value(), request->joining_origin_.value(),
            request->execution_mode_.value()));
        interest_group_names.emplace(
            std::move(request->interest_group_name_).value());
        bidding_keys.insert(request->bidder_keys_->begin(),
                            request->bidder_keys_->end());
        request->bidder_keys_.reset();
        request->batched_request_ = batched_request;
      }

      // Using `Unretained(`) for `OnKVv2SignalsLoaded()` is safe because it is
      // triggered by
      // `TrustedKVv2Signals::HandleKVv2DownloadResultOnV8Thread()`, which is
      // protected by a WeakPtr. Additionally, the trusted_kvv2_signals object
      // is owned by the `batched_requests_` of the
      // trusted_signals_request_manager object, ensuring its lifetime is
      // managed appropriately.
      batched_request->trusted_kvv2_signals =
          TrustedKVv2Signals::LoadKVv2BiddingSignals(
              url_loader_factory_, /*auction_network_events_handler=*/
              CreateNewAuctionNetworkEventsHandlerRemote(
                  auction_network_events_handler_),
              interest_group_names, bidding_keys, trusted_signals_url_,
              std::move(helper_builder), v8_helper_,
              base::BindOnce(&TrustedSignalsRequestManager::OnKVv2SignalsLoaded,
                             base::Unretained(this), batched_request));
    } else {
      // Append all render urls and ad component render urls into a single set,
      // and clear them from each request, as they're no longer needed.
      std::set<std::string> render_urls;
      std::set<std::string> ad_component_render_urls;

      auto helper_builder(
          std::make_unique<TrustedScoringSignalsKVv2RequestHelperBuilder>(
              top_level_origin_.host(), experiment_group_id_,
              public_key_->Clone()));

      for (auto& request : batched_request->requests) {
        CHECK(request->render_url_.has_value());
        CHECK(request->ad_component_render_urls_.has_value());
        CHECK(request->bidder_owner_origin_.has_value());
        CHECK(request->bidder_joining_origin_.has_value());

        request->SetKVv2IsolationIndex(helper_builder->AddTrustedSignalsRequest(
            request->render_url_.value(),
            request->ad_component_render_urls_.value(),
            request->bidder_owner_origin_.value(),
            request->bidder_joining_origin_.value()));
        render_urls.emplace(request->render_url_->spec());
        ad_component_render_urls.insert(
            request->ad_component_render_urls_->begin(),
            request->ad_component_render_urls_->end());
        request->render_url_.reset();
        request->batched_request_ = batched_request;
      }

      // The claim regarding the `Unretained()` lifetime guarantee is the same
      // as in the bidding signals case.
      batched_request->trusted_kvv2_signals =
          TrustedKVv2Signals::LoadKVv2ScoringSignals(
              url_loader_factory_, /*auction_network_events_handler=*/
              CreateNewAuctionNetworkEventsHandlerRemote(
                  auction_network_events_handler_),
              render_urls, ad_component_render_urls, trusted_signals_url_,
              std::move(helper_builder), v8_helper_,
              base::BindOnce(&TrustedSignalsRequestManager::OnKVv2SignalsLoaded,
                             base::Unretained(this), batched_request));
    }

    return;
  }

  std::unique_ptr<TrustedSignalsUrlBuilder> url_builder;
  bool split_fetch = base::FeatureList::IsEnabled(
      blink::features::kFledgeSplitTrustedSignalsFetchingURL);
  if (type_ == Type::kBiddingSignals) {
    url_builder = std::make_unique<TrustedBiddingSignalsUrlBuilder>(
        top_level_origin_.host(), trusted_signals_url_, experiment_group_id_,
        trusted_bidding_signals_slot_size_param_, split_fetch);
  } else {
    url_builder = std::make_unique<TrustedScoringSignalsUrlBuilder>(
        top_level_origin_.host(), trusted_signals_url_, experiment_group_id_,
        split_fetch);
  }

  for (auto& request : queued_requests_) {
    if (!url_builder->TryToAddRequest(request)) {
      // The url got too big so split out what we already have.
      IssueRequests(*url_builder.get());
      url_builder->Reset();
      url_builder->TryToAddRequest(request);
    }
  }

  IssueRequests(*url_builder.get());
  queued_requests_.clear();

  return;
}

bool TrustedSignalsRequestManager::HasPublicKey() {
  return !public_key_.is_null();
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

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const std::string& interest_group_name,
    std::set<std::string> bidder_keys,
    const url::Origin& joining_origin,
    blink::mojom::InterestGroup::ExecutionMode execution_mode,
    LoadSignalsCallback load_signals_callback)
    : interest_group_name_(interest_group_name),
      bidder_keys_(std::move(bidder_keys)),
      joining_origin_(joining_origin),
      execution_mode_(execution_mode),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {}

TrustedSignalsRequestManager::RequestImpl::RequestImpl(
    TrustedSignalsRequestManager* trusted_signals_request_manager,
    const GURL& render_url,
    std::set<std::string> ad_component_render_urls,
    const url::Origin& bidder_owner_origin,
    const url::Origin& bidder_joining_origin,
    LoadSignalsCallback load_signals_callback)
    : render_url_(render_url),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      bidder_owner_origin_(bidder_owner_origin),
      bidder_joining_origin_(bidder_joining_origin),
      load_signals_callback_(std::move(load_signals_callback)),
      trusted_signals_request_manager_(trusted_signals_request_manager) {}

TrustedSignalsRequestManager::RequestImpl::~RequestImpl() {
  if (trusted_signals_request_manager_) {
    trusted_signals_request_manager_->OnRequestDestroyed(this);
  }
}

void TrustedSignalsRequestManager::RequestImpl::SetKVv2IsolationIndex(
    TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex index) {
  kvv2_isolation_index_ = index;
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

void TrustedSignalsRequestManager::OnKVv2SignalsLoaded(
    BatchedTrustedSignalsRequest* batched_request,
    std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
        result_map,
    std::optional<std::string> error_msg) {
  DCHECK(batched_requests_.find(batched_request) != batched_requests_.end());
  for (RequestImpl* request : batched_request->requests) {
    DCHECK_EQ(request->batched_request_, batched_request);

    // Remove association with `this` and `batched_request` before invoking
    // callback, which may destroy the Request.
    request->trusted_signals_request_manager_ = nullptr;
    request->batched_request_ = nullptr;

    if (result_map.has_value()) {
      DCHECK(request->kvv2_isolation_index_);
      auto result_it = result_map->find(request->kvv2_isolation_index_.value());
      if (result_it != result_map->end()) {
        std::move(request->load_signals_callback_)
            .Run(result_it->second, error_msg);
      } else {
        std::move(request->load_signals_callback_)
            .Run(nullptr,
                 base::StringPrintf(
                     "Failed to locate compression group \"%d\" and "
                     "partition \"%d\" in the result map.",
                     request->kvv2_isolation_index_->compression_group_id,
                     request->kvv2_isolation_index_->partition_id));
      }
    } else {
      std::move(request->load_signals_callback_).Run(nullptr, error_msg);
    }
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
