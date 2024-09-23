// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session.h"

#include "base/test/bind.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_util.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

class SessionTest : public TestWithTaskEnvironment {
 protected:
  SessionTest() : context_(CreateTestURLRequestContextBuilder()->Build()) {}

  std::unique_ptr<URLRequestContext> context_;
};

class FakeDelegate : public URLRequest::Delegate {
  void OnReadCompleted(URLRequest* request, int bytes_read) override {}
};

constexpr net::NetworkTrafficAnnotationTag kDummyAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", "");
constexpr char kSessionId[] = "SessionId";
constexpr char kUrlString[] = "https://example.test/index.html";
const GURL kTestUrl(kUrlString);

SessionParams CreateValidParams() {
  SessionParams::Scope scope;
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie",
                                 "Secure; Domain=example.test"}});
  SessionParams params{kSessionId, kUrlString, std::move(scope),
                       std::move(cookie_credentials)};
  return params;
}

TEST_F(SessionTest, ValidService) {
  auto session = Session::CreateIfValid(CreateValidParams(), kTestUrl);
  EXPECT_TRUE(session);
}

TEST_F(SessionTest, InvalidServiceRefreshUrl) {
  auto params = CreateValidParams();
  params.refresh_url = "";
  EXPECT_FALSE(Session::CreateIfValid(params, kTestUrl));
}

TEST_F(SessionTest, ToFromProto) {
  std::unique_ptr<Session> session =
      Session::CreateIfValid(CreateValidParams(), kTestUrl);
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
  EXPECT_TRUE(restored->IsEqualForTesting(*session));
}

TEST_F(SessionTest, FailCreateFromInvalidProto) {
  // Empty proto.
  {
    proto::Session sproto;
    EXPECT_FALSE(Session::CreateFromProto(sproto));
  }

  // Create a fully populated proto.
  std::unique_ptr<Session> session =
      Session::CreateIfValid(CreateValidParams(), kTestUrl);
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
}

TEST_F(SessionTest, DeferredSession) {
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_TRUE(is_deferred);
}

TEST_F(SessionTest, NotDeferredAsExcluded) {
  auto params = CreateValidParams();
  SessionParams::Scope::Specification spec;
  spec.type = SessionParams::Scope::Specification::Type::kExclude;
  spec.domain = "example.test";
  spec.path = "/index.html";
  params.scope.specifications.push_back(spec);
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_FALSE(is_deferred);
}

TEST_F(SessionTest, NotDeferredSubdomain) {
  const char subdomain[] = "https://test.example.test/index.html";
  const GURL url_subdomain(subdomain);
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      url_subdomain, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_FALSE(is_deferred);
}

TEST_F(SessionTest, DeferredIncludedSubdomain) {
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
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      url_subdomain, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  ASSERT_TRUE(session->ShouldDeferRequest(request.get()));
}

TEST_F(SessionTest, NotDeferredWithCookieSession) {
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_TRUE(is_deferred);

  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v;Secure; Domain=example.test", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  EXPECT_FALSE(session->ShouldDeferRequest(request.get()));
}

TEST_F(SessionTest, NotDeferredInsecure) {
  const char insecure_url[] = "http://example.test/index.html";
  const GURL test_insecure_url(insecure_url);
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      test_insecure_url, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_FALSE(is_deferred);
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

TEST_F(SessionTest, NotDeferredNotSameSite) {
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_FALSE(is_deferred);
}

TEST_F(SessionTest, DeferredNotSameSiteDelegate) {
  context_->cookie_store()->SetCookieAccessDelegate(
      std::make_unique<InsecureDelegate>());
  auto params = CreateValidParams();
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);

  bool is_deferred = session->ShouldDeferRequest(request.get());
  EXPECT_TRUE(is_deferred);
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
  std::vector<SessionParams::Credential> cookie_credentials(
      {SessionParams::Credential{"test_cookie", "Secure;"}});
  params.credentials = std::move(cookie_credentials);
  auto session = Session::CreateIfValid(params, kTestUrl);
  ASSERT_TRUE(session);
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      url_subdomain, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(url_subdomain));
  ASSERT_FALSE(session->ShouldDeferRequest(request.get()));
}

}  // namespace

}  // namespace net::device_bound_sessions
