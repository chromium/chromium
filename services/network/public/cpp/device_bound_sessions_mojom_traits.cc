// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/device_bound_sessions_mojom_traits.h"

#include "base/unguessable_token.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "net/cookies/cookie_constants.h"
#include "net/device_bound_sessions/challenge_result.h"
#include "net/device_bound_sessions/inclusion_result.h"
#include "net/device_bound_sessions/refresh_result.h"
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

// static
network::mojom::DeviceBoundSessionRefreshResult
EnumTraits<network::mojom::DeviceBoundSessionRefreshResult,
           net::device_bound_sessions::RefreshResult>::
    ToMojom(net::device_bound_sessions::RefreshResult input) {
  using RefreshResult = net::device_bound_sessions::RefreshResult;
  using MojomRefreshResult = network::mojom::DeviceBoundSessionRefreshResult;
  switch (input) {
    case RefreshResult::kRefreshed:
      return MojomRefreshResult::kRefreshed;
    case RefreshResult::kInitializedService:
      return MojomRefreshResult::kInitializedService;
    case RefreshResult::kUnreachable:
      return MojomRefreshResult::kUnreachable;
    case RefreshResult::kServerError:
      return MojomRefreshResult::kServerError;
    case RefreshResult::kRefreshQuotaExceeded:
      return MojomRefreshResult::kRefreshQuotaExceeded;
    case RefreshResult::kFatalError:
      return MojomRefreshResult::kFatalError;
    case RefreshResult::kSigningQuotaExceeded:
      return MojomRefreshResult::kSigningQuotaExceeded;
  }
  NOTREACHED();
}

// static
bool EnumTraits<network::mojom::DeviceBoundSessionRefreshResult,
                net::device_bound_sessions::RefreshResult>::
    FromMojom(network::mojom::DeviceBoundSessionRefreshResult input,
              net::device_bound_sessions::RefreshResult* output) {
  using RefreshResult = net::device_bound_sessions::RefreshResult;
  using MojomRefreshResult = network::mojom::DeviceBoundSessionRefreshResult;
  switch (input) {
    case MojomRefreshResult::kRefreshed:
      *output = RefreshResult::kRefreshed;
      return true;
    case MojomRefreshResult::kInitializedService:
      *output = RefreshResult::kInitializedService;
      return true;
    case MojomRefreshResult::kUnreachable:
      *output = RefreshResult::kUnreachable;
      return true;
    case MojomRefreshResult::kServerError:
      *output = RefreshResult::kServerError;
      return true;
    case MojomRefreshResult::kRefreshQuotaExceeded:
      *output = RefreshResult::kRefreshQuotaExceeded;
      return true;
    case MojomRefreshResult::kFatalError:
      *output = RefreshResult::kFatalError;
      return true;
    case MojomRefreshResult::kSigningQuotaExceeded:
      *output = RefreshResult::kSigningQuotaExceeded;
      return true;
  }
  return false;
}

// static
network::mojom::DeviceBoundSessionChallengeResult
EnumTraits<network::mojom::DeviceBoundSessionChallengeResult,
           net::device_bound_sessions::ChallengeResult>::
    ToMojom(net::device_bound_sessions::ChallengeResult input) {
  using ChallengeResult = net::device_bound_sessions::ChallengeResult;
  using MojomChallengeResult =
      network::mojom::DeviceBoundSessionChallengeResult;
  switch (input) {
    case ChallengeResult::kSuccess:
      return MojomChallengeResult::kSuccess;
    case ChallengeResult::kNoSessionId:
      return MojomChallengeResult::kNoSessionId;
    case ChallengeResult::kNoSessionMatch:
      return MojomChallengeResult::kNoSessionMatch;
    case ChallengeResult::kCantSetBoundCookie:
      return MojomChallengeResult::kCantSetBoundCookie;
  }
  NOTREACHED();
}

// static
bool EnumTraits<network::mojom::DeviceBoundSessionChallengeResult,
                net::device_bound_sessions::ChallengeResult>::
    FromMojom(network::mojom::DeviceBoundSessionChallengeResult input,
              net::device_bound_sessions::ChallengeResult* output) {
  using ChallengeResult = net::device_bound_sessions::ChallengeResult;
  using MojomChallengeResult =
      network::mojom::DeviceBoundSessionChallengeResult;
  switch (input) {
    case MojomChallengeResult::kSuccess:
      *output = ChallengeResult::kSuccess;
      return true;
    case MojomChallengeResult::kNoSessionId:
      *output = ChallengeResult::kNoSessionId;
      return true;
    case MojomChallengeResult::kNoSessionMatch:
      *output = ChallengeResult::kNoSessionMatch;
      return true;
    case MojomChallengeResult::kCantSetBoundCookie:
      *output = ChallengeResult::kCantSetBoundCookie;
      return true;
  }
  return false;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
                  net::device_bound_sessions::FailedRequest>::
    Read(network::mojom::DeviceBoundSessionFailedRequestDataView data,
         net::device_bound_sessions::FailedRequest* out) {
  out->net_error = data.net_error();
  out->response_error = data.response_error();
  return data.ReadRequestUrl(&out->request_url) &&
         data.ReadResponseErrorBody(&out->response_error_body);
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionCreationDetailsDataView,
                  net::device_bound_sessions::CreationEventDetails>::
    Read(network::mojom::DeviceBoundSessionCreationDetailsDataView data,
         net::device_bound_sessions::CreationEventDetails* out) {
  return data.ReadFetchError(&out->fetch_error) &&
         data.ReadNewSessionDisplay(&out->new_session_display) &&
         data.ReadFailedRequest(&out->failed_request);
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
                  net::device_bound_sessions::RefreshEventDetails>::
    Read(network::mojom::DeviceBoundSessionRefreshDetailsDataView data,
         net::device_bound_sessions::RefreshEventDetails* out) {
  out->was_fully_proactive_refresh = data.was_fully_proactive_refresh();
  return data.ReadRefreshResult(&out->refresh_result) &&
         data.ReadFetchError(&out->fetch_error) &&
         data.ReadNewSessionDisplay(&out->new_session_display) &&
         data.ReadFailedRequest(&out->failed_request);
}
// static
bool StructTraits<network::mojom::DeviceBoundSessionTerminationDetailsDataView,
                  net::device_bound_sessions::TerminationEventDetails>::
    Read(network::mojom::DeviceBoundSessionTerminationDetailsDataView data,
         net::device_bound_sessions::TerminationEventDetails* out) {
  return data.ReadDeletionReason(&out->deletion_reason);
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionChallengeDetailsDataView,
                  net::device_bound_sessions::ChallengeEventDetails>::
    Read(network::mojom::DeviceBoundSessionChallengeDetailsDataView data,
         net::device_bound_sessions::ChallengeEventDetails* out) {
  return data.ReadChallengeResult(&out->challenge_result) &&
         data.ReadChallenge(&out->challenge);
}

// static
bool UnionTraits<network::mojom::DeviceBoundSessionEventTypeDetailsDataView,
                 net::device_bound_sessions::SessionEventTypeDetails>::
    Read(network::mojom::DeviceBoundSessionEventTypeDetailsDataView data,
         net::device_bound_sessions::SessionEventTypeDetails* out) {
  switch (data.tag()) {
    case network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
        kCreation: {
      net::device_bound_sessions::CreationEventDetails details;
      if (!data.ReadCreation(&details)) {
        return false;
      }
      *out = std::move(details);
      return true;
    }
    case network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
        kRefresh: {
      net::device_bound_sessions::RefreshEventDetails details;
      if (!data.ReadRefresh(&details)) {
        return false;
      }
      *out = std::move(details);
      return true;
    }
    case network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
        kTermination: {
      net::device_bound_sessions::TerminationEventDetails details;
      if (!data.ReadTermination(&details)) {
        return false;
      }
      *out = std::move(details);
      return true;
    }
    case network::mojom::DeviceBoundSessionEventTypeDetailsDataView::Tag::
        kChallenge: {
      net::device_bound_sessions::ChallengeEventDetails details;
      if (!data.ReadChallenge(&details)) {
        return false;
      }
      *out = std::move(details);
      return true;
    }
  }
}

// static
const base::UnguessableToken&
StructTraits<network::mojom::DeviceBoundSessionEventDataView,
             net::device_bound_sessions::SessionEvent>::
    event_id(const net::device_bound_sessions::SessionEvent& event) {
  return event.event_id;
}

// static
const net::SchemefulSite&
StructTraits<network::mojom::DeviceBoundSessionEventDataView,
             net::device_bound_sessions::SessionEvent>::
    site(const net::device_bound_sessions::SessionEvent& event) {
  return event.site;
}

// static
const std::optional<std::string>&
StructTraits<network::mojom::DeviceBoundSessionEventDataView,
             net::device_bound_sessions::SessionEvent>::
    session_id(const net::device_bound_sessions::SessionEvent& event) {
  return event.session_id;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionEventDataView,
                  net::device_bound_sessions::SessionEvent>::
    succeeded(const net::device_bound_sessions::SessionEvent& event) {
  return event.succeeded;
}

// static
const net::device_bound_sessions::SessionEventTypeDetails&
StructTraits<network::mojom::DeviceBoundSessionEventDataView,
             net::device_bound_sessions::SessionEvent>::
    event_type_details(const net::device_bound_sessions::SessionEvent& event) {
  return event.event_type_details;
}

// static
net::device_bound_sessions::SessionError::ErrorType
StructTraits<network::mojom::DeviceBoundSessionCreationDetailsDataView,
             net::device_bound_sessions::CreationEventDetails>::
    fetch_error(
        const net::device_bound_sessions::CreationEventDetails& event_details) {
  return event_details.fetch_error;
}

// static
const std::optional<net::device_bound_sessions::SessionError::ErrorType>&
StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
             net::device_bound_sessions::RefreshEventDetails>::
    fetch_error(
        const net::device_bound_sessions::RefreshEventDetails& event_details) {
  return event_details.fetch_error;
}

// static
net::device_bound_sessions::RefreshResult
StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
             net::device_bound_sessions::RefreshEventDetails>::
    refresh_result(
        const net::device_bound_sessions::RefreshEventDetails& event_details) {
  return event_details.refresh_result;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
                  net::device_bound_sessions::RefreshEventDetails>::
    was_fully_proactive_refresh(
        const net::device_bound_sessions::RefreshEventDetails& event_details) {
  return event_details.was_fully_proactive_refresh;
}

// static
net::device_bound_sessions::ChallengeResult
StructTraits<network::mojom::DeviceBoundSessionChallengeDetailsDataView,
             net::device_bound_sessions::ChallengeEventDetails>::
    challenge_result(const net::device_bound_sessions::ChallengeEventDetails&
                         event_details) {
  return event_details.challenge_result;
}

// static
const std::string&
StructTraits<network::mojom::DeviceBoundSessionChallengeDetailsDataView,
             net::device_bound_sessions::ChallengeEventDetails>::
    challenge(const net::device_bound_sessions::ChallengeEventDetails&
                  event_details) {
  return event_details.challenge;
}

// static
net::device_bound_sessions::DeletionReason
StructTraits<network::mojom::DeviceBoundSessionTerminationDetailsDataView,
             net::device_bound_sessions::TerminationEventDetails>::
    deletion_reason(const net::device_bound_sessions::TerminationEventDetails&
                        event_details) {
  return event_details.deletion_reason;
}

// static
const GURL&
StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
             net::device_bound_sessions::FailedRequest>::
    request_url(const net::device_bound_sessions::FailedRequest& r) {
  return r.request_url;
}

// static
std::optional<int32_t>
StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
             net::device_bound_sessions::FailedRequest>::
    net_error(const net::device_bound_sessions::FailedRequest& r) {
  return r.net_error;
}

// static
std::optional<int32_t>
StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
             net::device_bound_sessions::FailedRequest>::
    response_error(const net::device_bound_sessions::FailedRequest& r) {
  return r.response_error;
}

// static
const std::optional<std::string>&
StructTraits<network::mojom::DeviceBoundSessionFailedRequestDataView,
             net::device_bound_sessions::FailedRequest>::
    response_error_body(const net::device_bound_sessions::FailedRequest& r) {
  return r.response_error_body;
}

// static
const std::optional<net::device_bound_sessions::SessionDisplay>&
StructTraits<network::mojom::DeviceBoundSessionCreationDetailsDataView,
             net::device_bound_sessions::CreationEventDetails>::
    new_session_display(
        const net::device_bound_sessions::CreationEventDetails& obj) {
  return obj.new_session_display;
}

// static
const std::optional<net::device_bound_sessions::FailedRequest>&
StructTraits<network::mojom::DeviceBoundSessionCreationDetailsDataView,
             net::device_bound_sessions::CreationEventDetails>::
    failed_request(
        const net::device_bound_sessions::CreationEventDetails& obj) {
  return obj.failed_request;
}

// static
const std::optional<net::device_bound_sessions::SessionDisplay>&
StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
             net::device_bound_sessions::RefreshEventDetails>::
    new_session_display(
        const net::device_bound_sessions::RefreshEventDetails& obj) {
  return obj.new_session_display;
}

// static
const std::optional<net::device_bound_sessions::FailedRequest>&
StructTraits<network::mojom::DeviceBoundSessionRefreshDetailsDataView,
             net::device_bound_sessions::RefreshEventDetails>::
    failed_request(const net::device_bound_sessions::RefreshEventDetails& obj) {
  return obj.failed_request;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionEventDataView,
                  net::device_bound_sessions::SessionEvent>::
    Read(network::mojom::DeviceBoundSessionEventDataView data,
         net::device_bound_sessions::SessionEvent* out) {
  out->succeeded = data.succeeded();
  return data.ReadEventId(&out->event_id) && data.ReadSite(&out->site) &&
         data.ReadSessionId(&out->session_id) &&
         data.ReadEventTypeDetails(&out->event_type_details);
}

}  // namespace mojo
