// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// 10 kb update size limit. We are potentially fetching many interest group
// updates, so don't let this get too large.
constexpr size_t kMaxUpdateSize = 10 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("interest_group_update_fetcher", R"(
        semantics {
          sender: "Interest group periodic update fetcher"
          description:
            "Fetches periodic updates of interest groups previously joined by "
            "navigator.joinAdInterestGroup(). JavaScript running in the "
            "context of a frame cannot read interest groups, but it can "
            "request that all interest groups owned by the current frame's "
            "origin be updated by fetching JSON from the registered update URL."
            "See https://github.com/WICG/turtledove/blob/main/FLEDGE.md"
          trigger:
            "Fetched upon a navigator.updateAdInterestGroups() call."
          data: "URL registered for updating this interest group."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests are controlled by a feature flag that is off by "
            "default now. When enabled, they can be disabled by the Privacy"
            " Sandbox setting."
          policy_exception_justification:
            "These requests are triggered by a website."
        })");

}  // namespace

InterestGroupManager::InterestGroupManager(
    const base::FilePath& path,
    bool in_memory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : impl_(base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            in_memory ? base::FilePath() : path),
      auction_process_manager_(std::make_unique<AuctionProcessManager>()),
      url_loader_factory_(std::move(url_loader_factory)) {}

InterestGroupManager::~InterestGroupManager() = default;

void InterestGroupManager::JoinInterestGroup(blink::InterestGroup group,
                                             const GURL& joining_url) {
  impl_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(joining_url));
}

void InterestGroupManager::LeaveInterestGroup(const ::url::Origin& owner,
                                              const std::string& name) {
  impl_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(owner, name);
}

void InterestGroupManager::UpdateInterestGroup(blink::InterestGroup group) {
  impl_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(std::move(group));
}

void InterestGroupManager::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  ClaimInterestGroupsForUpdate(
      owner,
      base::BindOnce(
          &InterestGroupManager::DidUpdateInterestGroupsOfOwnerDbLoad,
          weak_factory_.GetWeakPtr(), owner, std::move(client_security_state)));
}

void InterestGroupManager::RecordInterestGroupBid(const ::url::Origin& owner,
                                                  const std::string& name) {
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupBid)
      .WithArgs(owner, name);
}

void InterestGroupManager::RecordInterestGroupWin(const ::url::Origin& owner,
                                                  const std::string& name,
                                                  const std::string& ad_json) {
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(owner, name, std::move(ad_json));
}

void InterestGroupManager::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwners)
      .Then(std::move(callback));
}

void InterestGroupManager::GetInterestGroupsForOwner(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
      .WithArgs(owner)
      .Then(std::move(callback));
}

void InterestGroupManager::ClaimInterestGroupsForUpdate(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::ClaimInterestGroupsForUpdate)
      .WithArgs(owner)
      .Then(std::move(callback));
}

void InterestGroupManager::DeleteInterestGroupData(
    base::RepeatingCallback<bool(const url::Origin&)> origin_matcher) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(origin_matcher));
}

void InterestGroupManager::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  impl_.AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
}

void InterestGroupManager::DidUpdateInterestGroupsOfOwnerDbLoad(
    url::Origin owner,
    network::mojom::ClientSecurityStatePtr client_security_state,
    std::vector<StorageInterestGroup> interest_groups) {
  net::IsolationInfo per_update_isolation_info =
      net::IsolationInfo::CreateTransient();

  for (auto& interest_group : interest_groups) {
    if (!interest_group.bidding_group->group.update_url)
      continue;
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url =
        interest_group.bidding_group->group.update_url.value();
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->request_initiator = owner;
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        per_update_isolation_info;
    resource_request->trusted_params->client_security_state =
        client_security_state.Clone();
    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);
    simple_url_loader->SetTimeoutDuration(base::Seconds(30));
    auto simple_url_loader_it =
        url_loaders_.insert(url_loaders_.end(), std::move(simple_url_loader));
    (*simple_url_loader_it)
        ->DownloadToString(
            url_loader_factory_.get(),
            base::BindOnce(
                &InterestGroupManager::DidUpdateInterestGroupsOfOwnerNetFetch,
                weak_factory_.GetWeakPtr(), simple_url_loader_it, owner,
                interest_group.bidding_group->group.name),
            kMaxUpdateSize);
  }
}

void InterestGroupManager::DidUpdateInterestGroupsOfOwnerNetFetch(
    UrlLoadersList::iterator simple_url_loader_it,
    url::Origin owner,
    std::string name,
    std::unique_ptr<std::string> fetch_body) {
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(*simple_url_loader_it);
  url_loaders_.erase(simple_url_loader_it);
  // TODO(crbug.com/1186444): Report HTTP error info to devtools.
  if (!fetch_body) {
    ReportUpdateFetchFailed(
        owner, name, /*net_disconnected=*/simple_url_loader->NetError() ==
                         net::ERR_INTERNET_DISCONNECTED);
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *fetch_body,
      base::BindOnce(
          &InterestGroupManager::DidUpdateInterestGroupsOfOwnerJsonParse,
          weak_factory_.GetWeakPtr(), std::move(owner), std::move(name)));
}

namespace {

// TODO(crbug.com/1186444): Report errors to devtools for the TryToCopy*().
// functions.

// Name and owner are optional in `value` (parsed server JSON response), but
// must match `name` and `owner`, respectively, if either is specified. Returns
// true if the check passes, and false otherwise.
WARN_UNUSED_RESULT bool ValidateNameAndOwnerIfPresent(
    const url::Origin& owner,
    const std::string& name,
    const base::Value& value) {
  const std::string* maybe_owner = value.FindStringKey("owner");
  if (maybe_owner && url::Origin::Create(GURL(*maybe_owner)) != owner)
    return false;
  const std::string* maybe_name = value.FindStringKey("name");
  if (maybe_name && *maybe_name != name)
    return false;
  return true;
}

// Copies the trustedBiddingSignals list JSON field into
// `interest_group_update`, returns true iff the JSON is valid and the copy
// completed.
WARN_UNUSED_RESULT bool TryToCopyTrustedBiddingSignalsKeys(
    blink::InterestGroup& interest_group_update,
    const base::Value& value) {
  const base::Value* maybe_update_trusted_bidding_signals_keys =
      value.FindListKey("trustedBiddingSignalsKeys");
  if (!maybe_update_trusted_bidding_signals_keys)
    return true;
  std::vector<std::string> trusted_bidding_signals_keys;
  for (const base::Value& keys_value :
       maybe_update_trusted_bidding_signals_keys->GetList()) {
    const std::string* maybe_key = keys_value.GetIfString();
    if (!maybe_key)
      return false;
    trusted_bidding_signals_keys.push_back(*maybe_key);
  }
  interest_group_update.trusted_bidding_signals_keys =
      trusted_bidding_signals_keys;
  return true;
}

// Copies the `ads` list  JSON field into `interest_group_update`, returns true
// iff the JSON is valid and the copy completed.
WARN_UNUSED_RESULT bool TryToCopyAds(
    blink::InterestGroup& interest_group_update,
    const base::Value& value) {
  const base::Value* maybe_ads = value.FindListKey("ads");
  if (!maybe_ads)
    return true;
  std::vector<blink::InterestGroup::Ad> ads;
  for (const base::Value& ads_value : maybe_ads->GetList()) {
    if (!ads_value.is_dict())
      return false;
    const std::string* maybe_render_url = ads_value.FindStringKey("renderUrl");
    if (!maybe_render_url)
      return false;
    blink::InterestGroup::Ad ad;
    ad.render_url = GURL(*maybe_render_url);
    const base::Value* maybe_metadata = ads_value.FindKey("metadata");
    if (maybe_metadata) {
      std::string metadata;
      JSONStringValueSerializer serializer(&metadata);
      if (!serializer.Serialize(*maybe_metadata)) {
        // Binary blobs shouldn't be present, but it's possible we exceeded the
        // max JSON depth.
        return false;
      }
      ad.metadata = std::move(metadata);
    }
    ads.push_back(std::move(ad));
  }
  interest_group_update.ads = std::move(ads);
  return true;
}

absl::optional<blink::InterestGroup> ParseUpdateJson(
    const url::Origin& owner,
    const std::string& name,
    const data_decoder::DataDecoder::ValueOrError& result) {
  // TODO(crbug.com/1186444): Report to devtools.
  if (result.error) {
    return absl::nullopt;
  }
  const base::Value& value = *result.value;
  if (!value.is_dict()) {
    return absl::nullopt;
  }
  if (!ValidateNameAndOwnerIfPresent(owner, name, value)) {
    return absl::nullopt;
  }
  blink::InterestGroup interest_group_update;
  interest_group_update.owner = owner;
  interest_group_update.name = name;
  const std::string* maybe_bidding_url = value.FindStringKey("biddingLogicUrl");
  if (maybe_bidding_url)
    interest_group_update.bidding_url = GURL(*maybe_bidding_url);
  const std::string* maybe_update_trusted_bidding_signals_url =
      value.FindStringKey("trustedBiddingSignalsUrl");
  if (maybe_update_trusted_bidding_signals_url) {
    interest_group_update.trusted_bidding_signals_url =
        GURL(*maybe_update_trusted_bidding_signals_url);
  }
  if (!TryToCopyTrustedBiddingSignalsKeys(interest_group_update, value)) {
    return absl::nullopt;
  }
  if (!TryToCopyAds(interest_group_update, value)) {
    return absl::nullopt;
  }
  if (!interest_group_update.IsValid()) {
    return absl::nullopt;
  }
  return interest_group_update;
}

}  // namespace

void InterestGroupManager::DidUpdateInterestGroupsOfOwnerJsonParse(
    url::Origin owner,
    std::string name,
    data_decoder::DataDecoder::ValueOrError result) {
  absl::optional<blink::InterestGroup> interest_group_update =
      ParseUpdateJson(owner, name, result);
  if (!interest_group_update)
    return;
  UpdateInterestGroup(std::move(*interest_group_update));
}

void InterestGroupManager::ReportUpdateFetchFailed(const url::Origin& owner,
                                                   const std::string& name,
                                                   bool net_disconnected) {
  impl_.AsyncCall(&InterestGroupStorage::ReportUpdateFetchFailed)
      .WithArgs(owner, name, net_disconnected);
}

}  // namespace content
