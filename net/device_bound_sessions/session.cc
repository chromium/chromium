// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session.h"

#include <memory>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/cookie_craving.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_inclusion_rules.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

Session::Session(Id id, url::Origin origin, GURL refresh)
    : id_(id), refresh_url_(refresh), inclusion_rules_(origin) {}

Session::Session(Id id,
                 GURL refresh,
                 SessionInclusionRules inclusion_rules,
                 std::vector<CookieCraving> cookie_cravings,
                 bool should_defer_when_expired,
                 base::Time expiry_date)
    : id_(id),
      refresh_url_(refresh),
      inclusion_rules_(std::move(inclusion_rules)),
      cookie_cravings_(std::move(cookie_cravings)),
      should_defer_when_expired_(should_defer_when_expired),
      expiry_date_(expiry_date) {}

Session::~Session() = default;

// static
std::unique_ptr<Session> Session::CreateIfValid(const SessionParams& params,
                                                GURL url) {
  GURL refresh(params.refresh_url);
  if (!refresh.is_valid()) {
    return nullptr;
  }

  if (params.session_id.empty()) {
    return nullptr;
  }

  std::unique_ptr<Session> session(
      new Session(Id(params.session_id), url::Origin::Create(url), refresh));
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

  for (const auto& cred : params.credentials) {
    if (!cred.name.empty() && !cred.attributes.empty()) {
      std::optional<CookieCraving> craving = CookieCraving::Create(
          url, cred.name, cred.attributes, base::Time::Now(), std::nullopt);
      if (craving) {
        session->cookie_cravings_.push_back(*craving);
      }
    }
  }

  return session;
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

  auto expiry_date = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(proto.expiry_time()));
  std::unique_ptr<Session> result(new Session(
      Id(proto.id()), std::move(refresh), std::move(*inclusion_rules),
      std::move(cravings), proto.should_defer_when_expired(), expiry_date));

  return result;
}

proto::Session Session::ToProto() const {
  proto::Session session_proto;
  session_proto.set_id(*id_);
  session_proto.set_refresh_url(refresh_url_.spec());
  session_proto.set_should_defer_when_expired(should_defer_when_expired_);
  session_proto.set_expiry_time(
      expiry_date_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  *session_proto.mutable_session_inclusion_rules() = inclusion_rules_.ToProto();

  for (auto& craving : cookie_cravings_) {
    session_proto.mutable_cookie_cravings()->Add(craving.ToProto());
  }

  return session_proto;
}

bool Session::ShouldDeferRequest(URLRequest* request) const {
  if (inclusion_rules_.EvaluateRequestUrl(request->url()) ==
      SessionInclusionRules::kExclude) {
    // Request is not in scope for this session.
    return false;
  }

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
                            // DBSC only affects secure URLs
                            false};

  // The main logic. This checks every CookieCraving against every (real)
  // CanonicalCookie.
  for (const CookieCraving& cookie_craving : cookie_cravings_) {
    if (!cookie_craving.IncludeForRequestURL(request->url(), options, params)
             .status.IsInclude()) {
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
      // There's an unsatisfied craving. Defer the request.
      return true;
    }
  }

  // All cookiecravings satisfied.
  return false;
}

bool Session::IsEqualForTesting(const Session& other) const {
  if (!base::ranges::equal(
          cookie_cravings_, other.cookie_cravings_,
          [](const CookieCraving& lhs, const CookieCraving& rhs) {
            return lhs.IsEqualForTesting(rhs);  // IN-TEST
          })) {
    return false;
  }

  return id_ == other.id_ && refresh_url_ == other.refresh_url_ &&
         inclusion_rules_ == other.inclusion_rules_ &&
         should_defer_when_expired_ == other.should_defer_when_expired_ &&
         expiry_date_ == other.expiry_date_ &&
         key_id_or_error_ == other.key_id_or_error_ &&
         cached_challenge_ == other.cached_challenge_;
}

}  // namespace net::device_bound_sessions
