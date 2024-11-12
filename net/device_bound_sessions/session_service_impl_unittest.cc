// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/test/test_future.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/device_bound_sessions/test_util.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace net::device_bound_sessions {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDummyAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", "");

class SessionServiceImplTest : public TestWithTaskEnvironment {
 protected:
  SessionServiceImplTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()),
        service_(*UnexportableKeyServiceFactory::GetInstance()->GetShared(),
                 context_.get(),
                 /*store=*/nullptr) {}

  SessionServiceImpl& service() { return service_; }

  std::unique_ptr<URLRequestContext> context_;

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  SessionServiceImpl service_;
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
  return std::make_optional<RegistrationFetcher::RegistrationCompleteParams>(
      std::move(session_params), std::move(key_id), kTestUrl, std::nullopt);
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
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
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
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

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
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

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
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

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
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
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
    service().SetChallengeForBoundSession(base::DoNothing(), kTestUrl, param);
  }

  const Session* session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), g_session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), "challenge");

  session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), "NonExisted");
  ASSERT_FALSE(session);
}

TEST_F(SessionServiceImplTest, ExpiryExtendedOnUser) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  Session* session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), g_session_id);
  ASSERT_TRUE(session);
  session->set_expiry_date(base::Time::Now() + base::Days(1));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  service().GetAnySessionRequiringDeferral(request.get());

  EXPECT_GT(session->expiry_date(), base::Time::Now() + base::Days(399));
}

TEST_F(SessionServiceImplTest, NullAccessObserver) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(SessionService::OnAccessCallback(),
                                 std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  // The access observer was null, so no call is expected
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnRegistration) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  base::test::TestFuture<SessionKey> future;
  service().RegisterBoundSession(
      future.GetRepeatingCallback<const SessionKey&>(), std::move(fetch_param),
      IsolationInfo::CreateTransient());

  SessionKey session_key = future.Take();
  EXPECT_EQ(session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(session_key.id.value(), g_session_id);
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnDeferral) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<SessionKey> future;
  request->SetDeviceBoundSessionAccessCallback(
      future.GetRepeatingCallback<const SessionKey&>());
  service().GetAnySessionRequiringDeferral(request.get());

  SessionKey session_key = future.Take();
  EXPECT_EQ(session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(session_key.id.value(), g_session_id);
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnSetChallenge) {
  // Set the session id to be used for in TestFetcher()
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader("Sec-Session-Challenge", "\"challenge\";id=\"SessionId\"");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1U);

  base::test::TestFuture<SessionKey> future;
  service().SetChallengeForBoundSession(
      future.GetRepeatingCallback<const SessionKey&>(), kTestUrl, params[0]);

  SessionKey session_key = future.Take();
  EXPECT_EQ(session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(session_key.id.value(), g_session_id);
}

}  // namespace

class SessionServiceImplWithStoreTest : public TestWithTaskEnvironment {
 public:
  SessionServiceImplWithStoreTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()),
        store_(std::make_unique<StrictMock<SessionStoreMock>>()),
        service_(*UnexportableKeyServiceFactory::GetInstance()->GetShared(),
                 context_.get(),
                 store_.get()) {}

  SessionServiceImpl& service() { return service_; }
  StrictMock<SessionStoreMock>& store() { return *store_; }

  void OnSessionsLoaded() {
    service().OnLoadSessionsComplete(SessionStore::SessionsMap());
  }

  size_t GetSiteSessionsCount(const SchemefulSite& site) {
    auto [begin, end] = service().GetSessionsForSite(site);
    return std::distance(begin, end);
  }

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<StrictMock<SessionStoreMock>> store_;
  SessionServiceImpl service_;
};

TEST_F(SessionServiceImplWithStoreTest, UsesSessionStore) {
  {
    InSequence seq;
    EXPECT_CALL(store(), LoadSessions)
        .Times(1)
        .WillOnce(
            Invoke(this, &SessionServiceImplWithStoreTest::OnSessionsLoaded));
    EXPECT_CALL(store(), SaveSession).Times(1);
    EXPECT_CALL(store(), DeleteSession).Times(1);
  }

  // Will invoke the store's load session method.
  service().LoadSessionsAsync();

  // Set the session id to be used in TestFetcher().
  g_session_id = "SessionId";
  ScopedTestFetcher scopedTestFetcher;
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  // Will invoke the store's save session method.
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  auto site = SchemefulSite(kTestUrl);
  Session* session = service().GetSessionForTesting(site, g_session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(GetSiteSessionsCount(site), 1u);
  session->set_expiry_date(base::Time::Now() - base::Days(1));
  // Will invoke the store's delete session method.
  EXPECT_EQ(GetSiteSessionsCount(site), 0u);
}

}  // namespace net::device_bound_sessions
