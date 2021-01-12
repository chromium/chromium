// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include "base/check_op.h"

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
// 0-argument constructor. Additionally, |opaque_and_non_transient| can only be
// true if both origins are opaque and |site_for_cookies| is null.
bool IsConsistent(IsolationInfo::RequestType request_type,
                  const base::Optional<url::Origin>& top_frame_origin,
                  const base::Optional<url::Origin>& frame_origin,
                  const SiteForCookies& site_for_cookies,
                  bool opaque_and_non_transient,
                  base::Optional<std::set<SchemefulSite>> party_context) {
  // Check for the default-constructed case.
  if (!top_frame_origin) {
    return request_type == IsolationInfo::RequestType::kOther &&
           !frame_origin && site_for_cookies.IsNull() &&
           !opaque_and_non_transient && !party_context;
  }

  // |frame_origin| may only be nullopt is |top_frame_origin| is as well.
  if (!frame_origin)
    return false;

  // As long as there is a |top_frame_origin|, |site_for_cookies| must be
  // consistent with the |top_frame_origin|.
  if (!ValidateSameSite(*top_frame_origin, site_for_cookies))
    return false;

  if (opaque_and_non_transient) {
    return (request_type == IsolationInfo::RequestType::kOther &&
            top_frame_origin->opaque() && top_frame_origin == frame_origin &&
            site_for_cookies.IsNull());
  }

  switch (request_type) {
    case IsolationInfo::RequestType::kMainFrame:
      // TODO(https://crbug.com/1056706): Check that |top_frame_origin| and
      // |frame_origin| are the same, once the ViewSource code creates a
      // consistent IsolationInfo object.
      //
      // TODO(https://crbug.com/1060631): Once CreatePartial() is removed, check
      // if SiteForCookies is non-null if the scheme is HTTP or HTTPS.
      //
      // TODO(https://crbug.com/1151947): Once CreatePartial() is removed, check
      // if party_context is non-null and empty.
      return true;
    case IsolationInfo::RequestType::kSubFrame:
      // For subframe navigations, the subframe's origin may not be consistent
      // with the SiteForCookies, so SameSite cookies may be sent if there's a
      // redirect to main frames site.
      return true;
    case IsolationInfo::RequestType::kOther:
      // SiteForCookies must consistent with the frame origin as well for
      // subresources.
      return ValidateSameSite(*frame_origin, site_for_cookies);
  }
}

}  // namespace

IsolationInfo::IsolationInfo()
    : IsolationInfo(RequestType::kOther,
                    base::nullopt,
                    base::nullopt,
                    SiteForCookies(),
                    false /* opaque_and_non_transient */,
                    base::nullopt) {}

IsolationInfo::IsolationInfo(const IsolationInfo&) = default;
IsolationInfo::IsolationInfo(IsolationInfo&&) = default;
IsolationInfo::~IsolationInfo() = default;
IsolationInfo& IsolationInfo::operator=(const IsolationInfo&) = default;
IsolationInfo& IsolationInfo::operator=(IsolationInfo&&) = default;

IsolationInfo IsolationInfo::CreateForInternalRequest(
    const url::Origin& top_frame_origin) {
  return IsolationInfo(RequestType::kOther, top_frame_origin, top_frame_origin,
                       SiteForCookies::FromOrigin(top_frame_origin),
                       false /* opaque_and_non_transient */,
                       std::set<SchemefulSite>() /* party_context */);
}

IsolationInfo IsolationInfo::CreateTransient() {
  url::Origin opaque_origin;
  return IsolationInfo(RequestType::kOther, opaque_origin, opaque_origin,
                       SiteForCookies(), false /* opaque_and_non_transient */,
                       base::nullopt /* party_context */);
}

IsolationInfo IsolationInfo::CreateOpaqueAndNonTransient() {
  url::Origin opaque_origin;
  return IsolationInfo(RequestType::kOther, opaque_origin, opaque_origin,
                       SiteForCookies(), true /* opaque_and_non_transient */,
                       base::nullopt /* party_context */);
}

IsolationInfo IsolationInfo::Create(
    RequestType request_type,
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    const SiteForCookies& site_for_cookies,
    base::Optional<std::set<SchemefulSite>> party_context) {
  return IsolationInfo(request_type, top_frame_origin, frame_origin,
                       site_for_cookies, false /* opaque_and_non_transient */,
                       std::move(party_context));
}

IsolationInfo IsolationInfo::CreatePartial(
    RequestType request_type,
    const net::NetworkIsolationKey& network_isolation_key) {
  if (!network_isolation_key.IsFullyPopulated())
    return IsolationInfo();

  // TODO(https://crbug.com/1148927): Use null origins in this case.
  url::Origin top_frame_origin =
      network_isolation_key.GetTopFrameSite()->site_as_origin_;
  url::Origin frame_origin;
  if (network_isolation_key.GetFrameSite().has_value()) {
    frame_origin = network_isolation_key.GetFrameSite()->site_as_origin_;
  } else if (request_type == RequestType::kMainFrame) {
    frame_origin = top_frame_origin;
  } else {
    frame_origin = url::Origin();
  }

  bool opaque_and_non_transient = top_frame_origin.opaque() &&
                                  frame_origin.opaque() &&
                                  !network_isolation_key.IsTransient();

  return IsolationInfo(request_type, top_frame_origin, frame_origin,
                       SiteForCookies(), opaque_and_non_transient,
                       base::nullopt /* party_context */);
}

base::Optional<IsolationInfo> IsolationInfo::CreateIfConsistent(
    RequestType request_type,
    const base::Optional<url::Origin>& top_frame_origin,
    const base::Optional<url::Origin>& frame_origin,
    const SiteForCookies& site_for_cookies,
    bool opaque_and_non_transient,
    base::Optional<std::set<SchemefulSite>> party_context) {
  if (!IsConsistent(request_type, top_frame_origin, frame_origin,
                    site_for_cookies, opaque_and_non_transient,
                    party_context)) {
    return base::nullopt;
  }
  return IsolationInfo(request_type, top_frame_origin, frame_origin,
                       site_for_cookies, opaque_and_non_transient,
                       std::move(party_context));
}

IsolationInfo IsolationInfo::CreateForRedirect(
    const url::Origin& new_origin) const {
  if (request_type_ == RequestType::kOther)
    return *this;

  if (request_type_ == RequestType::kSubFrame) {
    return IsolationInfo(request_type_, top_frame_origin_, new_origin,
                         site_for_cookies_, opaque_and_non_transient_,
                         party_context_);
  }

  DCHECK_EQ(RequestType::kMainFrame, request_type_);
  DCHECK(!party_context_ || party_context_->empty());
  return IsolationInfo(request_type_, new_origin, new_origin,
                       SiteForCookies::FromOrigin(new_origin),
                       opaque_and_non_transient_, party_context_);
}

bool IsolationInfo::IsEqualForTesting(const IsolationInfo& other) const {
  return (request_type_ == other.request_type_ &&
          top_frame_origin_ == other.top_frame_origin_ &&
          frame_origin_ == other.frame_origin_ &&
          network_isolation_key_ == other.network_isolation_key_ &&
          opaque_and_non_transient_ == other.opaque_and_non_transient_ &&
          site_for_cookies_.IsEquivalent(other.site_for_cookies_) &&
          party_context_ == other.party_context_);
}

IsolationInfo IsolationInfo::ToDoUseTopFrameOriginAsWell(
    const url::Origin& incorrectly_used_frame_origin) {
  return IsolationInfo(
      RequestType::kOther, incorrectly_used_frame_origin,
      incorrectly_used_frame_origin,
      SiteForCookies::FromOrigin(incorrectly_used_frame_origin),
      false /* opaque_and_non_transient */,
      std::set<SchemefulSite>() /* party_context */);
}

IsolationInfo::IsolationInfo(
    RequestType request_type,
    const base::Optional<url::Origin>& top_frame_origin,
    const base::Optional<url::Origin>& frame_origin,
    const SiteForCookies& site_for_cookies,
    bool opaque_and_non_transient,
    base::Optional<std::set<SchemefulSite>> party_context)
    : request_type_(request_type),
      top_frame_origin_(top_frame_origin),
      frame_origin_(frame_origin),
      network_isolation_key_(
          !top_frame_origin
              ? NetworkIsolationKey()
              : NetworkIsolationKey(SchemefulSite(*top_frame_origin),
                                    SchemefulSite(*frame_origin),
                                    opaque_and_non_transient)),
      site_for_cookies_(site_for_cookies),
      opaque_and_non_transient_(opaque_and_non_transient),
      party_context_(party_context.has_value() &&
                             party_context->size() > kPartyContextMaxSize
                         ? base::nullopt
                         : party_context) {
  DCHECK(IsConsistent(request_type_, top_frame_origin_, frame_origin_,
                      site_for_cookies_, opaque_and_non_transient_,
                      party_context_));
}

}  // namespace net
