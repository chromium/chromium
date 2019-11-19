// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

std::string GetOriginDebugString(const base::Optional<url::Origin>& origin) {
  return origin ? origin->GetDebugString() : "null";
}

url::Origin GetSchemeAndRegistrableDomain(const url::Origin& origin) {
  std::string registrable_domain = GetDomainAndRegistry(
      origin.host(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return url::Origin::CreateFromNormalizedTuple(
      origin.scheme(), registrable_domain,
      url::DefaultPortForScheme(origin.scheme().c_str(),
                                origin.scheme().length()));
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(const url::Origin& top_frame_origin,
                                         const url::Origin& frame_origin)
    : use_frame_origin_(base::FeatureList::IsEnabled(
          net::features::kAppendFrameOriginToNetworkIsolationKey)),
      top_frame_origin_(top_frame_origin) {
  if (use_frame_origin_) {
    frame_origin_ = frame_origin;
  }
  if (base::FeatureList::IsEnabled(
          net::features::kUseRegistrableDomainInNetworkIsolationKey)) {
    ReplaceOriginsWithRegistrableDomains();
  }
}

NetworkIsolationKey::NetworkIsolationKey()
    : use_frame_origin_(base::FeatureList::IsEnabled(
          net::features::kAppendFrameOriginToNetworkIsolationKey)) {}

NetworkIsolationKey::NetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey::~NetworkIsolationKey() = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    NetworkIsolationKey&& network_isolation_key) = default;

std::string NetworkIsolationKey::ToString() const {
  if (IsTransient())
    return "";

  return top_frame_origin_->Serialize() +
         (use_frame_origin_ ? " " + frame_origin_->Serialize() : "");
}

std::string NetworkIsolationKey::ToDebugString() const {
  // The space-separated serialization of |top_frame_origin_| and
  // |frame_origin_|.
  std::string return_string = GetOriginDebugString(top_frame_origin_);
  if (use_frame_origin_) {
    return_string += " " + GetOriginDebugString(frame_origin_);
  }
  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  return top_frame_origin_.has_value() &&
         (!use_frame_origin_ || frame_origin_.has_value());
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return top_frame_origin_->opaque() ||
         (use_frame_origin_ && frame_origin_->opaque());
}

bool NetworkIsolationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  *out_value = base::Value(base::Value::Type::LIST);
  // Store origins GURLs, since GURL has validation logic that can be used when
  // loading, while Origin only has DCHECKs.
  out_value->Append(base::Value(top_frame_origin_->GetURL().spec()));

  if (use_frame_origin_)
    out_value->Append(base::Value(frame_origin_->GetURL().spec()));
  return true;
}

bool NetworkIsolationKey::FromValue(
    const base::Value& value,
    NetworkIsolationKey* network_isolation_key) {
  if (value.type() != base::Value::Type::LIST)
    return false;

  base::span<const base::Value> list = value.GetList();
  if (list.empty()) {
    *network_isolation_key = NetworkIsolationKey();
    return true;
  }

  bool use_frame_origin = base::FeatureList::IsEnabled(
      net::features::kAppendFrameOriginToNetworkIsolationKey);
  if ((!use_frame_origin && list.size() != 1) ||
      (use_frame_origin && list.size() != 2)) {
    return false;
  }

  if (list[0].type() != base::Value::Type::STRING)
    return false;
  GURL top_frame_url(list[0].GetString());
  if (!top_frame_url.is_valid())
    return false;
  url::Origin top_frame_origin = url::Origin::Create(top_frame_url);
  if (!use_frame_origin) {
    NetworkIsolationKey result_value(top_frame_origin, top_frame_origin);
    if (result_value.IsTransient())
      return false;
    *network_isolation_key = std::move(result_value);
    return true;
  }

  if (list[1].type() != base::Value::Type::STRING)
    return false;
  GURL frame_url(list[1].GetString());
  if (!frame_url.is_valid())
    return false;
  url::Origin frame_origin = url::Origin::Create(frame_url);
  NetworkIsolationKey result_value(top_frame_origin, frame_origin);
  if (result_value.IsTransient())
    return false;
  *network_isolation_key = std::move(result_value);
  return true;
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_origin_.has_value() && !frame_origin_.has_value();
}

void NetworkIsolationKey::ReplaceOriginsWithRegistrableDomains() {
  if (!top_frame_origin_.has_value())
    return;

  if (!top_frame_origin_->opaque()) {
    top_frame_origin_ =
        GetSchemeAndRegistrableDomain(top_frame_origin_.value());
  }

  if (!frame_origin_.has_value() || frame_origin_->opaque())
    return;

  frame_origin_ = GetSchemeAndRegistrableDomain(frame_origin_.value());
}

}  // namespace net
