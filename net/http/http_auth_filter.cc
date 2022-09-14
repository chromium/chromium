// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_filter.h"

#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

// TODO(ahendrickson) -- Determine if we want separate allowlists for HTTP and
// HTTPS, one for both, or only an HTTP one.  My understanding is that the HTTPS
// entries in the registry mean that you are only allowed to connect to the site
// via HTTPS and still be considered 'safe'.

HttpAuthFilterAllowlist::HttpAuthFilterAllowlist(
    const std::string& server_allowlist) {
  SetAllowlist(server_allowlist);
}

HttpAuthFilterAllowlist::~HttpAuthFilterAllowlist() = default;

// Add a new domain |filter| to the allowlist, if it's not already there
bool HttpAuthFilterAllowlist::AddFilter(const std::string& filter,
                                        HttpAuth::Target target) {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  rules_.AddRuleFromString(filter);
  return true;
}

bool HttpAuthFilterAllowlist::IsValid(
    const url::SchemeHostPort& scheme_host_port,
    HttpAuth::Target target) const {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  return rules_.Matches(scheme_host_port.GetURL());
}

void HttpAuthFilterAllowlist::SetAllowlist(
    const std::string& server_allowlist) {
  // TODO(eroman): Is this necessary? The issue is that
  // HttpAuthFilterAllowlist is trying to use ProxyBypassRules as a generic
  // URL filter. However internally it has some implicit rules for localhost
  // and linklocal addresses.
  rules_.ParseFromString(ProxyBypassRules::GetRulesToSubtractImplicit() + ";" +
                         server_allowlist);
}

}  // namespace net
