// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include <cstddef>

#include "base/check_op.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/isolation_info.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // TODO(https://crbug.com/1060631): GetURL() is expensive. Maybe make a
  // version of IsFirstParty that works on origins?
  return site_for_cookies.IsFirstParty(origin.GetURL());
}

// Checks if these values are consistent. See IsolationInfo::Create() for
// descriptions of consistent sets of values. Also allows values used by the
// 0-argument constructor.
bool IsConsistent(IsolationInfo::RequestType request_type,
                  const absl::optional<url::Origin>& top_frame_origin,
                  const absl::optional<url::Origin>& frame_origin,
                  const SiteForCookies& site_for_cookies,
                  absl::optional<std::set<SchemefulSite>> party_context,
                  const base::UnguessableToken* nonce) {
  // Check for the default-constructed case.
  if (!top_frame_origin) {
    return request_type == IsolationInfo::RequestType::kOther &&
           !frame_origin && !nonce && site_for_cookies.IsNull() &&
           !party_context;
  }

  // As long as there is a |top_frame_origin|, |site_for_cookies| must be
  // consistent with the |top_frame_origin|.
  if (!ValidateSameSite(*top_frame_origin, site_for_cookies))
    return false;

  // Validate frame `frame_origin`
  if (IsolationInfo::IsFrameSiteEnabled()) {
    // IsolationInfo must have a `frame_origin` when frame origins are enabled
    // and the IsolationInfo is not default-constructed.
    if (!frame_origin) {
      return false;
    }
    switch (request_type) {
      case IsolationInfo::RequestType::kMainFrame:
        // TODO(https://crbug.com/1056706): Check that |top_frame_origin| and
        // |frame_origin| are the same, once the ViewSource code creates a
        // consistent IsolationInfo object.
        //
        // TODO(https://crbug.com/1060631): Once CreatePartial() is removed,
        // check if SiteForCookies is non-null if the scheme is HTTP or HTTPS.
        //
        // TODO(https://crbug.com/1151947): Once CreatePartial() is removed,
        // check if party_context is non-null and empty.
        break;
      case IsolationInfo::RequestType::kSubFrame:
        // For subframe navigations, the subframe's origin may not be consistent
        // with the SiteForCookies, so SameSite cookies may be sent if there's a
        // redirect to main frames site.
        break;
      case IsolationInfo::RequestType::kOther:
        // SiteForCookies must consistent with the frame origin as well for
        // subresources.
        return ValidateSameSite(*frame_origin, site_for_cookies);
    }
  }
  return true;
}

}  // namespace

IsolationInfo::IsolationInfo()
    : IsolationInfo(RequestType::kOther,
                    absl::nullopt,
                    absl::nullopt,
                    SiteForCookies(),
                    nullptr /* nonce */,
                    absl::nullopt) {}

IsolationInfo::IsolationInfo(const IsolationInfo&) = default;
IsolationInfo::IsolationInfo(IsolationInfo&&) = default;
IsolationInfo::~IsolationInfo() = default;
IsolationInfo& IsolationInfo::operator=(const IsolationInfo&) = default;
IsolationInfo& IsolationInfo::operator=(IsolationInfo&&) = default;

IsolationInfo IsolationInfo::CreateForInternalRequest(
    const url::Origin& top_frame_origin) {
  return IsolationInfo(RequestType::kOther, top_frame_origin, top_frame_origin,
                       SiteForCookies::FromOrigin(top_frame_origin),
                       nullptr /* nonce */,
                       std::set<SchemefulSite>() /* party_context */);
}

IsolationInfo IsolationInfo::CreateTransient() {
  url::Origin opaque_origin;
  return IsolationInfo(RequestType::kOther, opaque_origin, opaque_origin,
                       SiteForCookies(), nullptr /* nonce */,
                       absl::nullopt /* party_context */);
}

absl::optional<IsolationInfo> IsolationInfo::Deserialize(
    const std::string& serialized) {
  proto::IsolationInfo proto;
  if (!proto.ParseFromString(serialized))
    return absl::nullopt;

  absl::optional<url::Origin> top_frame_origin;
  if (proto.has_top_frame_origin())
    top_frame_origin = url::Origin::Create(GURL(proto.top_frame_origin()));

  absl::optional<url::Origin> frame_origin;
  if (proto.has_frame_origin())
    frame_origin = url::Origin::Create(GURL(proto.frame_origin()));

  absl::optional<std::set<SchemefulSite>> party_context;
  if (proto.has_party_context()) {
    party_context = std::set<SchemefulSite>();
    for (const auto& site : proto.party_context().site()) {
      party_context->insert(SchemefulSite::Deserialize(site));
    }
  }

  return IsolationInfo::CreateIfConsistent(
      static_cast<RequestType>(proto.request_type()),
      std::move(top_frame_origin), std::move(frame_origin),
      SiteForCookies::FromUrl(GURL(proto.site_for_cookies())),
      std::move(party_context), nullptr);
}

IsolationInfo IsolationInfo::Create(
    RequestType request_type,
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    const SiteForCookies& site_for_cookies,
    absl::optional<std::set<SchemefulSite>> party_context,
    const base::UnguessableToken* nonce) {
  return IsolationInfo(request_type, top_frame_origin, frame_origin,
                       site_for_cookies, nonce, std::move(party_context));
}

IsolationInfo IsolationInfo::DoNotUseCreatePartialFromNak(
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  if (!network_anonymization_key.IsFullyPopulated()) {
    return IsolationInfo();
  }

  url::Origin top_frame_origin =
      network_anonymization_key.GetTopFrameSite()->site_as_origin_;

  absl::optional<url::Origin> frame_origin;
  if (NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled() &&
      network_anonymization_key.GetIsCrossSite().value()) {
    // If we know that the origin is cross site to the top level site, create an
    // empty origin to use as the frame origin for the isolation info. This
    // should be cross site with the top level origin.
    frame_origin = url::Origin();
  } else {
    // If we don't know that it's cross site to the top level site, use the top
    // frame site to set the frame origin.
    frame_origin = top_frame_origin;
  }

  const base::UnguessableToken* nonce =
      network_anonymization_key.GetNonce()
          ? &network_anonymization_key.GetNonce().value()
          : nullptr;

  auto isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, top_frame_origin,
      frame_origin.value(), SiteForCookies(),
      /*party_context=*/absl::nullopt, nonce);
  // TODO(crbug/1343856): DCHECK isolation info is fully populated.
  return isolation_info;
}

absl::optional<IsolationInfo> IsolationInfo::CreateIfConsistent(
    RequestType request_type,
    const absl::optional<url::Origin>& top_frame_origin,
    const absl::optional<url::Origin>& frame_origin,
    const SiteForCookies& site_for_cookies,
    absl::optional<std::set<SchemefulSite>> party_context,
    const base::UnguessableToken* nonce) {
  if (!IsConsistent(request_type, top_frame_origin, frame_origin,
                    site_for_cookies, party_context, nonce)) {
    return absl::nullopt;
  }
  return IsolationInfo(request_type, top_frame_origin, frame_origin,
                       site_for_cookies, nonce, std::move(party_context));
}

IsolationInfo IsolationInfo::CreateForRedirect(
    const url::Origin& new_origin) const {
  if (request_type_ == RequestType::kOther)
    return *this;

  if (request_type_ == RequestType::kSubFrame) {
    return IsolationInfo(
        request_type_, top_frame_origin_, new_origin, site_for_cookies_,
        nonce_.has_value() ? &nonce_.value() : nullptr, party_context_);
  }

  DCHECK_EQ(RequestType::kMainFrame, request_type_);
  DCHECK(!party_context_ || party_context_->empty());
  return IsolationInfo(request_type_, new_origin, new_origin,
                       SiteForCookies::FromOrigin(new_origin),
                       nonce_.has_value() ? &nonce_.value() : nullptr,
                       party_context_);
}

const absl::optional<url::Origin>& IsolationInfo::frame_origin() const {
  // Frame origin will be empty if double-keying is enabled.
  CHECK(IsolationInfo::IsFrameSiteEnabled());
  return frame_origin_;
}

const absl::optional<url::Origin>& IsolationInfo::frame_origin_for_testing()
    const {
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
          party_context_ == other.party_context_);
}

IsolationInfo IsolationInfo::ToDoUseTopFrameOriginAsWell(
    const url::Origin& incorrectly_used_frame_origin) {
  return IsolationInfo(
      RequestType::kOther, incorrectly_used_frame_origin,
      incorrectly_used_frame_origin,
      SiteForCookies::FromOrigin(incorrectly_used_frame_origin),
      nullptr /* nonce */, std::set<SchemefulSite>() /* party_context */);
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

  info.set_site_for_cookies(site_for_cookies_.RepresentativeUrl().spec());

  if (party_context_) {
    auto* pc = info.mutable_party_context();
    for (const auto& site : *party_context_) {
      pc->add_site(site.Serialize());
    }
  }

  return info.SerializeAsString();
}

bool IsolationInfo::IsFrameSiteEnabled() {
  // NIKs, and thus IsolationInfo's, are currently always triple-keyed, but we
  // will experiment with 2.5-keying in crbug.com/1414808.
  return true;
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

  if (IsFrameSiteEnabled()) {
    s += "; frame_origin: ";
    if (frame_origin_) {
      s += frame_origin_.value().GetDebugString(true);
    } else {
      s += "(none)";
    }
  }

  s += "; network_anonymization_key: ";
  s += network_anonymization_key_.ToDebugString();

  s += "; network_isolation_key: ";
  s += network_isolation_key_.ToDebugString();

  s += "; party_context: ";
  if (party_context_) {
    s += "{";
    for (auto& site : party_context_.value()) {
      s += site.GetDebugString();
      s += ", ";
    }
    s += "}";
  } else {
    s += "(none)";
  }

  s += "; nonce: ";
  if (nonce_) {
    s += nonce_.value().ToString();
  } else {
    s += "(none)";
  }

  s += "; site_for_cookies: ";
  s += site_for_cookies_.ToDebugString();

  return s;
}

NetworkAnonymizationKey
IsolationInfo::CreateNetworkAnonymizationKeyForIsolationInfo(
    const absl::optional<url::Origin>& top_frame_origin,
    const absl::optional<url::Origin>& frame_origin,
    const base::UnguessableToken* nonce) const {
  if (!top_frame_origin) {
    return NetworkAnonymizationKey();
  }

  bool nak_is_cross_site;
  if (frame_origin) {
    SiteForCookies site_for_cookies =
        net::SiteForCookies::FromOrigin(top_frame_origin.value());
    nak_is_cross_site = !site_for_cookies.IsFirstParty(frame_origin->GetURL());
  } else {
    // If we are unable to determine if the frame is cross site we should create
    // it as cross site.
    nak_is_cross_site = true;
  }

  return NetworkAnonymizationKey(
      SchemefulSite(*top_frame_origin), absl::nullopt,
      absl::make_optional(nak_is_cross_site),
      nonce ? absl::make_optional(*nonce) : absl::nullopt);
}

IsolationInfo::IsolationInfo(
    RequestType request_type,
    const absl::optional<url::Origin>& top_frame_origin,
    const absl::optional<url::Origin>& frame_origin,
    const SiteForCookies& site_for_cookies,
    const base::UnguessableToken* nonce,
    absl::optional<std::set<SchemefulSite>> party_context)
    : request_type_(request_type),
      top_frame_origin_(top_frame_origin),
      frame_origin_(IsFrameSiteEnabled() ? frame_origin : absl::nullopt),
      network_isolation_key_(
          !top_frame_origin
              ? NetworkIsolationKey()
              : NetworkIsolationKey(SchemefulSite(*top_frame_origin),
                                    IsFrameSiteEnabled()
                                        ? SchemefulSite(*frame_origin)
                                        : SchemefulSite(),
                                    nonce)),
      network_anonymization_key_(
          CreateNetworkAnonymizationKeyForIsolationInfo(top_frame_origin,
                                                        frame_origin,
                                                        nonce)),
      site_for_cookies_(site_for_cookies),
      nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt),
      party_context_(party_context.has_value() &&
                             party_context->size() > kPartyContextMaxSize
                         ? absl::nullopt
                         : party_context) {
  DCHECK(IsConsistent(request_type_, top_frame_origin_, frame_origin_,
                      site_for_cookies_, party_context_, nonce));
}

}  // namespace net
