// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDummyAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", "");

class SessionServiceImplTest : public TestWithTaskEnvironment {
 protected:
  SessionServiceImplTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()),
        service_(*UnexportableKeyServiceFactory::GetInstance()->GetShared(),
                 context_.get()) {}

  SessionServiceImpl& service() { return service_; }

  std::unique_ptr<URLRequestContext> context_;

 private:
  SessionServiceImpl service_;
};

class FakeDelegate : public URLRequest::Delegate {
  void OnReadCompleted(URLRequest* request, int bytes_read) override {}
};

// Variables to be used by TestFetcher
// Can be changed by tests
std::string g_session_id = "SessionId";
// Constant variables
constexpr char kUrlString[] = "https://example.com";
const GURL kTestUrl(kUrlString);

std::optional<RegistrationFetcher::RegistrationCompleteParams> TestFetcher() {
  std::vector<SessionParams::Credential> cookie_credentials;
  cookie_credentials.push_back(
      SessionParams::Credential{"test_cookie", "secure"});
  SessionParams::Scope scope;
  scope.include_site = true;
  SessionParams session_params(g_session_id, kUrlString, std::move(scope),
                               std::move(cookie_credentials));
  unexportable_keys::UnexportableKeyId key_id;
  return RegistrationFetcher::RegistrationCompleteParams(
      std::move(session_params), std::move(key_id), kTestUrl);
}

class ScopedTestFetcher {
 public:
  ScopedTestFetcher() {
    RegistrationFetcher::SetFetcherForTesting(TestFetcher);
  }
  ~ScopedTestFetcher() { RegistrationFetcher::SetFetcherForTesting(nullptr); }
};

std::optional<RegistrationFetcher::RegistrationCompleteParams> NullFetcher() {
  return std::nullopt;
}

class ScopedNullFetcher {
 public:
  ScopedNullFetcher() {
    RegistrationFetcher::SetFetcherForTesting(NullFetcher);
  }
  ~ScopedNullFetcher() { RegistrationFetcher::SetFetcherForTesting(nullptr); }
};

// Not implemented so test just makes sure it can run
TEST_F(SessionServiceImplTest, TestDefer) {
  SessionService::RefreshCompleteCallback cb1 = base::DoNothing();
  SessionService::RefreshCompleteCallback cb2 = base::DoNothing();
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  service().DeferRequestForRefresh(request.get(), Session::Id("test"),
                                   std::move(cb1), std::move(cb2));
}

TEST_F(SessionServiceImplTest, RegisterSuccess) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  ASSERT_TRUE(maybe_id);
  EXPECT_EQ(**maybe_id, g_session_id);
}

TEST_F(SessionServiceImplTest, RegisterNoId) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  // g_session_id is empty, so should not be valid
  EXPECT_FALSE(maybe_id);
}

TEST_F(SessionServiceImplTest, RegisterNullFetcher) {
  ScopedNullFetcher scopedNullFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      kTestUrl, IDLE, new FakeDelegate(), kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  // NullFetcher, so should not be valid
  EXPECT_FALSE(maybe_id);
}

TEST_F(SessionServiceImplTest, SetChallengeForBoundSession) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader("Sec-Session-Challenge",
                     "\"challenge\";id=\"SessionId\", "
                     "\"challenge1\";id=\"NonExisted\"");
  headers->AddHeader("Sec-Session-Challenge", "\"challenge2\"");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());

  EXPECT_EQ(params.size(), 3U);

  for (const auto& param : params) {
    service().SetChallengeForBoundSession(kTestUrl, param);
  }

  const Session* session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), g_session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), "challenge");

  session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), "NonExisted");
  ASSERT_FALSE(session);
}

}  // namespace
}  // namespace net::device_bound_sessions
