// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/mock_session_store.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunOnceCallback;
using ::testing::_;
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
constexpr char kRefreshUrlString[] = "https://example.com/refresh";
const GURL kTestRefreshUrl(kRefreshUrlString);
const std::string kSessionId = "SessionId";
const std::string kOrigin = "https://example.com";

constexpr char kUrlString2[] = "https://example2.com";
const GURL kTestUrl2(kUrlString2);
constexpr char kRefreshUrlString2[] = "https://example2.com/refresh";
const GURL kTestRefreshUrl2(kRefreshUrlString);
const std::string kSessionId2 = "SessionId2";
const std::string kOrigin2 = "https://example2.com";

const std::string kChallenge = "challenge";

// Matcher for SessionKeys
auto ExpectId(std::string_view id) {
  return testing::Field(&SessionKey::id, Session::Id(std::string(id)));
}

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
  const std::vector<SessionAccess>& notifications() const {
    return notifications_;
  }

  void ClearNotifications() { notifications_.clear(); }

  void OnSessionAccessed(const SessionAccess& session_access) {
    notifications_.push_back(session_access);
  }

  SessionService::OnAccessCallback GetCallback() {
    return base::BindRepeating(
        &FakeDeviceBoundSessionObserver::OnSessionAccessed,
        base::Unretained(this));
  }

 private:
  std::vector<SessionAccess> notifications_;
};

class SessionServiceImplTest : public ::testing::Test,
                               public WithTaskEnvironment {
 public:
  SessionServiceImplTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()) {}

  void SetUp() override {
    service_ = std::make_unique<SessionServiceImpl>(
        *UnexportableKeyServiceFactory::GetInstance()->GetShared(),
        context_.get(),
        /*store=*/nullptr);
  }

  URLRequestContext* context() { return context_.get(); }
  SessionServiceImpl& service() { return *service_; }

  // Take list of <session_id, site_url> to add sessions for testing.
  void AddSessionsForTesting(
      std::vector<std::tuple<std::string, std::string, std::string>>
          id_url_origin_list) {
    for (const auto& [id, url_str, origin] : id_url_origin_list) {
      auto scoped_test_fetcher =
          ScopedTestRegistrationFetcher::CreateWithSuccess(id, url_str, origin);
      auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
          GURL(url_str),
          {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
          "challenge", /*authorization=*/std::nullopt);
      service_->RegisterBoundSession(
          base::DoNothing(), std::move(fetch_param),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
    }
  }

 private:
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<SessionServiceImpl> service_;
};

class SessionServiceImplNoRefreshQuotaTest : public SessionServiceImplTest {
 public:
  SessionServiceImplNoRefreshQuotaTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kDeviceBoundSessionsRefreshQuota);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SessionServiceImplTest, RegisterSuccess) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);
}

TEST_F(SessionServiceImplTest, RegisterNoId) {
  AddSessionsForTesting({{/*session_id=*/"", kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  // session_id is empty, so should not be valid
  EXPECT_FALSE(maybe_deferral);
}

TEST_F(SessionServiceImplTest, RegisterNullFetcher) {
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::ErrorType::kNetError, kRefreshUrlString);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  // NullFetcher, so should not be valid
  EXPECT_FALSE(maybe_deferral);
}

TEST_F(SessionServiceImplTest, SetChallengeForBoundSession) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

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
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

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

  service().ShouldDefer(request.get(), FirstPartySetMetadata());

  EXPECT_GT(session->expiry_date(), base::Time::Now() + base::Days(399));
}

TEST_F(SessionServiceImplTest, NullAccessObserver) {
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The access observer was null, so no call is expected
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnRegistration) {
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  base::test::TestFuture<SessionAccess> future;
  service().RegisterBoundSession(
      future.GetRepeatingCallback<const SessionAccess&>(),
      std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  SessionAccess access = future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kCreation);
  EXPECT_EQ(access.session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(access.session_key.id.value(), kSessionId);
  EXPECT_TRUE(access.cookies.empty());
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnDeferral) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<SessionAccess> future;
  request->SetDeviceBoundSessionAccessCallback(
      future.GetRepeatingCallback<const SessionAccess&>());
  service().ShouldDefer(request.get(), FirstPartySetMetadata());

  SessionAccess access = future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kUpdate);
  EXPECT_EQ(access.session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(access.session_key.id.value(), kSessionId);
  EXPECT_TRUE(access.cookies.empty());
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnSetChallenge) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader("Sec-Session-Challenge", "\"challenge\";id=\"SessionId\"");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1U);

  base::test::TestFuture<SessionAccess> future;
  service().SetChallengeForBoundSession(
      future.GetRepeatingCallback<const SessionAccess&>(), kTestUrl, params[0]);

  SessionAccess access = future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kUpdate);
  EXPECT_EQ(access.session_key.site, SchemefulSite(kTestUrl));
  EXPECT_EQ(access.session_key.id.value(), kSessionId);
  EXPECT_TRUE(access.cookies.empty());
}

TEST_F(SessionServiceImplTest, GetAllSessions) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin},
                         {kSessionId2, kRefreshUrlString2, kOrigin2}});

  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId(kSessionId),
                                                  ExpectId(kSessionId2)));
}

TEST_F(SessionServiceImplTest, DeleteSession) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto site = SchemefulSite(kTestUrl);
  auto session_id = Session::Id(kSessionId);

  ASSERT_TRUE(service().GetSession(site, session_id));

  base::test::TestFuture<SessionAccess> future;
  service().DeleteSessionAndNotify(
      site, session_id, future.GetRepeatingCallback<const SessionAccess&>());

  SessionAccess access = future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kTermination);
  EXPECT_EQ(access.session_key.site, site);
  EXPECT_EQ(access.session_key.id, session_id);
  EXPECT_EQ(access.cookies, std::vector<std::string>{"test_cookie"});
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsByCreationTime) {
  net::SchemefulSite site(kTestUrl);

  AddSessionsForTesting({{"SessionA", kRefreshUrlString, kOrigin},
                         {"SessionB", kRefreshUrlString, kOrigin},
                         {"SessionC", kRefreshUrlString, kOrigin}});

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
                              /*origin_and_site_matcher=*/
                              base::NullCallback(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(service().GetSession(site, Session::Id("SessionA")));
  EXPECT_FALSE(service().GetSession(site, Session::Id("SessionB")));
  EXPECT_TRUE(service().GetSession(site, Session::Id("SessionC")));
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsBySite) {
  GURL url_a("https://a_example.com");
  GURL url_b("https://b_example.com");

  AddSessionsForTesting({{kSessionId, url_a.spec(), "https://a_example.com"},
                         {kSessionId, url_b.spec(), "https://b_example.com"}});

  SchemefulSite site_a(url_a);
  SchemefulSite site_b(url_b);

  // `origin_and_site_matcher` only matches `site_a`
  base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
      origin_and_site_matcher = base::BindRepeating(
          [](const net::SchemefulSite& site_a, const url::Origin& origin,
             const net::SchemefulSite& site) { return site == site_a; },
          site_a);

  base::RunLoop run_loop;
  service().DeleteAllSessions(
      /*created_after_time=*/std::nullopt,
      /*created_before_time=*/std::nullopt, origin_and_site_matcher,
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(service().GetSession(site_a, Session::Id(kSessionId)));
  EXPECT_TRUE(service().GetSession(site_b, Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsByOrigin) {
  GURL url_a("https://example.com:1234");
  GURL url_b("https://example.com:5678");

  AddSessionsForTesting(
      {{kSessionId, url_a.spec(), "https://example.com:1234"},
       {kSessionId2, url_b.spec(), "https://example.com:5678"}});

  // Both sessions have the same site, but different origins.
  SchemefulSite site(url_a);

  // `origin_and_site_matcher` only matches the first origin.
  base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
      origin_and_site_matcher = base::BindRepeating(
          [](const url::Origin& origin_a, const url::Origin& origin,
             const net::SchemefulSite& site) { return origin == origin_a; },
          url::Origin::Create(url_a));

  base::RunLoop run_loop;
  service().DeleteAllSessions(
      /*created_after_time=*/std::nullopt,
      /*created_before_time=*/std::nullopt, origin_and_site_matcher,
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(service().GetSession(site, Session::Id(kSessionId)));
  EXPECT_TRUE(service().GetSession(site, Session::Id(kSessionId2)));
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestRestart) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up the fetcher for a successful refresh.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  SessionAccess expected_access{SessionAccess::AccessType::kCreation,
                                SessionKey(site, Session::Id(kSessionId))};
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))}));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue_FatalError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession(site_1, Session::Id(kSessionId)));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future_2;
  TestDeferCompletion defer_completion(
      future_2.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up a null fetcher for failure refresh.
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::ErrorType::kPersistentHttpError, kRefreshUrlString);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site_1, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site_1, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}}));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future_2.Take(), TestDeferCompletion::CallbackType::kContinue);
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue_NonFatalError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession(site_1, Session::Id(kSessionId)));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up a null fetcher for failure refresh.
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::ErrorType::kNetError, kRefreshUrlString);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site_1, Session::Id(kSessionId))}));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kContinue);
}

TEST_F(SessionServiceImplTest, TestDeferRequestArbitrary) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

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

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  // Defer the request any way.
  // Set AccessCallback for DeferRequestForRefresh().
  base::test::TestFuture<SessionAccess> future_2;
  request->SetDeviceBoundSessionAccessCallback(
      future_2.GetRepeatingCallback<const SessionAccess&>());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future_3;
  TestDeferCompletion defer_completion(
      future_3.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up a successful fetcher.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId2, kRefreshUrlString2, kOrigin2);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId2)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check the continue callback is called.
  EXPECT_EQ(future_3.Take(), TestDeferCompletion::CallbackType::kContinue);
}

TEST_F(SessionServiceImplTest, RefreshWithNewSessionId) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

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
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId2, kRefreshUrlString, kOrigin);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}},
                  SessionAccess{SessionAccess::AccessType::kCreation,
                                SessionKey(site, Session::Id(kSessionId2))}));

  // Check the restart callback is called for successful fetcher.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);

  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId2)));
  ASSERT_FALSE(service().GetSession(site, Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, RefreshWithInvalidParams) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession(site, Session::Id(kSessionId)));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set RestartCallback and ContinueCallback.
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());

  // Set up the fetcher for a successful refresh, but with invalid
  // parameters (e.g. doesn't specify any bound credentials).
  ScopedTestRegistrationFetcher scoped_test_fetcher(base::BindRepeating([]() {
    return base::expected<SessionParams, SessionError>(
        SessionParams(kSessionId, GURL(), "", SessionParams::Scope(),
                      std::vector<SessionParams::Credential>(),
                      unexportable_keys::UnexportableKeyId()));
  }));
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}}));
  // Check the restart callback is called due to unsuccessful refresh.
  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kContinue);
  ASSERT_FALSE(service().GetSession(site, Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, SessionTerminationFromContinueFalse) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  ASSERT_TRUE(
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId)));

  {
    auto scoped_fetcher = ScopedTestRegistrationFetcher::CreateWithTermination(
        kSessionId, kRefreshUrlString);
    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "challenge", /*authorization=*/std::nullopt);
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
  }

  EXPECT_FALSE(
      service().GetSession(SchemefulSite(kTestUrl), Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, NetLogRegistration) {
  RecordingNetLogObserver observer;

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST),
      /*original_request_initiator=*/std::nullopt);
  EXPECT_EQ(
      observer.GetEntriesWithType(NetLogEventType::DBSC_REGISTRATION_REQUEST)
          .size(),
      1u);
}

TEST_F(SessionServiceImplTest, NetLogRefresh) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

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
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().DeferRequestForRefresh(
      request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
      defer_completion.GetRestartCb(), defer_completion.GetContinueCb());

  EXPECT_EQ(
      observer.GetEntriesWithType(NetLogEventType::DBSC_REFRESH_REQUEST).size(),
      1u);
}

TEST_F(SessionServiceImplTest, SessionRefreshQuota) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // The first refresh succeeds
  {
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
  }

  // The second refresh succeeds
  {
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
  }

  // The third refresh is throttled
  {
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kContinue);
  }

  // After five minutes, the quota is restored and the fourth refresh
  // succeeds.
  FastForwardBy(base::Minutes(5));
  {
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
  }
}

TEST_F(SessionServiceImplNoRefreshQuotaTest, SessionRefreshQuotaDisabled) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // The third refresh is not throttled because the refresh quota is disabled.
  for (size_t i = 0; i < 3; i++) {
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
  }
}

TEST_F(SessionServiceImplTest, SessionBackoff) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::ErrorType::kTransientHttpError, kRefreshUrlString);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  EXPECT_TRUE(service().ShouldDefer(request.get(), FirstPartySetMetadata()));

  // Do four failing refreshes.
  for (size_t i = 0; i < 4; i++) {
    FastForwardBy(base::Minutes(5));
    base::test::TestFuture<TestDeferCompletion::CallbackType> future;
    TestDeferCompletion defer_completion(
        future.GetCallback<TestDeferCompletion::CallbackType>());
    service().DeferRequestForRefresh(
        request.get(), SessionService::DeferralParams(Session::Id(kSessionId)),
        defer_completion.GetRestartCb(), defer_completion.GetContinueCb());
    EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kContinue);
  }

  // Backoff should prevent us from deferring anymore.
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  EXPECT_FALSE(maybe_deferral);
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

  URLRequestContext* context() { return context_.get(); }

 private:
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
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

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  // Will invoke the store's save session method.
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

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
  scope.origin = "https://example.com";
  auto session_or_error = Session::CreateIfValid(SessionParams(
      "session_id", kTestUrl, "https://example.com/refresh", std::move(scope),
      /*creds=*/{}, unexportable_keys::UnexportableKeyId()));
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);

  // Complete loading. If we did not defer, we'd miss this session.
  SessionStore::SessionsMap session_map;
  session_map.insert({SchemefulSite(kTestUrl), std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // But we did defer, so we found it.
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId("session_id")));
}

TEST_F(SessionServiceImplWithStoreTest, RequestsWaitForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Create a request that should be deferred due to initialization not
  // having completed.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_TRUE(maybe_deferral->is_pending_initialization);

  // Now actually defer the request
  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());
  service().DeferRequestForRefresh(request.get(), *maybe_deferral,
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  EXPECT_FALSE(future.IsReady());

  // Complete loading. We should now restart the request.
  FinishLoadingSessions(SessionStore::SessionsMap());

  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
}

TEST_F(SessionServiceImplWithStoreTest, SessionKeyRestoredOnUse) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  base::Time expiry_time = base::Time::Now() + base::Days(1);

  proto::Session session_proto;
  session_proto.set_id(kSessionId);
  session_proto.set_refresh_url(kUrlString);
  session_proto.set_should_defer_when_expired(false);
  session_proto.set_expiry_time(
      expiry_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  session_proto.mutable_session_inclusion_rules()->set_origin(
      "https://example.com");
  session_proto.mutable_session_inclusion_rules()->set_do_include_site(true);

  proto::CookieCraving* craving_proto = session_proto.add_cookie_cravings();
  craving_proto->set_name("test_cookie");
  craving_proto->set_domain("example.com");
  craving_proto->set_path("/");
  craving_proto->set_secure(true);
  craving_proto->set_httponly(true);
  craving_proto->set_source_port(443);
  craving_proto->set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  craving_proto->set_same_site(proto::CookieSameSite::LAX_MODE);
  craving_proto->set_source_scheme(proto::CookieSourceScheme::SECURE);

  std::unique_ptr<Session> session = Session::CreateFromProto(session_proto);
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert({SchemefulSite(kTestUrl), std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(request.get(), FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), DeleteSession(_, _)).Times(1);
  EXPECT_CALL(store(), SaveSession(_, _)).Times(1);
  EXPECT_CALL(store(), RestoreSessionBindingKey(SchemefulSite(kTestUrl),
                                                Session::Id(kSessionId), _))
      .WillOnce(RunOnceCallback<2>(unexportable_keys::UnexportableKeyId()));

  base::test::TestFuture<TestDeferCompletion::CallbackType> future;
  TestDeferCompletion defer_completion(
      future.GetCallback<TestDeferCompletion::CallbackType>());
  service().DeferRequestForRefresh(request.get(), *maybe_deferral,
                                   defer_completion.GetRestartCb(),
                                   defer_completion.GetContinueCb());

  EXPECT_EQ(future.Take(), TestDeferCompletion::CallbackType::kRestart);
}

}  // namespace net::device_bound_sessions
