// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_deletion_info.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_options.h"

namespace net {

namespace {

// Return true if the eTLD+1 of the cookies domain matches any of the strings
// in |match_domains|, false otherwise.
bool DomainMatchesDomains(const net::CanonicalCookie& cookie,
                          const std::set<std::string>& match_domains) {
  if (match_domains.empty())
    return false;

  // If domain is an IP address it returns an empty string.
  std::string effective_domain(
      net::registry_controlled_domains::GetDomainAndRegistry(
          // GetDomainAndRegistry() is insensitive to leading dots, i.e.
          // to host/domain cookie distinctions.
          cookie.Domain(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  // If the cookie's domain is is not parsed as belonging to a registry
  // (e.g. for IP addresses or internal hostnames) an empty string will be
  // returned.  In this case, use the domain in the cookie.
  if (effective_domain.empty()) {
    if (cookie.IsDomainCookie())
      effective_domain = cookie.Domain().substr(1);
    else
      effective_domain = cookie.Domain();
  }

  return match_domains.count(effective_domain) != 0;
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

bool CookieDeletionInfo::TimeRange::Contains(const base::Time& time) const {
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
                                 CookieAccessSemantics access_semantics) const {
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
                                 access_semantics)
           .IsInclude()) {
    return false;
  }

  if (!domains_and_ips_to_delete.empty() &&
      !DomainMatchesDomains(cookie, domains_and_ips_to_delete)) {
    return false;
  }

  if (!domains_and_ips_to_ignore.empty() &&
      DomainMatchesDomains(cookie, domains_and_ips_to_ignore)) {
    return false;
  }

  return true;
}

}  // namespace net
