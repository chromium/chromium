// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/test/test_future.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/device_bound_sessions/mock_session_store.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
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
const std::string kOrigin = "example.com";

constexpr char kUrlString2[] = "https://example2.com";
const GURL kTestUrl2(kUrlString2);
const std::string kSessionId2 = "SessionId2";
const std::string kOrigin2 = "example2.com";

const std::string kChallenge = "challenge";

// Matcher for SessionKeys
auto ExpectId(std::string_view id) {
  return testing::Field(&SessionKey::id, Session::Id(std::string(id)));
}

std::optional<RegistrationFetcher::RegistrationCompleteParams> TestFetcher(
    std::string session_id,
    std::string url_string,
    std::string origin) {
  std::vector<SessionParams::Credential> cookie_credentials;
  cookie_credentials.push_back(
      SessionParams::Credential{"test_cookie", "secure"});
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.origin = origin;
  SessionParams session_params(std::move(session_id), url_string,
                               std::move(scope), std::move(cookie_credentials));
  unexportable_keys::UnexportableKeyId key_id;
  return std::make_optional<RegistrationFetcher::RegistrationCompleteParams>(
      std::move(session_params), std::move(key_id), GURL(url_string));
}

std::optional<RegistrationFetcher::RegistrationCompleteParams> NullFetcher() {
  return std::nullopt;
}

std::optional<RegistrationFetcher::RegistrationCompleteParams>
ContinueFalseFetcher() {
  unexportable_keys::UnexportableKeyId key_id;
  return std::make_optional<RegistrationFetcher::RegistrationCompleteParams>(
      SessionTerminationParams{kSessionId}, std::move(key_id), kTestUrl);
}

RegistrationFetcher::FetcherType TestFetcherFactory(std::string session_id,
                                                    std::string url_string,
                                                    std::string origin) {
  static std::string g_session_id;
  static std::string g_url_string;
  static std::string g_origin;
  g_session_id = std::move(session_id);
  g_url_string = std::move(url_string);
  g_origin = std::move(origin);

  return []() { return TestFetcher(g_session_id, g_url_string, g_origin); };
}

class ScopedTestFetcher {
 public:
  ScopedTestFetcher(std::string session_id,
                    std::string url_string,
                    std::string origin) {
    // Reset the testing fetch function.
    RegistrationFetcher::SetFetcherForTesting(nullptr);
    RegistrationFetcher::SetFetcherForTesting(TestFetcherFactory(
        std::move(session_id), std::move(url_string), std::move(origin)));
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

class ScopedContinueFalseFetcher {
 public:
  ScopedContinueFalseFetcher() {
    RegistrationFetcher::SetFetcherForTesting(ContinueFalseFetcher);
  }

  ~ScopedContinueFalseFetcher() {
    RegistrationFetcher::SetFetcherForTesting(nullptr);
  }
};

class TestDeferCompletion {
 public:
  enum class CallbackType { kRestart, kContinue };

  explicit TestDeferCompletion(base::OnceCallback<void(CallbackType)> callback)
      : completion_callback_(std::move(callback)) {}
  ~TestDeferCompletion() = default;

  SessionService::RefreshCompleteCallback GetRestartCb() {
    return base::BindOnce(&TestDeferCompletion::RestartRequest,
                          weak_factory_.GetWeakPtr());
  }

  SessionService::RefreshCompleteCallback GetContinueCb() {
    return base::BindOnce(&TestDeferCompletion::ContinueRequest,
                          weak_factory_.GetWeakPtr());
  }

  void RestartRequest() {
    std::move(completion_callback_).Run(CallbackType::kRestart);
  }
  void ContinueRequest() {
    std::move(completion_callback_).Run(CallbackType::kContinue);
  }

 private:
  base::OnceCallback<void(CallbackType)> completion_callback_;
  base::WeakPtrFactory<TestDeferCompletion> weak_factory_{this};
};

class FakeDeviceBoundSessionObserver {
 public:
  const std::vector<SessionKey>& notifications() const {
    return notifications_;
  }

  void ClearNotifications() { notifications_.clear(); }

  void OnSessionAccessed(const SessionKey& session_key) {
    notifications_.push_back(session_key);
  }

  SessionService::OnAccessCallback GetCallback() {
    return base::BindRepeating(
        &FakeDeviceBoundSessionObserver::OnSessionAccessed,
        base::Unretained(this));
  }

 private:
  std::vector<SessionKey> notifications_;
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

  // Take list of <session_id, site_url> to add sessions for testing.
  void AddSessionsForTesting(
      std::vector<std::tuple<std::string, std::string, std::string>>
          id_url_origin_list) {
    for (const auto& [id, url_str, origin] : id_url_origin_list) {
      ScopedTestFetcher scoped_test_fetcher(id, url_str, origin);
      auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
          GURL(url_str),
          {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
          "challenge", /*authorization=*/std::nullopt);
      service_.RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                    IsolationInfo::CreateTransient(),
                                    NetLogWithSource());
    }
  }

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  std::unique_ptr<URLRequestContext> context_;
  SessionServiceImpl service_;
};

TEST_F(SessionServiceImplTest, RegisterSuccess) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

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
  AddSessionsForTesting({{/*session_id=*/"", kUrlString, kOrigin}});

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
                                 IsolationInfo::CreateTransient(),
                                 NetLogWithSource());

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
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

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
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId));
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), "challenge");

  session =
      service().GetSession(SchemefulSite(kTestUrl), Session::Id("NonExisted"));
  ASSERT_FALSE(session);
}

TEST_F(SessionServiceImplTest, ExpiryExtendedOnUser) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  Session* session =
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId));
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
  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(), NetLogWithSource());

  // The access observer was null, so no call is expected
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnRegistration) {
  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  base::test::TestFuture<SessionKey> future;
  service().RegisterBoundSession(
      future.GetRepeatingCallback<const SessionKey&>(), std::move(fetch_param),
      IsolationInfo::CreateTransient(), NetLogWithSource());

  SessionKey session_key = future.Take();
  EXPECT_EQ(session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(session_key.id.value(), kSessionId);
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnDeferral) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

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
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

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

TEST_F(SessionServiceImplTest, GetAllSessions) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin},
                         {kSessionId2, kUrlString2, kOrigin2}});

  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId(kSessionId),
                                                  ExpectId(kSessionId2)));
}

TEST_F(SessionServiceImplTest, DeleteSession) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});
  auto site = SchemefulSite(kTestUrl);
  auto session_id = Session::Id(kSessionId);

  ASSERT_TRUE(service().GetSession(site, session_id));

  service().DeleteSession(site, session_id);

  EXPECT_FALSE(service().GetSession(site, session_id));
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsByCreationTime) {
  net::SchemefulSite site(kTestUrl);

  AddSessionsForTesting({{"SessionA", kUrlString, kOrigin},
                         {"SessionB", kUrlString, kOrigin},
                         {"SessionC", kUrlString, kOrigin}});

  service()
      .GetSession(site, Session::Id("SessionA"))
      ->set_creation_date(base::Time::Now() - base::Days(6));
  service()
      .GetSession(site, Session::Id("SessionB"))
      ->set_creation_date(base::Time::Now() - base::Days(4));
  service()
      .GetSession(site, Session::Id("SessionC"))
      ->set_creation_date(base::Time::Now() - base::Days(2));

  base::RunLoop run_loop;
  service().DeleteAllSessions(base::Time::Now() - base::Days(5),
                              base::Time::Now() - base::Days(3),
                              /*site_matcher=*/
                              base::NullCallback(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(service().GetSession(site, Session::Id("SessionA")));
  EXPECT_FALSE(service().GetSession(site, Session::Id("SessionB")));
  EXPECT_TRUE(service().GetSession(site, Session::Id("SessionC")));
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsBySite) {
  GURL url_a("https://a_example.com");
  GURL url_b("https://b_example.com");

  AddSessionsForTesting({{kSessionId, url_a.spec(), "a_example.com"},
                         {kSessionId, url_b.spec(), "b_example.com"}});

  SchemefulSite site_a(url_a);
  SchemefulSite site_b(url_b);

  // `site_matcher` only matches `site_a`
  base::RepeatingCallback<bool(const net::SchemefulSite&)> site_matcher =
      base::BindRepeating(std::equal_to<net::SchemefulSite>(), site_a);

  base::RunLoop run_loop;
  service().DeleteAllSessions(
      /*created_after_time=*/std::nullopt,
      /*created_before_time=*/std::nullopt, site_matcher,
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(service().GetSession(site_a, Session::Id(kSessionId)));
  EXPECT_TRUE(service().GetSession(site_b, Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestRestart) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  // Create a request to kTestUrl and defer it.
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

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up the fetcher for a successful refresh.
  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);
  service().DeferRequestForRefresh(request.get(), Session::Id(kSessionId),
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  ASSERT_THAT(observer.notifications(),
              ElementsAre(SessionKey(site, Session::Id(kSessionId))));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession(site_1, Session::Id(kSessionId)));

  // Create a request to kTestUrl and defer it.
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

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  base::test::TestFuture<SessionKey> future_2;
  request->SetDeviceBoundSessionAccessCallback(
      future_2.GetRepeatingCallback<const SessionKey&>());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future_3;
  TestDeferCompletion defer_completion(
      future_3.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up a null fetcher for failure refresh.
  ScopedNullFetcher scoped_null_fetcher;
  service().DeferRequestForRefresh(request.get(), Session::Id(kSessionId),
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  SessionKey session_key = future_2.Take();
  EXPECT_EQ(session_key.site, site_1);
  EXPECT_EQ(session_key.id.value(), kSessionId);

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future_3.Take(), TestDeferCompletion::CallbackType::kContinue);
}

TEST_F(SessionServiceImplTest, TestDeferRequestArbitrary) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  // No session for site_2.
  SchemefulSite site_1(kTestUrl);
  SchemefulSite site_2(kTestUrl2);
  ASSERT_TRUE(service().GetSession(site_1, Session::Id(kSessionId)));
  ASSERT_FALSE(service().GetSession(site_2, Session::Id(kSessionId2)));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl2, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl2));

  std::optional<Session::Id> maybe_id =
      service().GetAnySessionRequiringDeferral(request.get());
  ASSERT_FALSE(maybe_id);

  // Defer the request any way.
  // Set AccessCallback for DeferRequestForRefresh().
  base::test::TestFuture<SessionKey> future_2;
  request->SetDeviceBoundSessionAccessCallback(
      future_2.GetRepeatingCallback<const SessionKey&>());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future_3;
  TestDeferCompletion defer_completion(
      future_3.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up a successful fetcher.
  ScopedTestFetcher scoped_test_fetcher_2(kSessionId2, kUrlString2, kOrigin2);
  service().DeferRequestForRefresh(request.get(), Session::Id(kSessionId2),
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  // Check the continue callback is called.
  EXPECT_EQ(future_3.Take(), TestDeferCompletion::CallbackType::kContinue);
}

TEST_F(SessionServiceImplTest, RefreshWithNewSessionId) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  // Create a request and defer it.
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

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up the fetcher for a successful refresh with a new session ID
  // which doesn't equal to the refreshing one.
  ScopedTestFetcher scoped_test_fetcher(kSessionId2, kUrlString, kOrigin);
  service().DeferRequestForRefresh(request.get(), Session::Id(kSessionId),
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(observer.notifications(),
              ElementsAre(SessionKey(site, Session::Id(kSessionId)),
                          SessionKey(site, Session::Id(kSessionId2))));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);

  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId2)));
  ASSERT_FALSE(service().GetSession(site, Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, SessionTerminationFromContinueFalse) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  ASSERT_TRUE(
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId)));

  {
    ScopedContinueFalseFetcher scoped_fetcher;
    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                   IsolationInfo::CreateTransient(),
                                   NetLogWithSource());
  }

  EXPECT_FALSE(
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, NetLogRegistration) {
  RecordingNetLogObserver observer;

  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(),
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST));
  EXPECT_EQ(
      observer.GetEntriesWithType(NetLogEventType::DBSC_REGISTRATION_REQUEST)
          .size(),
      1u);
}

TEST_F(SessionServiceImplTest, NetLogRefresh) {
  AddSessionsForTesting({{kSessionId, kUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  RecordingNetLogObserver observer;
  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);
  service().DeferRequestForRefresh(request.get(), Session::Id(kSessionId),
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  EXPECT_EQ(
      observer.GetEntriesWithType(NetLogEventType::DBSC_REFRESH_REQUEST).size(),
      1u);
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

  ScopedTestFetcher scoped_test_fetcher(kSessionId, kUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  // Will invoke the store's save session method.
  service().RegisterBoundSession(base::DoNothing(), std::move(fetch_param),
                                 IsolationInfo::CreateTransient(),
                                 NetLogWithSource());

  auto site = SchemefulSite(kTestUrl);
  Session* session = service().GetSession(site, Session::Id(kSessionId));
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

  SessionParams::Scope scope;
  scope.origin = "example.com";
  std::unique_ptr<Session> session = Session::CreateIfValid(
      SessionParams("session_id", "https://example.com/refresh",
                    std::move(scope),
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
