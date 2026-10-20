// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_deletion_info.h"

#include <string_view>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"

namespace net {

namespace {

// Returns the registrable domain (eTLD+1) of `domain`, or `domain` without a
// leading dot if it is not parsed as belonging to a registry (e.g. for IP
// addresses or internal hostnames).
std::string GetEffectiveDomainOrHost(std::string_view domain) {
  // GetDomainAndRegistry() is insensitive to leading dots, i.e. to host/domain
  // cookie distinctions. If `domain` is an IP address or internal hostname, an
  // empty string will be returned; in that case, fall back to `domain` without
  // any leading dot.
  std::string effective_domain(
      net::registry_controlled_domains::GetDomainAndRegistry(
          domain,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  if (effective_domain.empty()) {
    return cookie_util::CookieDomainAsHost(domain);
  }
  return effective_domain;
}

// Return true if the eTLD+1 of the cookies domain matches any of the strings
// in |match_domains|, false otherwise.
bool DomainMatchesDomains(const net::CanonicalCookie& cookie,
                          const std::set<std::string>& match_domains) {
  if (match_domains.empty())
    return false;

  return match_domains.count(GetEffectiveDomainOrHost(cookie.Domain())) != 0;
}

}  // anonymous namespace

CookieDeletionInfo::TimeRange::TimeRange() = default;

CookieDeletionInfo::TimeRange::TimeRange(const TimeRange& other) = default;

CookieDeletionInfo::TimeRange::TimeRange(base::Time start, base::Time end)
    : start_(start), end_(end) {
  if (!start.is_null() && !end.is_null())
    DCHECK_GE(end, start);
}

CookieDeletionInfo::TimeRange& CookieDeletionInfo::TimeRange::operator=(
    const TimeRange& rhs) = default;

bool CookieDeletionInfo::TimeRange::Contains(base::Time time) const {
  DCHECK(!time.is_null());

  if (!start_.is_null() && start_ == end_)
    return time == start_;
  return (start_.is_null() || start_ <= time) &&
         (end_.is_null() || time < end_);
}

void CookieDeletionInfo::TimeRange::SetStart(base::Time value) {
  start_ = value;
}

void CookieDeletionInfo::TimeRange::SetEnd(base::Time value) {
  end_ = value;
}

CookieDeletionInfo::CookieDeletionInfo()
    : CookieDeletionInfo(base::Time(), base::Time()) {}

CookieDeletionInfo::CookieDeletionInfo(base::Time start_time,
                                       base::Time end_time)
    : creation_range(start_time, end_time) {}

CookieDeletionInfo::CookieDeletionInfo(CookieDeletionInfo&& other) = default;

CookieDeletionInfo::CookieDeletionInfo(const CookieDeletionInfo& other) =
    default;

CookieDeletionInfo::~CookieDeletionInfo() = default;

CookieDeletionInfo& CookieDeletionInfo::operator=(CookieDeletionInfo&& rhs) =
    default;

CookieDeletionInfo& CookieDeletionInfo::operator=(
    const CookieDeletionInfo& rhs) = default;

bool CookieDeletionInfo::Matches(const CanonicalCookie& cookie,
                                 const CookieAccessParams& params) const {
  if (session_control != SessionControl::IGNORE_CONTROL &&
      (cookie.IsPersistent() !=
       (session_control == SessionControl::PERSISTENT_COOKIES))) {
    return false;
  }

  if (!creation_range.Contains(cookie.CreationDate()))
    return false;

  if (host.has_value() &&
      !(cookie.IsHostCookie() && cookie.IsDomainMatch(host.value()))) {
    return false;
  }

  if (name.has_value() && cookie.Name() != name)
    return false;

  if (value_for_testing.has_value() &&
      value_for_testing.value() != cookie.Value()) {
    return false;
  }

  // |CookieOptions::MakeAllInclusive()| options will make sure that all
  // cookies associated with the URL are deleted.
  if (url.has_value() &&
      !cookie
           .IncludeForRequestURL(url.value(), CookieOptions::MakeAllInclusive(),
                                 params)
           .status.IsInclude()) {
    return false;
  }

  if (domains_and_ips_to_delete.has_value() &&
      !DomainMatchesDomains(cookie, *domains_and_ips_to_delete)) {
    return false;
  }

  if (domains_and_ips_to_ignore.has_value() &&
      DomainMatchesDomains(cookie, *domains_and_ips_to_ignore)) {
    return false;
  }

  if (cookie.IsPartitioned() &&
      !cookie_partition_key_collection.Contains(*cookie.PartitionKey())) {
    return false;
  }

  if (partitioned_state_only && !cookie.IsPartitioned()) {
    return false;
  }

  return true;
}

std::optional<std::string> CookieDeletionInfo::SingleMatchingRegistrableDomain()
    const {
  // `CookieDeletionInfo::Matches` evaluates a conjunction (logical AND) of
  // all active filter fields. When at least one filter constrains matching
  // cookies to a single registrable domain (eTLD+1) or host bucket `key`,
  // every matching cookie is guaranteed to be loaded by
  // `LoadCookiesForKey(*key)`.
  if (host.has_value()) {
    // `host` only matches host cookies (`IsHostCookie()`) where
    // `IsDomainMatch(*host)` holds, which requires `cookie.Domain() == *host`.
    return GetEffectiveDomainOrHost(*host);
  }
  if (url.has_value() && url->is_valid()) {
    // `url` requires `IncludeForRequestURL(*url)`, which domain-matches
    // against `url->host()`, placing all matching host and domain cookies
    // under `GetEffectiveDomainOrHost(url->host())`.
    return GetEffectiveDomainOrHost(url->host());
  }
  if (domains_and_ips_to_delete.has_value() &&
      domains_and_ips_to_delete->size() == 1) {
    // `DomainMatchesDomains` computes the same registrable domain via
    // `GetEffectiveDomainOrHost`. Checking `candidate_key == candidate` ensures
    // non-canonical subdomain entries (which match no cookies in
    // `DomainMatchesDomains`) are not misidentified as single-domain filters.
    const std::string& candidate = *domains_and_ips_to_delete->begin();
    std::string candidate_key = GetEffectiveDomainOrHost(candidate);
    if (candidate_key == candidate) {
      return candidate_key;
    }
  }
  return std::nullopt;
}

}  // namespace net
