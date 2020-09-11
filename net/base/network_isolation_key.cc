// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {

namespace {

std::string GetOriginDebugString(const base::Optional<url::Origin>& origin) {
  return origin ? origin->GetDebugString() : "null";
}

// If |origin| has a value and represents an HTTP or HTTPS scheme, return a new
// origin with its registerable domain if possible, using the standard port for
// its scheme. Otherwise, return the passed in origin. WS and WSS origins are
// not modified, as they shouldn't be used meaningfully for NIKs, though trying
// to navigate to a WS URL may generate such a NIK.
base::Optional<url::Origin> SwitchToRegistrableDomainAndRemovePort(
    const base::Optional<url::Origin>& origin) {
  if (!origin.has_value())
    return origin;

  if (origin->scheme() != url::kHttpsScheme &&
      origin->scheme() != url::kHttpScheme) {
    return origin;
  }

  // scheme() returns the empty string for opaque origins.
  DCHECK(!origin->opaque());

  std::string registrable_domain = GetDomainAndRegistry(
      origin.value(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // GetDomainAndRegistry() returns an empty string for IP literals and
  // effective TLDs.
  if (registrable_domain.empty())
    registrable_domain = origin->host();
  return url::Origin::CreateFromNormalizedTuple(
      origin->scheme(), registrable_domain,
      url::DefaultPortForScheme(origin->scheme().c_str(),
                                origin->scheme().length()));
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(const url::Origin& top_frame_origin,
                                         const url::Origin& frame_origin)
    : NetworkIsolationKey(top_frame_origin,
                          frame_origin,
                          false /* opaque_and_non_transient */) {}

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

NetworkIsolationKey NetworkIsolationKey::CreateTransient() {
  url::Origin opaque_origin;
  return NetworkIsolationKey(opaque_origin, opaque_origin);
}

NetworkIsolationKey NetworkIsolationKey::CreateOpaqueAndNonTransient() {
  url::Origin opaque_origin;
  return NetworkIsolationKey(opaque_origin, opaque_origin,
                             true /* opaque_and_non_transient */);
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameOrigin(
    const url::Origin& new_frame_origin) const {
  if (!top_frame_origin_)
    return NetworkIsolationKey();
  NetworkIsolationKey key(top_frame_origin_.value(), new_frame_origin);
  key.opaque_and_non_transient_ = opaque_and_non_transient_;
  return key;
}

std::string NetworkIsolationKey::ToString() const {
  if (IsTransient())
    return "";

  if (IsOpaque()) {
    // This key is opaque but not transient.
    DCHECK(opaque_and_non_transient_);
    return "opaque non-transient " +
           top_frame_origin_->nonce_->token().ToString();
  }

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
  if (IsFullyPopulated() && IsOpaque() && opaque_and_non_transient_) {
    return_string += " non-transient";
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
  if (opaque_and_non_transient_) {
    DCHECK(IsOpaque());
    return false;
  }
  return IsOpaque();
}

bool NetworkIsolationKey::ToValue(base::Value* out_value) const {
  if (IsEmpty()) {
    *out_value = base::Value(base::Value::Type::LIST);
    return true;
  }

  if (IsTransient())
    return false;

  base::Optional<std::string> top_frame_value =
      top_frame_origin_->SerializeWithNonce();
  if (!top_frame_value)
    return false;
  *out_value = base::Value(base::Value::Type::LIST);
  out_value->Append(std::move(*top_frame_value));

  if (use_frame_origin_) {
    base::Optional<std::string> frame_value =
        frame_origin_->SerializeWithNonce();
    if (!frame_value)
      return false;
    out_value->Append(std::move(*frame_value));
  }

  return true;
}

bool NetworkIsolationKey::FromValue(
    const base::Value& value,
    NetworkIsolationKey* network_isolation_key) {
  if (!value.is_list())
    return false;

  base::Value::ConstListView list = value.GetList();
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

  if (!list[0].is_string())
    return false;
  base::Optional<url::Origin> deserialized_top_frame =
      url::Origin::Deserialize(list[0].GetString());
  if (!deserialized_top_frame)
    return false;
  url::Origin top_frame_origin = *deserialized_top_frame;

  // An opaque origin key will only be serialized into a base::Value if
  // |opaque_and_non_transient_| is set. Therefore if either origin is opaque,
  // |opaque_and_non_transient_| must be true.
  bool opaque_and_non_transient = top_frame_origin.opaque();

  if (!use_frame_origin) {
    *network_isolation_key =
        NetworkIsolationKey(top_frame_origin, top_frame_origin);
    network_isolation_key->opaque_and_non_transient_ = opaque_and_non_transient;
    return true;
  }

  if (!list[1].is_string())
    return false;
  base::Optional<url::Origin> deserialized_frame =
      url::Origin::Deserialize(list[1].GetString());
  if (!deserialized_frame)
    return false;
  url::Origin frame_origin = *deserialized_frame;

  opaque_and_non_transient |= frame_origin.opaque();

  *network_isolation_key = NetworkIsolationKey(top_frame_origin, frame_origin);
  network_isolation_key->opaque_and_non_transient_ = opaque_and_non_transient;
  return true;
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_origin_.has_value() && !frame_origin_.has_value();
}

NetworkIsolationKey::NetworkIsolationKey(const url::Origin& top_frame_origin,
                                         const url::Origin& frame_origin,
                                         bool opaque_and_non_transient)
    : opaque_and_non_transient_(opaque_and_non_transient),
      use_frame_origin_(base::FeatureList::IsEnabled(
          net::features::kAppendFrameOriginToNetworkIsolationKey)),
      top_frame_origin_(
          SwitchToRegistrableDomainAndRemovePort(top_frame_origin)),
      original_top_frame_origin_(top_frame_origin) {
  DCHECK(!opaque_and_non_transient || top_frame_origin.opaque());
  if (use_frame_origin_) {
    DCHECK(!opaque_and_non_transient || frame_origin.opaque());
    frame_origin_ = SwitchToRegistrableDomainAndRemovePort(frame_origin);
    original_frame_origin_ = frame_origin;
  }
}

bool NetworkIsolationKey::IsOpaque() const {
  return top_frame_origin_->opaque() ||
         (use_frame_origin_ && frame_origin_->opaque());
}

}  // namespace net
