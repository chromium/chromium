// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session.h"

#include <string_view>

#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/host_patterns.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

using base::test::ErrorIs;

namespace net::device_bound_sessions {

namespace {

class SessionTest : public ::testing::Test, public WithTaskEnvironment {
 protected:
  SessionTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()) {
    feature_list_.InitAndEnableFeature(features::kDeviceBoundSessions);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<URLRequestContext> context_;
};

constexpr net::NetworkTrafficAnnotationTag kDummyAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", "");
constexpr char kSessionId[] = "SessionId";
constexpr char kRefreshUrlString[] = "https://example.test/refresh";
constexpr char kUrlString[] = "https://example.test/index.html";
constexpr char kUrlStringForWrongETLD[] = "https://example.co.uk/index.html";
const GURL kTestUrl(kUrlString);
const GURL kRefreshUrl(kRefreshUrlString);
const GURL kTestUrlForWrongETLD(kUrlStringForWrongETLD);

SessionParams CreateValidParams() {
  SessionParams::Scope scope;
  scope.origin = "https://example.test";
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie",
                                 "Secure; Domain=example.test"}});
  return SessionParams{kSessionId,
                       kTestUrl,
                       kRefreshUrlString,
                       std::move(scope),
                       std::move(cookie_credentials),
                       unexportable_keys::UnexportableKeyId(),
                       /*allowed_refresh_initiators=*/{"*"}};
}

TEST_F(SessionTest, ValidService) {
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  EXPECT_OK(session_or_error);
  std::unique_ptr<Session> session = std::move(*session_or_error);
  EXPECT_TRUE(session);
}

TEST_F(SessionTest, DefaultExpiry) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateValidParams()));
  ASSERT_TRUE(session);
  EXPECT_LT(base::Time::Now() + base::Days(399), session->expiry_date());
}

TEST_F(SessionTest, RelativeServiceRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = "/internal/RefreshSession";
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);

  // Validate session refresh URL.
  EXPECT_EQ(session->refresh_url().spec(),
            "https://example.test/internal/RefreshSession");
}

TEST_F(SessionTest, RelativeServiceRefreshUrlEscaped) {
  auto params = CreateValidParams();
  params.refresh_url = "/internal%26RefreshSession";
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);

  // Validate session refresh URL.
  EXPECT_EQ(session->refresh_url().spec(),
            "https://example.test/internal&RefreshSession");
}

TEST_F(SessionTest, InvalidServiceRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = "http://?not-a-valid=url";
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kInvalidRefreshUrl)));
}

TEST_F(SessionTest, InvalidScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "hello world";
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kInvalidScopeOrigin)));
}

TEST_F(SessionTest, InvalidFetcherUrl) {
  auto params = CreateValidParams();
  params.fetcher_url = GURL();
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kInvalidFetcherUrl)));
}

TEST_F(SessionTest, InvalidScopeOriginWithPath) {
  auto params = CreateValidParams();
  params.scope.origin = "https://example.test/path";
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kScopeOriginContainsPath)));
}

TEST_F(SessionTest, InvalidScopeOriginWithTrailingSlash) {
  auto params = CreateValidParams();
  params.scope.origin = "https://example.test/";
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kScopeOriginContainsPath)));
}

TEST_F(SessionTest, ScopeOriginSameSiteMismatch) {
  auto params = CreateValidParams();
  params.fetcher_url = kTestUrlForWrongETLD;
  EXPECT_THAT(
      Session::CreateIfValid(params),
      ErrorIs(SessionError(SessionError::kScopeOriginSameSiteMismatch)));
}

TEST_F(SessionTest, ScopeOriginPrivateRegistryChildDomainSameSiteMismatch) {
  // Since appspot.com is on the Public Suffix List (with private registries
  // included), it should not be possible for any of its child domains to
  // create a session on the parent domain.
  auto params = CreateValidParams();
  params.fetcher_url = GURL("https://example.appspot.com/refresh");
  params.refresh_url = "https://example.appspot.com/refresh";
  params.scope.origin = "https://appspot.com";
  EXPECT_THAT(
      Session::CreateIfValid(params),
      ErrorIs(SessionError(SessionError::kScopeOriginSameSiteMismatch)));
}

TEST_F(SessionTest, SameSiteMismatchRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = kUrlStringForWrongETLD;
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kRefreshUrlSameSiteMismatch)));
}

TEST_F(SessionTest, NonSecureUrl) {
  // HTTP is not allowed for the refresh URL.
  {
    auto params = CreateValidParams();
    params.fetcher_url = GURL("http://example.test/index.html");
    params.refresh_url = "http://example.test/registration";
    params.scope.origin = "http://example.test";
    EXPECT_THAT(Session::CreateIfValid(params),
                ErrorIs(SessionError(SessionError::kInvalidRefreshUrl)));
  }

  // But localhost is okay.
  {
    auto params = CreateValidParams();
    params.fetcher_url = GURL("http://localhost:8080/index.html");
    params.refresh_url = "http://localhost:8080/registration";
    params.scope.origin = "http://localhost:8080";
    // localhost can't use the default cookies, which are only for example.test.
    params.credentials = {
        SessionParams::Credential{"test_cookie",
                                  /*attributes=*/"Domain=localhost"}};
    EXPECT_OK(Session::CreateIfValid(params));
  }
}

TEST_F(SessionTest, CreateSiteScopedWithSessionRule) {
  auto params = CreateValidParams();
  params.scope.include_site = true;
  params.scope.specifications.push_back(
      {SessionParams::Scope::Specification::Type::kExclude,
       "subdomain.example.test", "/index.html"});
  EXPECT_OK(Session::CreateIfValid(params));
}

TEST_F(SessionTest, CreateOriginScopedWithSessionRules) {
  auto params = CreateValidParams();
  params.scope.include_site = false;
  params.scope.specifications.push_back(
      {SessionParams::Scope::Specification::Type::kExclude,
       "subdomain.example.test", "/index.html"});
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(
                  SessionError::kScopeRuleOriginScopedHostPatternMismatch)));
}

TEST_F(SessionTest, CreateWithInvalidCredential) {
  auto params = CreateValidParams();
  // Try to create a cookie on the wrong domain.
  params.credentials = {SessionParams::Credential{
      "test_cookie",
      /*attributes=*/"Domain=some-other-domain.test"}};
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(
                  SessionError::kInvalidCredentialsCookieInvalidDomain)));

  // Try to create a cookie with no name.
  params.credentials = {
      SessionParams::Credential{"",
                                /*attributes=*/"Domain=example.test"}};
  EXPECT_THAT(Session::CreateIfValid(params),
              ErrorIs(SessionError(SessionError::kInvalidCredentialsCookie)));
}

TEST_F(SessionTest, ToFromProto) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateValidParams()));
  ASSERT_TRUE(session);

  // Convert to proto and validate contents.
  proto::Session sproto = session->ToProto();
  EXPECT_EQ(Session::Id(sproto.id()), session->id());
  EXPECT_EQ(sproto.refresh_url(), session->refresh_url().spec());
  EXPECT_EQ(sproto.should_defer_when_expired(),
            session->should_defer_when_expired());

  // Restore session from proto and validate contents.
  std::unique_ptr<Session> restored = Session::CreateFromProto(sproto);
  ASSERT_TRUE(restored);
  // Simulate unwrapping successfully.
  restored->set_unexportable_key_id(session->unexportable_key_id());
  EXPECT_TRUE(restored->IsEqualForTesting(*session));
}

TEST_F(SessionTest, FailCreateFromInvalidProto) {
  // Empty proto.
  {
    proto::Session sproto;
    EXPECT_FALSE(Session::CreateFromProto(sproto));
  }

  // Create a fully populated proto.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateValidParams()));
  ASSERT_TRUE(session);
  proto::Session sproto = session->ToProto();

  // Missing fields.
  {
    proto::Session s(sproto);
    s.clear_id();
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
  {
    proto::Session s(sproto);
    s.clear_refresh_url();
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
  {
    proto::Session s(sproto);
    s.clear_should_defer_when_expired();
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
  {
    proto::Session s(sproto);
    s.clear_expiry_time();
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
  {
    proto::Session s(sproto);
    s.clear_session_inclusion_rules();
    EXPECT_FALSE(Session::CreateFromProto(s));
  }

  // Empty id.
  {
    proto::Session s(sproto);
    s.set_id("");
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
  // Invalid refresh URL.
  {
    proto::Session s(sproto);
    s.set_refresh_url("blank");
    EXPECT_FALSE(Session::CreateFromProto(s));
  }

  // Expired
  {
    proto::Session s(sproto);
    base::Time expiry_date = base::Time::Now() - base::Days(1);
    s.set_expiry_time(expiry_date.ToDeltaSinceWindowsEpoch().InMicroseconds());
    EXPECT_FALSE(Session::CreateFromProto(s));
  }

  // Invalid refresh initiator
  {
    proto::Session s(sproto);
    s.add_allowed_refresh_initiators("a.*.example.test");
    EXPECT_FALSE(Session::CreateFromProto(s));
  }
}

TEST_F(SessionTest, DeferredSession) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredAsExcluded) {
  auto params = CreateValidParams();
  SessionParams::Scope::Specification spec;
  spec.type = SessionParams::Scope::Specification::Type::kExclude;
  spec.domain = "example.test";
  spec.path = "/index.html";
  params.scope.specifications.push_back(spec);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  // The SessionService typically sets this once it starts looking for a
  // session on the same site as `request`.
  request->set_device_bound_session_usage(SessionUsage::kNoUsage);

  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(session->IsInScope(dbsc_request));
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, NotDeferredSubdomain) {
  const char subdomain[] = "https://test.example.test/index.html";
  const GURL url_subdomain(subdomain);
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  // The SessionService typically sets this once it starts looking for a
  // session on the same site as `request`.
  request->set_device_bound_session_usage(SessionUsage::kNoUsage);

  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(session->IsInScope(dbsc_request));
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, DeferredIncludedSubdomain) {
  // Since `include_site` is true, a different origin can be
  // matched when the spec includes it.
  const char subdomain[] = "https://test.example.test/index.html";
  const GURL url_subdomain(subdomain);
  auto params = CreateValidParams();
  SessionParams::Scope::Specification spec;
  spec.type = SessionParams::Scope::Specification::Type::kInclude;
  spec.domain = "test.example.test";
  spec.path = "/index.html";
  params.scope.specifications.push_back(spec);
  params.scope.include_site = true;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredWithCookieSession) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);

  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v;Secure; Domain=example.test", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_FALSE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  // Even though the second session didn't defer, the request was
  // deferred by the first session.
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredInsecure) {
  const char insecure_url[] = "http://example.test/index.html";
  const GURL test_insecure_url(insecure_url);
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      test_insecure_url, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  // The SessionService typically sets this once it starts looking for a
  // session on the same site as `request`.
  request->set_device_bound_session_usage(SessionUsage::kNoUsage);

  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(session->IsInScope(dbsc_request));
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, DeferredEmptyCookieAttributesCredentialsField) {
  auto params = CreateValidParams();
  // Set the credentials attributes field to an empty string. This will use
  // default cookie attributes.
  params.credentials = {SessionParams::Credential{"test_cookie",
                                                  /*attributes=*/""}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, DeferredNarrowerScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "https://sub.example.test";
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request matching the scope origin.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("https://sub.example.test/index.html"), IDLE,
                              &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredNarrowerScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "https://sub.example.test";
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request with a broader scope than the scope origin.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  // The SessionService typically sets this once it starts looking for a
  // session on the same site as `request`.
  request->set_device_bound_session_usage(SessionUsage::kNoUsage);

  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(session->IsInScope(dbsc_request));
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, DeferredMissingScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "";
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request matching the fetcher URL.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, DeferredAllowedRefreshInitiators) {
  auto params = CreateValidParams();
  params.allowed_refresh_initiators = {"*.not-example.test"};
  // We need a third-party cookie to be included on requests from other
  // initiators.
  params.credentials = {SessionParams::Credential{
      "test_cookie",
      /*attributes=*/"Secure; Domain=example.test; SameSite=None"}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request matching the fetcher URL.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Browser-initiated requests can always be deferred
  request->set_initiator(std::nullopt);
  DbscRequest dbsc_request(request.get());

  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());

  // Initiators on the site can always be deferred, despite no matching
  // initiator pattern.
  request->set_initiator(url::Origin::Create(GURL("https://example.test/")));
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());

  // Initiators matching the pattern can be deferred.
  request->set_initiator(
      url::Origin::Create(GURL("https://subdomain.not-example.test/")));
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());

  // Initiators not on the site or matching a rule cannot be deferred.
  request->set_initiator(
      url::Origin::Create(GURL("https://some-other-not-example.test/")));
  EXPECT_FALSE(session->IsInScope(dbsc_request));
}

class InsecureDelegate : public CookieAccessDelegate {
 public:
  bool ShouldTreatUrlAsTrustworthy(const GURL& url) const override {
    return true;
  }
  CookieAccessSemantics GetAccessSemantics(
      const CanonicalCookie& cookie) const override {
    return CookieAccessSemantics::UNKNOWN;
  }

  CookieScopeSemantics GetScopeSemantics(
      const std::string_view domain) const override {
    return CookieScopeSemantics::UNKNOWN;
  }
  // Returns whether a cookie should be attached regardless of its SameSite
  // value vs the request context.
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const SiteForCookies& site_for_cookies) const override {
    return true;
  }
  [[nodiscard]] std::optional<
      std::pair<FirstPartySetMetadata, FirstPartySetsCacheFilter::MatchInfo>>
  ComputeFirstPartySetMetadataMaybeAsync(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      base::OnceCallback<void(FirstPartySetMetadata,
                              FirstPartySetsCacheFilter::MatchInfo)> callback)
      const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>>
  FindFirstPartySetEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<
          void(base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>)>
          callback) const override {
    return std::nullopt;
  }
};

TEST_F(SessionTest, NotDeferredNotSameSiteForCookies) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_FALSE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(),
            SessionUsage::kInScopeNotDeferred);
}

TEST_F(SessionTest, DeferredNotSameSiteDelegate) {
  context_->cookie_store()->SetCookieAccessDelegate(
      std::make_unique<InsecureDelegate>());
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, DeferredHostCookie) {
  auto params = CreateValidParams();
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"__Host-test_cookie",
                                 "Secure; HttpOnly; Path=/"}});
  params.credentials = std::move(cookie_credentials);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredIncludedSubdomainHostCraving) {
  // Unless include site is specified, only same origin will be
  // matched even if the spec adds an include for a different
  // origin.
  const char subdomain[] = "https://test.example.test/index.html";
  const GURL url_subdomain(subdomain);
  auto params = CreateValidParams();
  SessionParams::Scope::Specification spec;
  spec.type = SessionParams::Scope::Specification::Type::kInclude;
  spec.domain = "test.example.test";
  spec.path = "/index.html";
  params.scope.specifications.push_back(spec);
  params.scope.include_site = true;
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie", "Secure;"}});
  params.credentials = std::move(cookie_credentials);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_FALSE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(request->device_bound_session_usage(),
            SessionUsage::kInScopeNotDeferred);
}

TEST_F(SessionTest, CreationDate) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateValidParams()));
  ASSERT_TRUE(session);
  // Make sure it's set to a plausible value.
  EXPECT_LT(base::Time::Now() - base::Days(1), session->creation_date());
}

TEST_F(SessionTest, NetLogSessionInfo) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  RecordingNetLogObserver net_log_observer;
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  EXPECT_EQ(
      net_log_observer.GetEntriesWithType(NetLogEventType::DBSC_REQUEST).size(),
      1u);
}

TEST_F(SessionTest, NetLogMissingCookie) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  RecordingNetLogObserver net_log_observer;
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_TRUE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(*entries[0].params.FindString("refresh_required_reason"),
            "missing_cookie");
}

TEST_F(SessionTest, NetLogNoRefresh) {
  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v;Secure; Domain=example.test", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  RecordingNetLogObserver net_log_observer;
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(session->IsInScope(dbsc_request));
  EXPECT_FALSE(
      session->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
          .is_zero());
  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(*entries[0].params.FindString("refresh_required_reason"),
            "refresh_not_required");
}

TEST_F(SessionTest, NetLogWrongInitiator) {
  auto params = CreateValidParams();
  params.allowed_refresh_initiators = {};
  // We need a third-party cookie to be included on requests from other
  // initiators.
  params.credentials = {SessionParams::Credential{
      "test_cookie",
      /*attributes=*/"Secure; Domain=example.test; SameSite=None"}};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_initiator(
      url::Origin::Create(GURL("https://not-example.test/")));

  RecordingNetLogObserver net_log_observer;
  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(session->IsInScope(dbsc_request));

  std::vector<NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(*entries[0].params.FindString("refresh_required_reason"),
            "refresh_not_allowed_for_initiator");
}

TEST_F(SessionTest, RefreshUrlExcludedFromSession) {
  auto params = CreateValidParams();

  // Make sure the refresh endpoint isn't explicitly excluded
  EXPECT_TRUE(params.scope.specifications.empty());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateValidParams()));
  ASSERT_TRUE(session);

  EXPECT_FALSE(session->IncludesUrl(kRefreshUrl));
}

TEST_F(SessionTest, Backoff) {
  using enum SessionError::ErrorType;

  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  struct TestCase {
    SessionError::ErrorType error_type;
    bool expect_backoff;
  };

  const TestCase kTestCases[] = {
      {kSuccess, /*expect_backoff=*/false},
      {kNetError, /*expect_backoff=*/false},
      {kTransientHttpError, /*expect_backoff=*/true},
      {kPersistentHttpError, /*expect_backoff=*/false}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Error type: " << static_cast<int>(test_case.error_type)
                 << (test_case.expect_backoff ? " should backoff"
                                              : " should not backoff"));
    // Reset the backoff state
    for (size_t i = 0; i < 4; i++) {
      session->InformOfRefreshResult(/*was_proactive=*/false, kSuccess);
    }
    FastForwardBy(base::Seconds(1));
    DbscRequest dbsc_request(request.get());
    EXPECT_TRUE(session->IsInScope(dbsc_request));
    EXPECT_TRUE(
        session
            ->MinimumBoundCookieLifetime(dbsc_request, FirstPartySetMetadata())
            .is_zero());

    // Four errors in a row will enter backoff, if necessary
    for (size_t i = 0; i < 4; i++) {
      session->InformOfRefreshResult(/*was_proactive=*/false,
                                     test_case.error_type);
    }

    EXPECT_EQ(session->ShouldBackoff(), test_case.expect_backoff);
  }
}

TEST_F(SessionTest, ProactiveBackoff) {
  using enum SessionError::ErrorType;

  auto params = CreateValidParams();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));
  ASSERT_TRUE(session);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Proactive refreshes can be followed up with other proactive refreshes
  session->InformOfRefreshResult(/*was_proactive=*/true, kSuccess);
  EXPECT_FALSE(session->attempted_proactive_refresh_since_last_success());

  // Deferring refreshes can be followed up with proactive refresh
  session->InformOfRefreshResult(/*was_proactive=*/false, kSuccess);
  EXPECT_FALSE(session->attempted_proactive_refresh_since_last_success());

  // A failed deferring refresh does not block proactive refresh
  session->InformOfRefreshResult(/*was_proactive=*/false, kNetError);
  EXPECT_FALSE(session->attempted_proactive_refresh_since_last_success());

  // A failed praoctive refresh blocks future proactive refreshes
  session->InformOfRefreshResult(/*was_proactive=*/true, kNetError);
  EXPECT_TRUE(session->attempted_proactive_refresh_since_last_success());

  // We're still blocked while deferring refreshes fail
  session->InformOfRefreshResult(/*was_proactive=*/false, kNetError);
  EXPECT_TRUE(session->attempted_proactive_refresh_since_last_success());

  // We only become unblocked when a deferring refresh succeeds
  session->InformOfRefreshResult(/*was_proactive=*/true, kSuccess);
  EXPECT_FALSE(session->attempted_proactive_refresh_since_last_success());
}

TEST_F(SessionTest, RefreshInitiators) {
  auto params = CreateValidParams();
  params.allowed_refresh_initiators = {"*.not-example.test"};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(params));

  ASSERT_EQ(session->allowed_refresh_initiators().size(), 1);

  const std::string& initiator_rule = session->allowed_refresh_initiators()[0];
  EXPECT_FALSE(MatchesHostPattern(initiator_rule,
                                  GURL("https://not-example.test").GetHost()));
  EXPECT_TRUE(MatchesHostPattern(
      initiator_rule, GURL("https://subdomain.not-example.test").GetHost()));
  EXPECT_FALSE(MatchesHostPattern(
      initiator_rule, GURL("https://some-other-example.test").GetHost()));
}

TEST_F(SessionTest, InvalidRefreshInitiators) {
  auto params = CreateValidParams();
  params.allowed_refresh_initiators = {"star.in.middle.*.of.example.test"};
  EXPECT_THAT(
      Session::CreateIfValid(params),
      ErrorIs(SessionError(SessionError::kRefreshInitiatorInvalidHostPattern)));
}

}  // namespace

}  // namespace net::device_bound_sessions
