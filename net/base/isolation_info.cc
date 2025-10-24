// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include <cstddef>
#include <optional>

#include "base/check_op.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/isolation_info.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace net {

namespace {

// Checks that |origin| is consistent with |site_for_cookies|.
bool ValidateSameSite(const url::Origin& origin,
                      const SiteForCookies& site_for_cookies) {
  // If not sending SameSite cookies, or sending them for a non-scheme, consider
  // all origins consistent. Note that SiteForCookies should never be created
  // for websocket schemes for valid navigations, since frames can't be
  // navigated to those schemes.
  if (site_for_cookies.IsNull() ||
      (site_for_cookies.scheme() != url::kHttpScheme &&
       site_for_cookies.scheme() != url::kHttpsScheme)) {
    return true;
  }

  // Shouldn't send cookies for opaque origins.
  if (origin.opaque())
    return false;

  // TODO(crbug.com/40122112): GetURL() is expensive. Maybe make a
  // version of IsFirstParty that works on origins?
  return site_for_cookies.IsFirstParty(origin.GetURL());
}

// Checks if these values are consistent. See IsolationInfo::Create() for
// descriptions of consistent sets of values. Also allows values used by the
// 0-argument constructor.
bool IsConsistent(IsolationInfo::RequestType request_type,
                  const std::optional<url::Origin>& top_frame_origin,
                  const std::optional<url::Origin>& frame_origin,
                  const SiteForCookies& site_for_cookies,
                  const std::optional<base::UnguessableToken>& nonce,
                  std::optional<IsolationInfo::FrameAncestorRelation>
                      frame_ancestor_relation) {
  // Check for the default-constructed case.
  if (!top_frame_origin) {
    return request_type == IsolationInfo::RequestType::kOther &&
           !frame_origin && !nonce && site_for_cookies.IsNull();
  }

  // As long as there is a |top_frame_origin|, |site_for_cookies| must be
  // consistent with the |top_frame_origin|.
  if (!ValidateSameSite(*top_frame_origin, site_for_cookies))
    return false;

  // Validate frame `frame_origin`
  // IsolationInfo must have a `frame_origin` when frame origins are enabled
  // and the IsolationInfo is not default-constructed.
  if (!frame_origin) {
    return false;
  }
  switch (request_type) {
    case IsolationInfo::RequestType::kMainFrame:
      if (frame_ancestor_relation !=
          IsolationInfo::FrameAncestorRelation::kSameOrigin) {
        return false;
      }

      // TODO(crbug.com/40677006): Check that |top_frame_origin| and
      // |frame_origin| are the same, once the ViewSource code creates a
      // consistent IsolationInfo object.
      //
      // TODO(crbug.com/40122112): Once CreatePartial() is removed,
      // check if SiteForCookies is non-null if the scheme is HTTP or HTTPS.
      break;
    case IsolationInfo::RequestType::kSubFrame:
      // For subframe navigations, the subframe's origin may not be consistent
      // with the SiteForCookies, so SameSite cookies may be sent if there's a
      // redirect to main frames site.
      break;
    case IsolationInfo::RequestType::kOther:
      // Check whether `frame_ancestor_relation` is consistent with
      // `frame_origin` and `top_frame_origin`. This is only relevant for
      // subresource requests.
      if (frame_ancestor_relation &&
          frame_ancestor_relation !=
              IsolationInfo::FrameAncestorRelation::kCrossSite &&
          frame_ancestor_relation <
              IsolationInfo::OriginRelationToFrameAncestorRelation(
                  GetOriginRelation(top_frame_origin.value(),
                                    frame_origin.value()))) {
        return false;
      }
      // SiteForCookies must consistent with the frame origin as well for
      // subresources.
      return ValidateSameSite(*frame_origin, site_for_cookies);
  }
  return true;
}

proto::FrameAncestorRelation SerializeFrameAncestorRelation(
    IsolationInfo::FrameAncestorRelation frame_ancestor_relation) {
  switch (frame_ancestor_relation) {
    case IsolationInfo::FrameAncestorRelation::kSameOrigin:
      return proto::FrameAncestorRelation::SAME_ORIGIN;
    case IsolationInfo::FrameAncestorRelation::kSameSite:
      return proto::FrameAncestorRelation::SAME_SITE;
    case IsolationInfo::FrameAncestorRelation::kCrossSite:
      return proto::FrameAncestorRelation::CROSS_SITE;
  }
}

std::optional<IsolationInfo::FrameAncestorRelation>
DeserializeFrameAncestorRelation(
    proto::IsolationInfo serialized_isolation_info) {
  if (!serialized_isolation_info.has_frame_ancestor_relation()) {
    return std::nullopt;
  }

  switch (serialized_isolation_info.frame_ancestor_relation()) {
    case proto::FrameAncestorRelation::SAME_ORIGIN:
      return IsolationInfo::FrameAncestorRelation::kSameOrigin;
    case proto::FrameAncestorRelation::SAME_SITE:
      return IsolationInfo::FrameAncestorRelation::kSameSite;
    case proto::FrameAncestorRelation::CROSS_SITE:
      return IsolationInfo::FrameAncestorRelation::kCrossSite;
  }
}

}  // namespace

IsolationInfo::IsolationInfo()
    : IsolationInfo(RequestType::kOther,
                    /*top_frame_origin=*/std::nullopt,
                    /*frame_origin=*/std::nullopt,
                    SiteForCookies(),
                    /*nonce=*/std::nullopt,
                    NetworkIsolationPartition::kGeneral,
                    /*frame_ancestor_relation=*/std::nullopt) {}

IsolationInfo::IsolationInfo(const IsolationInfo&) = default;
IsolationInfo::IsolationInfo(IsolationInfo&&) = default;
IsolationInfo::~IsolationInfo() = default;
IsolationInfo& IsolationInfo::operator=(const IsolationInfo&) = default;
IsolationInfo& IsolationInfo::operator=(IsolationInfo&&) = default;

IsolationInfo IsolationInfo::CreateForInternalRequest(
    const url::Origin& top_frame_origin) {
  return IsolationInfo(RequestType::kOther, top_frame_origin, top_frame_origin,
                       SiteForCookies::FromOrigin(top_frame_origin),
                       /*nonce=*/std::nullopt,
                       NetworkIsolationPartition::kGeneral,
                       /*frame_ancestor_relation=*/std::nullopt);
}

IsolationInfo IsolationInfo::CreateTransient(
    std::optional<base::UnguessableToken> nonce) {
  url::Origin opaque_origin;
  return IsolationInfo(RequestType::kOther, opaque_origin, opaque_origin,
                       SiteForCookies(), /*nonce=*/std::move(nonce),
                       NetworkIsolationPartition::kGeneral,
                       /*frame_ancestor_relation=*/std::nullopt);
}

std::optional<IsolationInfo> IsolationInfo::Deserialize(
    const std::string& serialized) {
  proto::IsolationInfo proto;
  if (!proto.ParseFromString(serialized))
    return std::nullopt;

  std::optional<url::Origin> top_frame_origin;
  if (proto.has_top_frame_origin())
    top_frame_origin = url::Origin::Create(GURL(proto.top_frame_origin()));

  std::optional<url::Origin> frame_origin;
  if (proto.has_frame_origin())
    frame_origin = url::Origin::Create(GURL(proto.frame_origin()));

  NetworkIsolationPartition network_isolation_partition =
      NetworkIsolationPartition::kGeneral;
  if (proto.has_network_isolation_partition()) {
    if (proto.network_isolation_partition() >
            static_cast<int32_t>(NetworkIsolationPartition::kMaxValue) ||
        proto.network_isolation_partition() < 0) {
      return std::nullopt;
    }
    network_isolation_partition = static_cast<NetworkIsolationPartition>(
        proto.network_isolation_partition());
  }

  if (proto.request_type() < 0 ||
      proto.request_type() > static_cast<int>(RequestType::kMax)) {
    return std::nullopt;
  }

  return IsolationInfo::CreateIfConsistent(
      static_cast<RequestType>(proto.request_type()),
      std::move(top_frame_origin), std::move(frame_origin),
      SiteForCookies::FromUrl(GURL(proto.site_for_cookies())),
      /*nonce=*/std::nullopt, network_isolation_partition,
      DeserializeFrameAncestorRelation(proto));
}

IsolationInfo IsolationInfo::Create(
    RequestType request_type,
    url::Origin top_frame_origin,
    url::Origin frame_origin,
    SiteForCookies site_for_cookies,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition,
    std::optional<FrameAncestorRelation> frame_ancestor_relation) {
  // TODO(crbug.com/420876079): Remove this "recovery" and force all callers to
  // supply a `frame_ancestor_relation` that is consistent (e.g. kSameSite if
  // their `request_type` is kMainFrame).
  if (request_type == RequestType::kMainFrame && !frame_ancestor_relation) {
    frame_ancestor_relation = FrameAncestorRelation::kSameOrigin;
  }
  return IsolationInfo(request_type, std::move(top_frame_origin),
                       std::move(frame_origin), std::move(site_for_cookies),
                       std::move(nonce), network_isolation_partition,
                       frame_ancestor_relation);
}

IsolationInfo IsolationInfo::DoNotUseCreatePartialFromNak(
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  if (!network_anonymization_key.IsFullyPopulated()) {
    return IsolationInfo();
  }

  url::Origin top_frame_origin =
      network_anonymization_key.GetTopFrameSite()->site_as_origin_;

  std::optional<url::Origin> frame_origin;
  if (network_anonymization_key.IsCrossSite()) {
    // If we know that the origin is cross site to the top level site, create an
    // empty origin to use as the frame origin for the isolation info. This
    // should be cross site with the top level origin.
    frame_origin = url::Origin();
  } else {
    // If we don't know that it's cross site to the top level site, use the top
    // frame site to set the frame origin.
    frame_origin = top_frame_origin;
  }

  const std::optional<base::UnguessableToken>& nonce =
      network_anonymization_key.GetNonce();

  auto isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, top_frame_origin,
      frame_origin.value(), SiteForCookies(), nonce);
  // TODO(crbug.com/40852603): DCHECK isolation info is fully populated.
  return isolation_info;
}

std::optional<IsolationInfo> IsolationInfo::CreateIfConsistent(
    RequestType request_type,
    std::optional<url::Origin> top_frame_origin,
    std::optional<url::Origin> frame_origin,
    SiteForCookies site_for_cookies,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition,
    std::optional<FrameAncestorRelation> frame_ancestor_relation) {
  // TODO(crbug.com/420876079): Remove this "recovery" and force all callers to
  // supply a `frame_ancestor_relation` that is consistent (e.g. kSameSite if
  // their `request_type` is kMainFrame).
  if (request_type == RequestType::kMainFrame && !frame_ancestor_relation) {
    frame_ancestor_relation = FrameAncestorRelation::kSameOrigin;
  }
  if (!IsConsistent(request_type, top_frame_origin, frame_origin,
                    site_for_cookies, nonce, frame_ancestor_relation)) {
    return std::nullopt;
  }
  return IsolationInfo(request_type, std::move(top_frame_origin),
                       std::move(frame_origin), std::move(site_for_cookies),
                       std::move(nonce), network_isolation_partition,
                       frame_ancestor_relation);
}

IsolationInfo IsolationInfo::CreateForRedirect(
    const url::Origin& new_origin) const {
  if (request_type_ == RequestType::kOther)
    return *this;

  if (request_type_ == RequestType::kSubFrame) {
    return IsolationInfo(
        request_type_, top_frame_origin_, new_origin, site_for_cookies_, nonce_,
        GetNetworkIsolationPartition(), frame_ancestor_relation_);
  }

  DCHECK_EQ(RequestType::kMainFrame, request_type_);
  DCHECK_EQ(frame_ancestor_relation_.value(),
            FrameAncestorRelation::kSameOrigin);

  return IsolationInfo(request_type_, new_origin, new_origin,
                       SiteForCookies::FromOrigin(new_origin), nonce_,
                       GetNetworkIsolationPartition(),
                       FrameAncestorRelation::kSameOrigin);
}

const std::optional<url::Origin>& IsolationInfo::frame_origin() const {
  return frame_origin_;
}

bool IsolationInfo::IsEqualForTesting(const IsolationInfo& other) const {
  return (request_type_ == other.request_type_ &&
          top_frame_origin_ == other.top_frame_origin_ &&
          frame_origin_ == other.frame_origin_ &&
          network_isolation_key_ == other.network_isolation_key_ &&
          network_anonymization_key_ == other.network_anonymization_key_ &&
          nonce_ == other.nonce_ &&
          site_for_cookies_.IsEquivalent(other.site_for_cookies_) &&
          frame_ancestor_relation_ == other.frame_ancestor_relation_);
}

std::string IsolationInfo::Serialize() const {
  if (network_isolation_key().IsTransient())
    return "";

  proto::IsolationInfo info;

  info.set_request_type(static_cast<int32_t>(request_type_));

  if (top_frame_origin_)
    info.set_top_frame_origin(top_frame_origin_->Serialize());

  if (frame_origin_)
    info.set_frame_origin(frame_origin_->Serialize());

  // The NetworkIsolationPartition defaults to kGeneral if not present in
  // the protobuf.
  if (GetNetworkIsolationPartition() != NetworkIsolationPartition::kGeneral) {
    info.set_network_isolation_partition(
        static_cast<int32_t>(GetNetworkIsolationPartition()));
  }

  info.set_site_for_cookies(site_for_cookies_.RepresentativeUrl().spec());

  if (frame_ancestor_relation_) {
    info.set_frame_ancestor_relation(
        SerializeFrameAncestorRelation(frame_ancestor_relation_.value()));
  }

  return info.SerializeAsString();
}

std::string IsolationInfo::DebugString() const {
  std::string s;
  s += "request_type: ";
  switch (request_type_) {
    case IsolationInfo::RequestType::kMainFrame:
      s += "kMainFrame";
      break;
    case IsolationInfo::RequestType::kSubFrame:
      s += "kSubFrame";
      break;
    case IsolationInfo::RequestType::kOther:
      s += "kOther";
      break;
  }

  s += "; top_frame_origin: ";
  if (top_frame_origin_) {
    s += top_frame_origin_.value().GetDebugString(true);
  } else {
    s += "(none)";
  }

  s += "; frame_origin: ";
  if (frame_origin_) {
    s += frame_origin_.value().GetDebugString(true);
  } else {
    s += "(none)";
  }

  s += "; network_anonymization_key: ";
  s += network_anonymization_key_.ToDebugString();

  s += "; network_isolation_key: ";
  s += network_isolation_key_.ToDebugString();

  s += "; nonce: ";
  if (nonce_) {
    s += nonce_.value().ToString();
  } else {
    s += "(none)";
  }

  s += "; site_for_cookies: ";
  s += site_for_cookies_.ToDebugString();

  s += "; frame_ancestor_relation: ";
  if (frame_ancestor_relation_) {
    s += FrameAncestorRelationString(frame_ancestor_relation_.value());
  } else {
    s += "(none)";
  }

  return s;
}

std::optional<IsolationInfo::FrameAncestorRelation>
IsolationInfo::OriginRelationToFrameAncestorRelation(
    std::optional<OriginRelation> origin_relation_value) {
  if (!origin_relation_value) {
    return std::nullopt;
  }

  switch (origin_relation_value.value()) {
    case OriginRelation::kSameOrigin:
      return FrameAncestorRelation::kSameOrigin;
    case OriginRelation::kSameSite:
      return FrameAncestorRelation::kSameSite;
    case OriginRelation::kCrossSite:
      return FrameAncestorRelation::kCrossSite;
  }
}

std::optional<IsolationInfo::FrameAncestorRelation>
IsolationInfo::ComputeNewFrameAncestorRelation(
    std::optional<FrameAncestorRelation> cur_relation,
    const url::Origin& frame_origin,
    const url::Origin& top_frame_origin) {
  if (!cur_relation) {
    return std::nullopt;
  }

  // A value of `kCrossSite` cannot be superceded, so skip computation.
  if (cur_relation == FrameAncestorRelation::kCrossSite) {
    return FrameAncestorRelation::kCrossSite;
  }
  std::optional<FrameAncestorRelation> new_relation =
      OriginRelationToFrameAncestorRelation(
          GetOriginRelation(frame_origin, top_frame_origin));

  CHECK(new_relation);

  return std::max({cur_relation, new_relation});
}

std::string_view IsolationInfo::FrameAncestorRelationString(
    FrameAncestorRelation frame_ancestor_relation) {
  switch (frame_ancestor_relation) {
    case FrameAncestorRelation::kSameOrigin:
      return "same-origin";
    case FrameAncestorRelation::kSameSite:
      return "same-site";
    case FrameAncestorRelation::kCrossSite:
      return "cross-site";
  }
}

IsolationInfo::IsolationInfo(
    RequestType request_type,
    std::optional<url::Origin> top_frame_origin,
    std::optional<url::Origin> frame_origin,
    SiteForCookies site_for_cookies,
    std::optional<base::UnguessableToken> nonce,
    NetworkIsolationPartition network_isolation_partition,
    std::optional<FrameAncestorRelation> frame_ancestor_relation)
    : request_type_(request_type),
      top_frame_origin_(std::move(top_frame_origin)),
      frame_origin_(std::move(frame_origin)),
      frame_ancestor_relation_(frame_ancestor_relation),
      network_isolation_key_(
          !top_frame_origin_
              ? NetworkIsolationKey()
              : NetworkIsolationKey(SchemefulSite(*top_frame_origin_),
                                    SchemefulSite(*frame_origin_),
                                    nonce,
                                    network_isolation_partition)),
      network_anonymization_key_(
          !top_frame_origin_ ? NetworkAnonymizationKey()
                             : NetworkAnonymizationKey::CreateFromFrameSite(
                                   SchemefulSite(*top_frame_origin_),
                                   SchemefulSite(*frame_origin_),
                                   nonce,
                                   network_isolation_partition)),
      site_for_cookies_(std::move(site_for_cookies)),
      nonce_(std::move(nonce)) {
  DCHECK(IsConsistent(request_type_, top_frame_origin_, frame_origin_,
                      site_for_cookies_, nonce_, frame_ancestor_relation_));
}

}  // namespace net
