// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session.h"

#include <memory>

#include "base/strings/escape.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_params.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/cookie_craving.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_inclusion_rules.h"
#include "net/device_bound_sessions/session_usage.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

namespace {

constexpr base::TimeDelta kSessionTtl = base::Days(400);

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    3,

    // Initial delay for exponential backoff in ms.
    500,

    // Factor by which the waiting time will be multiplied.
    1.5,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 8,  // 8 Minutes

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};
}

Session::Session(Id id, url::Origin origin, GURL refresh)
    : id_(id),
      refresh_url_(refresh),
      inclusion_rules_(origin),
      backoff_(&kBackoffPolicy) {}

Session::Session(Id id,
                 GURL refresh,
                 SessionInclusionRules inclusion_rules,
                 std::vector<CookieCraving> cookie_cravings,
                 bool should_defer_when_expired,
                 base::Time creation_date,
                 base::Time expiry_date)
    : id_(id),
      refresh_url_(refresh),
      inclusion_rules_(std::move(inclusion_rules)),
      cookie_cravings_(std::move(cookie_cravings)),
      should_defer_when_expired_(should_defer_when_expired),
      creation_date_(creation_date),
      expiry_date_(expiry_date),
      backoff_(&kBackoffPolicy) {}

Session::~Session() = default;

// static
base::expected<std::unique_ptr<Session>, SessionError> Session::CreateIfValid(
    const SessionParams& params) {
  if (!params.fetcher_url.is_valid()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidFetcherUrl,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  } else if (params.refresh_url.empty()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidRefreshUrl,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  } else if (params.session_id.empty()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidSessionId,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  }

  // If there is an origin in the scope, verify it is valid. Default to the
  // fetcher URL if the origin is missing from the scope.
  GURL scope_origin_as_url = params.scope.origin.empty()
                                 ? params.fetcher_url
                                 : GURL(params.scope.origin);
  url::Origin scope_origin = url::Origin::Create(scope_origin_as_url);
  if (scope_origin.opaque()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidScopeOrigin,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  }

  // Check if the scope-origin is samesite with fetcher URL.
  if (net::SchemefulSite(scope_origin_as_url) !=
      net::SchemefulSite(params.fetcher_url)) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kScopeOriginSameSiteMismatch,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  }

  // The refresh endpoint can be a full URL (samesite with request origin)
  // or a relative URL, starting with a "/" to make it origin-relative,
  // and starting with anything else making it current-path-relative to
  // request URL.
  std::string unescaped_path = base::UnescapeURLComponent(
      params.refresh_url,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  GURL candidate_refresh_endpoint = params.fetcher_url.Resolve(unescaped_path);

  // Check if the refresh URL is valid, secure.
  if (!candidate_refresh_endpoint.is_valid() ||
      !IsSecure(candidate_refresh_endpoint)) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidRefreshUrl,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  }

  // Check if the refresh URL is same-site with the fetcher URL.
  if (net::SchemefulSite(candidate_refresh_endpoint) !=
      net::SchemefulSite(params.fetcher_url)) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kRefreshUrlSameSiteMismatch,
                     net::SchemefulSite(), /*session_id=*/std::nullopt});
  }

  std::unique_ptr<Session> session(new Session(
      Id(params.session_id), scope_origin, candidate_refresh_endpoint));
  for (const auto& spec : params.scope.specifications) {
    if (!spec.domain.empty() && !spec.path.empty()) {
      const auto inclusion_result =
          spec.type == SessionParams::Scope::Specification::Type::kExclude
              ? SessionInclusionRules::InclusionResult::kExclude
              : SessionInclusionRules::InclusionResult::kInclude;
      session->inclusion_rules_.AddUrlRuleIfValid(inclusion_result, spec.domain,
                                                  spec.path);
    }
  }

  // Sessions should never include the refresh endpoint, since that would
  // prevent them from ever refreshing when a cookie expires.
  session->inclusion_rules_.AddUrlRuleIfValid(
      SessionInclusionRules::InclusionResult::kExclude,
      candidate_refresh_endpoint.host(), candidate_refresh_endpoint.path());

  session->inclusion_rules_.SetIncludeSite(params.scope.include_site);

  for (const auto& cred : params.credentials) {
    if (!cred.name.empty()) {
      std::optional<CookieCraving> craving =
          CookieCraving::Create(params.fetcher_url, cred.name, cred.attributes,
                                base::Time::Now(), std::nullopt);
      if (craving) {
        session->cookie_cravings_.push_back(*craving);
      }
    }
  }

  session->set_creation_date(base::Time::Now());
  session->set_expiry_date(base::Time::Now() + kSessionTtl);
  session->set_unexportable_key_id(std::move(params.key_id));

  return base::ok(std::move(session));
}

// static
std::unique_ptr<Session> Session::CreateFromProto(const proto::Session& proto) {
  if (!proto.has_id() || !proto.has_refresh_url() ||
      !proto.has_should_defer_when_expired() || !proto.has_expiry_time() ||
      !proto.has_session_inclusion_rules() || !proto.cookie_cravings_size()) {
    return nullptr;
  }

  if (proto.id().empty()) {
    return nullptr;
  }

  GURL refresh(proto.refresh_url());
  if (!refresh.is_valid()) {
    return nullptr;
  }

  std::unique_ptr<SessionInclusionRules> inclusion_rules =
      SessionInclusionRules::CreateFromProto(proto.session_inclusion_rules());
  if (!inclusion_rules) {
    return nullptr;
  }

  std::vector<CookieCraving> cravings;
  for (const auto& craving_proto : proto.cookie_cravings()) {
    std::optional<CookieCraving> craving =
        CookieCraving::CreateFromProto(craving_proto);
    if (!craving.has_value()) {
      return nullptr;
    }
    cravings.push_back(std::move(*craving));
  }

  auto creation_date = base::Time::Now();
  if (proto.has_creation_time()) {
    creation_date = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(proto.creation_time()));
  }

  auto expiry_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(proto.expiry_time()));
  if (base::Time::Now() > expiry_date) {
    return nullptr;
  }

  std::unique_ptr<Session> result(new Session(
      Id(proto.id()), std::move(refresh), std::move(*inclusion_rules),
      std::move(cravings), proto.should_defer_when_expired(), creation_date,
      expiry_date));

  return result;
}

proto::Session Session::ToProto() const {
  proto::Session session_proto;
  session_proto.set_id(*id_);
  session_proto.set_refresh_url(refresh_url_.spec());
  session_proto.set_should_defer_when_expired(should_defer_when_expired_);
  session_proto.set_creation_time(
      creation_date_.ToDeltaSinceWindowsEpoch().InMicroseconds());
  session_proto.set_expiry_time(
      expiry_date_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  *session_proto.mutable_session_inclusion_rules() = inclusion_rules_.ToProto();

  for (auto& craving : cookie_cravings_) {
    session_proto.mutable_cookie_cravings()->Add(craving.ToProto());
  }

  return session_proto;
}

bool Session::ShouldDeferRequest(
    URLRequest* request,
    const net::FirstPartySetMetadata& first_party_set_metadata) const {
  if (backoff_.ShouldRejectRequest()) {
    return false;
  }

  if (request->device_bound_session_usage() < SessionUsage::kNoUsage) {
    request->set_device_bound_session_usage(SessionUsage::kNoUsage);
  }

  if (!IncludesUrl(request->url())) {
    // Request is not in scope for this session.
    return false;
  }

  if (request->device_bound_session_usage() <
      SessionUsage::kInScopeNotDeferred) {
    request->set_device_bound_session_usage(SessionUsage::kInScopeNotDeferred);
  }

  request->net_log().AddEvent(
      net::NetLogEventType::DBSC_REQUEST, [&](NetLogCaptureMode capture_mode) {
        base::Value::Dict dict;
        dict.Set("refresh_url", refresh_url_.spec());
        dict.Set("scope", inclusion_rules_.DebugString());

        base::Value::List credentials;
        for (const CookieCraving& craving : cookie_cravings_) {
          credentials.Append(craving.DebugString());
        }

        dict.Set("credentials", std::move(credentials));

        if (NetLogCaptureIncludesSensitive(capture_mode)) {
          dict.Set("session_id", id_.value());
        }

        return dict;
      });

  // TODO(crbug.com/353766029): Refactor this.
  // The below is all copied from AddCookieHeaderAndStart. We should refactor
  // it.
  CookieStore* cookie_store = request->context()->cookie_store();
  bool force_ignore_site_for_cookies = request->force_ignore_site_for_cookies();
  if (cookie_store->cookie_access_delegate() &&
      cookie_store->cookie_access_delegate()->ShouldIgnoreSameSiteRestrictions(
          request->url(), request->site_for_cookies())) {
    force_ignore_site_for_cookies = true;
  }

  bool is_main_frame_navigation =
      IsolationInfo::RequestType::kMainFrame ==
          request->isolation_info().request_type() ||
      request->force_main_frame_for_same_site_cookies();
  CookieOptions::SameSiteCookieContext same_site_context =
      net::cookie_util::ComputeSameSiteContextForRequest(
          request->method(), request->url_chain(), request->site_for_cookies(),
          request->initiator(), is_main_frame_navigation,
          force_ignore_site_for_cookies);

  CookieOptions options;
  options.set_same_site_cookie_context(same_site_context);
  options.set_include_httponly();
  // Not really relevant for CookieCraving, but might as well make it explicit.
  options.set_do_not_update_access_time();

  CookieAccessParams params{CookieAccessSemantics::NONLEGACY,
                            CookieScopeSemantics::UNKNOWN,
                            // DBSC only affects secure URLs
                            false};

  // The main logic. This checks every CookieCraving against every (real)
  // CanonicalCookie.
  for (const CookieCraving& cookie_craving : cookie_cravings_) {
    if (!cookie_craving.ShouldIncludeForRequest(
            request, first_party_set_metadata, options, params)) {
      continue;
    }

    bool satisfied = false;
    for (const CookieWithAccessResult& request_cookie :
         request->maybe_sent_cookies()) {
      // Note that any request_cookie that satisfies the craving is fine, even
      // if it does not ultimately get included when sending the request. We
      // only need to ensure the cookie is present in the store.
      //
      // Note that in general if a CanonicalCookie isn't included, then the
      // corresponding CookieCraving typically also isn't included, but there
      // are exceptions.
      //
      // For example, if a CookieCraving is for a secure cookie, and the
      // request is insecure, then the CookieCraving will be excluded, but the
      // CanonicalCookie will be included. DBSC only applies to secure context
      // but there might be similar cases.
      //
      // TODO: think about edge cases here...
      if (cookie_craving.IsSatisfiedBy(request_cookie.cookie)) {
        satisfied = true;
        break;
      }
    }

    if (!satisfied) {
      request->net_log().AddEvent(
          net::NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED,
          [&](NetLogCaptureMode capture_mode) {
            base::Value::Dict dict;
            dict.Set("refresh_required_reason", "missing_cookie");

            if (NetLogCaptureIncludesSensitive(capture_mode)) {
              dict.Set("refresh_missing_cookie", cookie_craving.Name());
            }

            return dict;
          });

      // There's an unsatisfied craving. Defer the request.
      request->set_device_bound_session_usage(SessionUsage::kDeferred);
      return true;
    }
  }

  request->net_log().AddEvent(net::NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED,
                              [&](NetLogCaptureMode capture_mode) {
                                base::Value::Dict dict;
                                dict.Set("refresh_required_reason",
                                         "refresh_not_required");
                                return dict;
                              });

  // All cookiecravings satisfied.
  return false;
}

bool Session::IsEqualForTesting(const Session& other) const {
  if (!std::ranges::equal(
          cookie_cravings_, other.cookie_cravings_,
          [](const CookieCraving& lhs, const CookieCraving& rhs) {
            return lhs.IsEqualForTesting(rhs);  // IN-TEST
          })) {
    return false;
  }

  return id_ == other.id_ && refresh_url_ == other.refresh_url_ &&
         inclusion_rules_ == other.inclusion_rules_ &&
         should_defer_when_expired_ == other.should_defer_when_expired_ &&
         creation_date_ == other.creation_date_ &&
         expiry_date_ == other.expiry_date_ &&
         key_id_or_error_ == other.key_id_or_error_ &&
         cached_challenge_ == other.cached_challenge_;
}

void Session::RecordAccess() {
  expiry_date_ = base::Time::Now() + kSessionTtl;
}

bool Session::IncludesUrl(const GURL& url) const {
  return inclusion_rules_.EvaluateRequestUrl(url) ==
         SessionInclusionRules::kInclude;
}

void Session::InformOfRefreshResult(SessionError::ErrorType error_type) {
  using enum SessionError::ErrorType;

  switch (error_type) {
    case kSuccess:
      backoff_.InformOfRequest(/*succeeded=*/true);
      break;
    // Fatal errors, no backoff needed
    case kKeyError:
    case kSigningError:
    case kServerRequestedTermination:
    case kInvalidConfigJson:
    case kInvalidSessionId:
    case kInvalidCredentials:
    case kInvalidChallenge:
    case kTooManyChallenges:
    case kInvalidFetcherUrl:
    case kInvalidRefreshUrl:
    case kPersistentHttpError:
    case kScopeOriginSameSiteMismatch:
    case kRefreshUrlSameSiteMismatch:
    case kInvalidScopeOrigin:

    // We do not want to back off on many network connection errors
    // (e.g. internet disconnected), so we do not hit our maximum
    // backoff whenever the machine goes offline while the browser is
    // running.
    case kNetError:
      break;
    case kTransientHttpError:
      backoff_.InformOfRequest(/*succeeded=*/false);
      break;
  }
}

}  // namespace net::device_bound_sessions
