// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session.h"

#include <string_view>

#include "base/test/bind.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

class SessionTest : public ::testing::Test, public WithTaskEnvironment {
 protected:
  SessionTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()) {}

  std::unique_ptr<URLRequestContext> context_;
};

class FakeDelegate : public URLRequest::Delegate {
  void OnReadCompleted(URLRequest* request, int bytes_read) override {}
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
                       unexportable_keys::UnexportableKeyId()};
}

TEST_F(SessionTest, ValidService) {
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  EXPECT_TRUE(session);
}

TEST_F(SessionTest, DefaultExpiry) {
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  EXPECT_LT(base::Time::Now() + base::Days(399), session->expiry_date());
}

TEST_F(SessionTest, RelativeServiceRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = "/internal/RefreshSession";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);

  // Validate session refresh URL.
  EXPECT_EQ(session->refresh_url().spec(),
            "https://example.test/internal/RefreshSession");
}

TEST_F(SessionTest, RelativeServiceRefreshUrlEscaped) {
  auto params = CreateValidParams();
  params.refresh_url = "/internal%26RefreshSession";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);

  // Validate session refresh URL.
  EXPECT_EQ(session->refresh_url().spec(),
            "https://example.test/internal&RefreshSession");
}

TEST_F(SessionTest, InvalidServiceRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = "";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_FALSE(session_or_error.has_value());
  EXPECT_EQ(session_or_error.error().type,
            SessionError::ErrorType::kInvalidRefreshUrl);
}

TEST_F(SessionTest, InvalidScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "hello world";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_FALSE(session_or_error.has_value());
  EXPECT_EQ(session_or_error.error().type,
            SessionError::ErrorType::kInvalidScopeOrigin);
}

TEST_F(SessionTest, ScopeOriginSameSiteMismatch) {
  auto params = CreateValidParams();
  params.fetcher_url = kTestUrlForWrongETLD;
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_FALSE(session_or_error.has_value());
  EXPECT_EQ(session_or_error.error().type,
            SessionError::ErrorType::kScopeOriginSameSiteMismatch);
}

TEST_F(SessionTest, ScopeOriginPrivateRegistryChildDomainSameSiteMismatch) {
  // Since appspot.com is on the Public Suffix List (with private registries
  // included), it should not be possible for any of its child domains to
  // create a session on the parent domain.
  auto params = CreateValidParams();
  params.fetcher_url = GURL("https://example.appspot.com/refresh");
  params.refresh_url = "https://example.appspot.com/refresh";
  params.scope.origin = "https://appspot.com";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_FALSE(session_or_error.has_value());
  EXPECT_EQ(session_or_error.error().type,
            SessionError::ErrorType::kScopeOriginSameSiteMismatch);
}

TEST_F(SessionTest, SameSiteMismatchRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = kUrlStringForWrongETLD;
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_FALSE(session_or_error.has_value());
  EXPECT_EQ(session_or_error.error().type,
            SessionError::ErrorType::kRefreshUrlSameSiteMismatch);
}

TEST_F(SessionTest, NonSecureUrl) {
  // HTTP is not allowed for the refresh URL.
  {
    auto params = CreateValidParams();
    params.fetcher_url = GURL("http://example.test/index.html");
    params.refresh_url = "http://example.test/registration";
    params.scope.origin = "http://example.test";
    auto session_or_error = Session::CreateIfValid(params);
    ASSERT_FALSE(session_or_error.has_value());
    EXPECT_EQ(session_or_error.error().type,
              SessionError::ErrorType::kInvalidRefreshUrl);
  }

  // But localhost is okay.
  {
    auto params = CreateValidParams();
    params.fetcher_url = GURL("http://localhost:8080/index.html");
    params.refresh_url = "http://localhost:8080/registration";
    params.scope.origin = "http://localhost:8080";
    EXPECT_TRUE(Session::CreateIfValid(params).has_value());
  }
}

TEST_F(SessionTest, ToFromProto) {
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
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
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
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
}

TEST_F(SessionTest, DeferredSession) {
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredAsExcluded) {
  auto params = CreateValidParams();
  SessionParams::Scope::Specification spec;
  spec.type = SessionParams::Scope::Specification::Type::kExclude;
  spec.domain = "example.test";
  spec.path = "/index.html";
  params.scope.specifications.push_back(spec);
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, NotDeferredSubdomain) {
  const char subdomain[] = "https://test.example.test/index.html";
  const GURL url_subdomain(subdomain);
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, NotDeferredIncludedSubdomain) {
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
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  EXPECT_FALSE(
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()));
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
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  EXPECT_TRUE(
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()));
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredWithCookieSession) {
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);

  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v;Secure; Domain=example.test", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  EXPECT_FALSE(
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()));
  // Even though the second session didn't defer, the request was
  // deferred by the first session.
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredInsecure) {
  const char insecure_url[] = "http://example.test/index.html";
  const GURL test_insecure_url(insecure_url);
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      test_insecure_url, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, DeferredEmptyCookieAttributesCredentialsField) {
  auto params = CreateValidParams();
  // Set the credentials attributes field to an empty string. This will use
  // default cookie attributes.
  params.credentials = {SessionParams::Credential{"test_cookie",
                                                  /*attributes=*/""}};
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, DeferredNarrowerScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "https://sub.example.test";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request matching the scope origin.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("https://sub.example.test/index.html"), IDLE,
                              &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

TEST_F(SessionTest, NotDeferredNarrowerScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "https://sub.example.test";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request with a broader scope than the scope origin.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);
}

TEST_F(SessionTest, DeferredMissingScopeOrigin) {
  auto params = CreateValidParams();
  params.scope.origin = "";
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  // Create a request matching the fetcher URL.
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
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
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(is_deferred);
  EXPECT_EQ(request->device_bound_session_usage(),
            SessionUsage::kInScopeNotDeferred);
}

TEST_F(SessionTest, DeferredNotSameSiteDelegate) {
  context_->cookie_store()->SetCookieAccessDelegate(
      std::make_unique<InsecureDelegate>());
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  bool is_deferred =
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_TRUE(is_deferred);
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
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(url_subdomain, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  EXPECT_FALSE(
      session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()));
  EXPECT_EQ(request->device_bound_session_usage(),
            SessionUsage::kInScopeNotDeferred);
}

TEST_F(SessionTest, CreationDate) {
  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  // Make sure it's set to a plausible value.
  EXPECT_LT(base::Time::Now() - base::Days(1), session->creation_date());
}

TEST_F(SessionTest, NetLogSessionInfo) {
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  RecordingNetLogObserver net_log_observer;
  session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_EQ(
      net_log_observer.GetEntriesWithType(NetLogEventType::DBSC_REQUEST).size(),
      1u);
}

TEST_F(SessionTest, NetLogMissingCookie) {
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  RecordingNetLogObserver net_log_observer;
  session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_EQ(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED)
          .size(),
      1u);
}

TEST_F(SessionTest, NetLogNoRefresh) {
  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
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
  session->ShouldDeferRequest(request.get(), FirstPartySetMetadata());
  EXPECT_EQ(
      net_log_observer
          .GetEntriesWithType(NetLogEventType::CHECK_DBSC_REFRESH_REQUIRED)
          .size(),
      1u);
}

TEST_F(SessionTest, RefreshUrlExcludedFromSession) {
  auto params = CreateValidParams();

  // Make sure the refresh endpoint isn't explicitly excluded
  EXPECT_TRUE(params.scope.specifications.empty());

  auto session_or_error = Session::CreateIfValid(CreateValidParams());
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);

  EXPECT_FALSE(session->IncludesUrl(kRefreshUrl));
}

TEST_F(SessionTest, Backoff) {
  using enum SessionError::ErrorType;

  auto params = CreateValidParams();
  auto session_or_error = Session::CreateIfValid(params);
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
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
      session->InformOfRefreshResult(kSuccess);
    }
    FastForwardBy(base::Seconds(1));
    EXPECT_TRUE(
        session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()));

    // Four errors in a row will enter backoff, if necessary
    for (size_t i = 0; i < 4; i++) {
      session->InformOfRefreshResult(test_case.error_type);
    }

    EXPECT_EQ(
        session->ShouldDeferRequest(request.get(), FirstPartySetMetadata()),
        !test_case.expect_backoff);
  }
}

}  // namespace

}  // namespace net::device_bound_sessions
