// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/test/test_future.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/device_bound_sessions/mock_session_store.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

namespace net::device_bound_sessions {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDummyAnnotation =
    net::DefineNetworkTrafficAnnotation("dbsc_registration", "");

// Constant variables
constexpr char kUrlString[] = "https://example.com";
const GURL kTestUrl(kUrlString);
const std::string kSessionId = "SessionId";
const std::string kChallenge = "challenge";

// Matcher for SessionKeys
auto ExpectId(std::string_view id) {
  return testing::Field(&SessionKey::id, Session::Id(std::string(id)));
}

std::optional<RegistrationFetcher::RegistrationCompleteParams> TestFetcher(
    std::string session_id,
    std::optional<std::string> referral_session_identifier) {
  std::vector<SessionParams::Credential> cookie_credentials;
  cookie_credentials.push_back(
      SessionParams::Credential{"test_cookie", "secure"});
  SessionParams::Scope scope;
  scope.include_site = true;
  SessionParams session_params(std::move(session_id), kUrlString,
                               std::move(scope), std::move(cookie_credentials));
  unexportable_keys::UnexportableKeyId key_id;
  return std::make_optional<RegistrationFetcher::RegistrationCompleteParams>(
      std::move(session_params), std::move(key_id), kTestUrl,
      std::move(referral_session_identifier));
}

std::optional<RegistrationFetcher::RegistrationCompleteParams> NullFetcher() {
  return std::nullopt;
}

RegistrationFetcher::FetcherType TestFetcherFactory(
    std::string session_id,
    std::optional<std::string> referral_session_identifier) {
  static std::string g_session_id;
  static std::optional<std::string> g_referral_session_id;
  g_session_id = std::move(session_id);
  g_referral_session_id = std::move(referral_session_identifier);

  return []() { return TestFetcher(g_session_id, g_referral_session_id); };
}

class ScopedTestFetcher {
 public:
  explicit ScopedTestFetcher(
      std::string session_id,
      std::optional<std::string> referral_session_identifier = std::nullopt) {
    RegistrationFetcher::SetFetcherForTesting(TestFetcherFactory(
        std::move(session_id), std::move(referral_session_identifier)));
  }

  ~ScopedTestFetcher() { RegistrationFetcher::SetFetcherForTesting(nullptr); }
};

class ScopedNullFetcher {
 public:
  ScopedNullFetcher() {
    RegistrationFetcher::SetFetcherForTesting(NullFetcher);
  }
  ~ScopedNullFetcher() { RegistrationFetcher::SetFetcherForTesting(nullptr); }
};

class SessionServiceImplTest : public TestWithTaskEnvironment {
 public:
  SessionServiceImplTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()),
        service_(*UnexportableKeyServiceFactory::GetInstance()->GetShared(),
                 context_.get(),
                 /*store=*/nullptr) {}

  URLRequestContext* context() { return context_.get(); }
  SessionServiceImpl& service() { return service_; }

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  std::unique_ptr<URLRequestContext> context_;
  SessionServiceImpl service_;
};

// Not implemented so test just makes sure it can run
TEST_F(SessionServiceImplTest, TestDefer) {
  SessionService::RefreshCompleteCallback cb1 = base::DoNothing();
  SessionService::RefreshCompleteCallback cb2 = base::DoNothing();
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  service().DeferRequestForRefresh(request.get(), Session::Id("test"),
                                   std::move(cb1), std::move(cb2));
}

TEST_F(SessionServiceImplTest, RegisterSuccess) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  ASSERT_TRUE(maybe_id);
  EXPECT_EQ(**maybe_id, kSessionId);
}

TEST_F(SessionServiceImplTest, RegisterNoId) {
  ScopedTestFetcher scoped_test_fetcher(/*session_id=*/"");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  // session_id is empty, so should not be valid
  EXPECT_FALSE(maybe_id);
}

TEST_F(SessionServiceImplTest, RegisterNullFetcher) {
  ScopedNullFetcher scopedNullFetcher;
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  // NullFetcher, so should not be valid
  EXPECT_FALSE(maybe_id);
}

TEST_F(SessionServiceImplTest, SetChallengeForBoundSession) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(
      "Sec-Session-Challenge",
      R"("challenge";id="SessionId", "challenge1";id="NonExisted")");
  headers->AddHeader("Sec-Session-Challenge", R"("challenge2")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());

  EXPECT_EQ(params.size(), 3U);

  for (const auto& param : params) {
    service().SetChallengeForBoundSession(base::DoNothing(), kTestUrl, param);
  }

  const Session* session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), kSessionId);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), "challenge");

  session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), "NonExisted");
  ASSERT_FALSE(session);
}

TEST_F(SessionServiceImplTest, ExpiryExtendedOnUser) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  Session* session =
      service().GetSessionForTesting(SchemefulSite(kTestUrl), kSessionId);
  ASSERT_TRUE(session);
  session->set_expiry_date(base::Time::Now() + base::Days(1));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  service().GetAnySessionRequiringDeferral(request.get());

  EXPECT_GT(session->expiry_date(), base::Time::Now() + base::Days(399));
}

TEST_F(SessionServiceImplTest, NullAccessObserver) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(SessionService::OnAccessCallback(),
                                 std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  // The access observer was null, so no call is expected
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnRegistration) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  base::test::TestFuture<SessionKey> future;
  service().RegisterBoundSession(
      future.GetRepeatingCallback<const SessionKey&>(), std::move(fetch_param),
      IsolationInfo::CreateTransient());

  SessionKey session_key = future.Take();
  EXPECT_EQ(session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(session_key.id.value(), kSessionId);
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnDeferral) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
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
  EXPECT_EQ(session_key.id.value(), kSessionId);
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnSetChallenge) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);

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
  EXPECT_EQ(session_key.id.value(), kSessionId);
}

TEST_F(SessionServiceImplTest, ReferralSessionIdentifier) {
  // Register a session with Id `kSessionId`.
  {
    ScopedTestFetcher scoped_test_fetcher(kSessionId);

    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                   IsolationInfo::CreateTransient());
  }

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSessionForTesting(site, kSessionId));

  // Register a session with new Id to replace the former session.
  std::string session_id("NewSessionId");
  {
    ScopedTestFetcher scoped_test_fetcher(session_id, kSessionId);

    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                   IsolationInfo::CreateTransient());
  }

  ASSERT_TRUE(service().GetSessionForTesting(site, session_id));
  ASSERT_FALSE(service().GetSessionForTesting(site, kSessionId));
}

TEST_F(SessionServiceImplTest, GetAllSessions) {
  const std::string session_id_1("SessionId");
  {
    ScopedTestFetcher scoped_test_fetcher(session_id_1);

    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                   IsolationInfo::CreateTransient());
  }

  const std::string session_id_2("SessionId2");
  {
    ScopedTestFetcher scoped_test_fetcher(session_id_2);

    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                   IsolationInfo::CreateTransient());
  }

  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId(session_id_1),
                                                  ExpectId(session_id_2)));
}

TEST_F(SessionServiceImplTest, DeleteSession) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  ASSERT_TRUE(
      service().GetSessionForTesting(SchemefulSite(kTestUrl), kSessionId));

  service().DeleteSession(SchemefulSite(kTestUrl), Session::Id(kSessionId));

  EXPECT_FALSE(
      service().GetSessionForTesting(SchemefulSite(kTestUrl), kSessionId));
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

  void FinishLoadingSessions(SessionStore::SessionsMap loaded_sessions) {
    service().OnLoadSessionsComplete(std::move(loaded_sessions));
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

  ScopedTestFetcher scoped_test_fetcher(kSessionId);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  // Will invoke the store's save session method.
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient());

  auto site = SchemefulSite(kTestUrl);
  Session* session = service().GetSessionForTesting(site, kSessionId);
  ASSERT_TRUE(session);
  EXPECT_EQ(GetSiteSessionsCount(site), 1u);
  session->set_expiry_date(base::Time::Now() - base::Days(1));
  // Will invoke the store's delete session method.
  EXPECT_EQ(GetSiteSessionsCount(site), 0u);
}

TEST_F(SessionServiceImplWithStoreTest, GetAllSessionsWaitsForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Request sessions, which should wait until we finish loading.
  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());

  std::unique_ptr<Session> session = Session::CreateIfValid(
      SessionParams("session_id", "https://example.com/refresh", /*scope=*/{},
                    /*creds=*/{}),
      kTestUrl);
  ASSERT_TRUE(session);

  // Complete loading. If we did not defer, we'd miss this session.
  SessionStore::SessionsMap session_map;
  session_map.insert({SchemefulSite(kTestUrl), std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // But we did defer, so we found it.
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId("session_id")));
}

}  // namespace net::device_bound_sessions
