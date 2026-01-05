// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/device_bound_sessions_mojom_traits.h"

#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/cookies/cookie_constants.h"
#include "net/device_bound_sessions/inclusion_result.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_params.h"
#include "services/network/public/cpp/cookie_manager_mojom_traits.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::DeviceBoundSessionKeyDataView,
                  net::device_bound_sessions::SessionKey>::
    Read(network::mojom::DeviceBoundSessionKeyDataView data,
         net::device_bound_sessions::SessionKey* out) {
  if (!data.ReadSite(&out->site)) {
    return false;
  }

  if (!data.ReadId(&out->id.value())) {
    return false;
  }

  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionAccessDataView,
                  net::device_bound_sessions::SessionAccess>::
    Read(network::mojom::DeviceBoundSessionAccessDataView data,
         net::device_bound_sessions::SessionAccess* out) {
  if (!data.ReadAccessType(&out->access_type)) {
    return false;
  }

  if (!data.ReadSessionKey(&out->session_key)) {
    return false;
  }

  if (!data.ReadCookies(&out->cookies)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionScopeSpecificationDataView,
    net::device_bound_sessions::SessionParams::Scope::Specification>::
    Read(network::mojom::DeviceBoundSessionScopeSpecificationDataView data,
         net::device_bound_sessions::SessionParams::Scope::Specification* out) {
  if (!data.ReadType(&out->type)) {
    return false;
  }
  if (!data.ReadDomain(&out->domain)) {
    return false;
  }
  if (!data.ReadPath(&out->path)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionScopeDataView,
                  net::device_bound_sessions::SessionParams::Scope>::
    Read(network::mojom::DeviceBoundSessionScopeDataView data,
         net::device_bound_sessions::SessionParams::Scope* out) {
  out->include_site = data.include_site();
  if (!data.ReadSpecifications(&out->specifications)) {
    return false;
  }
  if (!data.ReadOrigin(&out->origin)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionCredentialDataView,
                  net::device_bound_sessions::SessionParams::Credential>::
    Read(network::mojom::DeviceBoundSessionCredentialDataView data,
         net::device_bound_sessions::SessionParams::Credential* out) {
  if (!data.ReadName(&out->name)) {
    return false;
  }
  if (!data.ReadAttributes(&out->attributes)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionParamsDataView,
                  net::device_bound_sessions::SessionParams>::
    Read(network::mojom::DeviceBoundSessionParamsDataView data,
         net::device_bound_sessions::SessionParams* out) {
  std::string session_id;
  GURL fetcher_url;
  std::string refresh_url;
  net::device_bound_sessions::SessionParams::Scope scope;
  std::vector<net::device_bound_sessions::SessionParams::Credential>
      credentials;
  std::vector<std::string> allowed_refresh_initiators;

  if (!data.ReadSessionId(&session_id) || !data.ReadFetcherUrl(&fetcher_url) ||
      !data.ReadRefreshUrl(&refresh_url) || !data.ReadScope(&scope) ||
      !data.ReadCredentials(&credentials) ||
      !data.ReadAllowedRefreshInitiators(&allowed_refresh_initiators)) {
    return false;
  }

  *out = net::device_bound_sessions::SessionParams(
      std::move(session_id), std::move(fetcher_url), std::move(refresh_url),
      std::move(scope), std::move(credentials),
      unexportable_keys::UnexportableKeyId(),
      std::move(allowed_refresh_initiators));

  return true;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
             net::device_bound_sessions::CookieCravingDisplay>::
    name(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.name;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
             net::device_bound_sessions::CookieCravingDisplay>::
    domain(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.domain;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
             net::device_bound_sessions::CookieCravingDisplay>::
    path(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.path;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
    net::device_bound_sessions::CookieCravingDisplay>::
    secure(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.secure;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
    net::device_bound_sessions::CookieCravingDisplay>::
    http_only(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.http_only;
}

// static
net::CookieSameSite
StructTraits<network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
             net::device_bound_sessions::CookieCravingDisplay>::
    same_site(const net::device_bound_sessions::CookieCravingDisplay& r) {
  return r.same_site;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionCookieCravingDisplayDataView,
    net::device_bound_sessions::CookieCravingDisplay>::
    Read(network::mojom::DeviceBoundSessionCookieCravingDisplayDataView data,
         net::device_bound_sessions::CookieCravingDisplay* out) {
  if (!data.ReadName(&out->name) || !data.ReadDomain(&out->domain) ||
      !data.ReadPath(&out->path) || !data.ReadSameSite(&out->same_site)) {
    return false;
  }
  out->secure = data.secure();
  out->http_only = data.http_only();
  return true;
}

// static
network::mojom::DeviceBoundSessionInclusionResult
EnumTraits<network::mojom::DeviceBoundSessionInclusionResult,
           net::device_bound_sessions::InclusionResult>::
    ToMojom(net::device_bound_sessions::InclusionResult inclusion_result) {
  switch (inclusion_result) {
    case net::device_bound_sessions::InclusionResult::kExclude:
      return network::mojom::DeviceBoundSessionInclusionResult::kExclude;
    case net::device_bound_sessions::InclusionResult::kInclude:
      return network::mojom::DeviceBoundSessionInclusionResult::kInclude;
  }
}

// static
bool EnumTraits<network::mojom::DeviceBoundSessionInclusionResult,
                net::device_bound_sessions::InclusionResult>::
    FromMojom(network::mojom::DeviceBoundSessionInclusionResult input,
              net::device_bound_sessions::InclusionResult* output) {
  switch (input) {
    case network::mojom::DeviceBoundSessionInclusionResult::kExclude:
      *output = net::device_bound_sessions::InclusionResult::kExclude;
      return true;
    case network::mojom::DeviceBoundSessionInclusionResult::kInclude:
      *output = net::device_bound_sessions::InclusionResult::kInclude;
      return true;
  }
  return false;
}

// static
net::device_bound_sessions::InclusionResult
StructTraits<network::mojom::DeviceBoundSessionUrlRuleDisplayDataView,
             net::device_bound_sessions::UrlRuleDisplay>::
    rule_type(const net::device_bound_sessions::UrlRuleDisplay& r) {
  return r.rule_type;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionUrlRuleDisplayDataView,
             net::device_bound_sessions::UrlRuleDisplay>::
    host_pattern(const net::device_bound_sessions::UrlRuleDisplay& r) {
  return r.host_pattern;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionUrlRuleDisplayDataView,
             net::device_bound_sessions::UrlRuleDisplay>::
    path_prefix(const net::device_bound_sessions::UrlRuleDisplay& r) {
  return r.path_prefix;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionUrlRuleDisplayDataView,
                  net::device_bound_sessions::UrlRuleDisplay>::
    Read(network::mojom::DeviceBoundSessionUrlRuleDisplayDataView data,
         net::device_bound_sessions::UrlRuleDisplay* out) {
  return data.ReadRuleType(&out->rule_type) &&
         data.ReadHostPattern(&out->host_pattern) &&
         data.ReadPathPrefix(&out->path_prefix);
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView,
             net::device_bound_sessions::SessionInclusionRulesDisplay>::
    origin(const net::device_bound_sessions::SessionInclusionRulesDisplay& r) {
  return r.origin;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView,
    net::device_bound_sessions::SessionInclusionRulesDisplay>::
    include_site(
        const net::device_bound_sessions::SessionInclusionRulesDisplay& r) {
  return r.include_site;
}

// static
const std::vector<net::device_bound_sessions::UrlRuleDisplay>&
StructTraits<network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView,
             net::device_bound_sessions::SessionInclusionRulesDisplay>::
    url_rules(
        const net::device_bound_sessions::SessionInclusionRulesDisplay& r) {
  return r.url_rules;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView,
    net::device_bound_sessions::SessionInclusionRulesDisplay>::
    Read(network::mojom::DeviceBoundSessionInclusionRulesDisplayDataView data,
         net::device_bound_sessions::SessionInclusionRulesDisplay* out) {
  out->include_site = data.include_site();
  return data.ReadUrlRules(&out->url_rules) && data.ReadOrigin(&out->origin);
}

// static
const net::device_bound_sessions::SessionKey&
StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
             net::device_bound_sessions::SessionDisplay>::
    key(const net::device_bound_sessions::SessionDisplay& r) {
  return r.key;
}

// static
const GURL& StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
                         net::device_bound_sessions::SessionDisplay>::
    refresh_url(const net::device_bound_sessions::SessionDisplay& r) {
  return r.refresh_url;
}

// static
const net::device_bound_sessions::SessionInclusionRulesDisplay&
StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
             net::device_bound_sessions::SessionDisplay>::
    inclusion_rules(const net::device_bound_sessions::SessionDisplay& r) {
  return r.inclusion_rules;
}

// static
const std::vector<net::device_bound_sessions::CookieCravingDisplay>&
StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
             net::device_bound_sessions::SessionDisplay>::
    cookie_cravings(const net::device_bound_sessions::SessionDisplay& r) {
  return r.cookie_cravings;
}

// static
base::Time StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
                        net::device_bound_sessions::SessionDisplay>::
    expiry_date(const net::device_bound_sessions::SessionDisplay& r) {
  return r.expiry_date;
}

// static
const std::optional<std::string>&
StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
             net::device_bound_sessions::SessionDisplay>::
    cached_challenge(const net::device_bound_sessions::SessionDisplay& r) {
  return r.cached_challenge;
}

// static
const std::vector<std::string>&
StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
             net::device_bound_sessions::SessionDisplay>::
    allowed_refresh_initiators(
        const net::device_bound_sessions::SessionDisplay& r) {
  return r.allowed_refresh_initiators;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionDisplayDataView,
                  net::device_bound_sessions::SessionDisplay>::
    Read(network::mojom::DeviceBoundSessionDisplayDataView data,
         net::device_bound_sessions::SessionDisplay* out) {
  return data.ReadKey(&out->key) && data.ReadRefreshUrl(&out->refresh_url) &&
         data.ReadInclusionRules(&out->inclusion_rules) &&
         data.ReadCookieCravings(&out->cookie_cravings) &&
         data.ReadExpiryDate(&out->expiry_date) &&
         data.ReadAllowedRefreshInitiators(&out->allowed_refresh_initiators) &&
         data.ReadCachedChallenge(&out->cached_challenge);
}
}  // namespace mojo
