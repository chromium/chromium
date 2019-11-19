// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "net/http/http_util.h"
#include "services/network/network_context.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"

namespace {

// Marker for (temporarily) exempted origins. The presence of the "?" guarantees
// that this is not a valid policy as it is not a valid http token.
const char kExemptedOriginPolicyVersion[] = "exception?";

}  // namespace

namespace network {

OriginPolicyManager::OriginPolicyManager(NetworkContext* owner_network_context)
    : owner_network_context_(owner_network_context),
      url_loader_factory_(
          owner_network_context_->CreateUrlLoaderFactoryForNetworkService()) {}

OriginPolicyManager::~OriginPolicyManager() {}

void OriginPolicyManager::AddReceiver(
    mojo::PendingReceiver<mojom::OriginPolicyManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OriginPolicyManager::RetrieveOriginPolicy(
    const url::Origin& origin,
    const std::string& header_value,
    RetrieveOriginPolicyCallback callback) {
  DCHECK(origin.GetURL().is_valid());
  DCHECK(!origin.opaque());

  OriginPolicyHeaderValues header_info =
      GetRequestedPolicyAndReportGroupFromHeaderString(header_value);

  auto iter = latest_version_map_.find(origin);

  // Process policy deletion first!
  if (header_info.policy_version == kOriginPolicyDeletePolicy) {
    if (iter != latest_version_map_.end())
      latest_version_map_.erase(iter);
    InvokeCallbackWithPolicyState(origin, OriginPolicyState::kNoPolicyApplies,
                                  std::move(callback));
    return;
  }

  // Process policy exceptions.
  if (iter != latest_version_map_.end() &&
      iter->second == kExemptedOriginPolicyVersion) {
    InvokeCallbackWithPolicyState(origin, OriginPolicyState::kNoPolicyApplies,
                                  std::move(callback));
    return;
  }

  // No policy applies to this request or invalid header present.
  if (header_info.policy_version.empty()) {
    // If there header has no policy version is present, use cached version, if
    // there is one. Otherwise, fail.
    if (iter == latest_version_map_.end()) {
      InvokeCallbackWithPolicyState(origin,
                                    header_value.empty()
                                        ? OriginPolicyState::kNoPolicyApplies
                                        : OriginPolicyState::kCannotLoadPolicy,
                                    std::move(callback));
      MaybeReport(OriginPolicyState::kCannotLoadPolicy, header_info, GURL());
      return;
    }
    header_info.policy_version = iter->second;
  } else if (iter == latest_version_map_.end()) {
    latest_version_map_.emplace(origin, header_info.policy_version);
  } else {
    iter->second = header_info.policy_version;
  }

  origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
      this, header_info, origin, url_loader_factory_.get(),
      std::move(callback)));
}

void OriginPolicyManager::AddExceptionFor(const url::Origin& origin) {
  latest_version_map_[origin] = kExemptedOriginPolicyVersion;
}

void OriginPolicyManager::FetcherDone(OriginPolicyFetcher* fetcher,
                                      const OriginPolicy& origin_policy,
                                      RetrieveOriginPolicyCallback callback) {
  std::move(callback).Run(origin_policy);

  auto it = origin_policy_fetchers_.find(fetcher);
  DCHECK(it != origin_policy_fetchers_.end());
  origin_policy_fetchers_.erase(it);
}

void OriginPolicyManager::RetrieveDefaultOriginPolicy(
    const url::Origin& origin,
    RetrieveOriginPolicyCallback callback) {
  origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
      this, origin, url_loader_factory_.get(), std::move(callback)));
}

#if BUILDFLAG(ENABLE_REPORTING)
void OriginPolicyManager::MaybeReport(
    OriginPolicyState state,
    const OriginPolicyHeaderValues& header_info,
    const GURL& policy_url) {
  if (header_info.report_to.empty())
    return;

  const char* reason_str = nullptr;
  switch (state) {
    case OriginPolicyState::kCannotLoadPolicy:
      reason_str = "CANNOT_LOAD";
      break;
    case OriginPolicyState::kInvalidRedirect:
      reason_str = "REDIRECT";
      break;
    case OriginPolicyState::kOther:
      reason_str = "OTHER";
      break;
    default:
      NOTREACHED();
      return;
  }

  base::DictionaryValue report_body;
  report_body.SetKey("origin_policy_url", base::Value(policy_url.spec()));
  report_body.SetKey("policy", base::Value(header_info.raw_header));
  report_body.SetKey("policy_error_reason", base::Value(reason_str));

  owner_network_context_->QueueReport("origin-policy", header_info.report_to,
                                      policy_url, base::nullopt,
                                      std::move(report_body));
}
#else
void OriginPolicyManager::MaybeReport(
    OriginPolicyState state,
    const OriginPolicyHeaderValues& header_info,
    const GURL& policy_url) {}
#endif  // BUILDFLAG(ENABLE_REPORTING)

// static
const char* OriginPolicyManager::GetExemptedVersionForTesting() {
  return kExemptedOriginPolicyVersion;
}

// static
OriginPolicyHeaderValues
OriginPolicyManager::GetRequestedPolicyAndReportGroupFromHeaderString(
    const std::string& header_value) {
  if (net::HttpUtil::TrimLWS(header_value) == kOriginPolicyDeletePolicy)
    return OriginPolicyHeaderValues(
        {kOriginPolicyDeletePolicy, "", header_value});

  base::Optional<std::string> policy;
  base::Optional<std::string> report_to;
  bool valid = true;
  net::HttpUtil::NameValuePairsIterator iter(header_value.cbegin(),
                                             header_value.cend(), ',');
  while (iter.GetNext()) {
    std::string token_value = net::HttpUtil::TrimLWS(iter.value()).as_string();
    bool is_token = net::HttpUtil::IsToken(token_value);
    if (iter.name() == kOriginPolicyPolicy) {
      valid &= is_token && !policy.has_value();
      policy = token_value;
    } else if (iter.name() == kOriginPolicyReportTo) {
      valid &= is_token && !report_to.has_value();
      report_to = token_value;
    }
  }
  valid &= iter.valid();
  valid &= (policy.has_value() && policy->find('.') == std::string::npos);

  if (!valid)
    return OriginPolicyHeaderValues({"", "", header_value});
  return OriginPolicyHeaderValues(
      {policy.value(), report_to.value_or(""), header_value});
}

// static
void OriginPolicyManager::InvokeCallbackWithPolicyState(
    const url::Origin& origin,
    OriginPolicyState state,
    RetrieveOriginPolicyCallback callback) {
  OriginPolicy result;
  result.state = state;
  result.policy_url = OriginPolicyFetcher::GetDefaultPolicyURL(origin);
  std::move(callback).Run(result);
}

}  // namespace network
