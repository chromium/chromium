// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/jwk_utils.h"
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
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
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

const std::string kSessionId3 = "SessionId3";

const std::string kChallenge = "challenge";

constexpr char kSessionChallengeHeaderName[] = "Secure-Session-Challenge";

proto::Session CreateSessionProto(std::string_view session_id,
                                  std::string_view url_string) {
  base::Time expiry_time = base::Time::Now() + base::Days(1);

  proto::Session session_proto;
  session_proto.set_id(session_id);
  session_proto.set_refresh_url(url_string);
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
  return session_proto;
}

// Matcher for SessionKeys
auto ExpectId(std::string_view id) {
  return testing::Field(&SessionKey::id, Session::Id(std::string(id)));
}

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

class RefreshTracker {
 public:
  void Refresh(RegistrationFetcher::RegistrationCompleteCallback callback) {
    pending_refreshes_.push_back(std::move(callback));
  }

  size_t num_pending_refreshes() { return pending_refreshes_.size(); }

  void ResolvePendingRefresh(RegistrationResult result) {
    CHECK(!pending_refreshes_.empty());
    std::move(pending_refreshes_[0]).Run(nullptr, std::move(result));
    pending_refreshes_.erase(pending_refreshes_.begin());
  }

 private:
  std::vector<RegistrationFetcher::RegistrationCompleteCallback>
      pending_refreshes_;
};

class SessionServiceImplTest : public ::testing::Test,
                               public WithTaskEnvironment {
 public:
  SessionServiceImplTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto network_delegate = std::make_unique<TestNetworkDelegate>();
    network_delegate_ = network_delegate.get();
    context_builder->set_network_delegate(std::move(network_delegate));
    context_ = context_builder->Build();
  }

  void SetUp() override {
    service_ = std::make_unique<SessionServiceImpl>(unexportable_key_service_,
                                                    context_.get(),
                                                    /*store=*/nullptr);
  }

  void TearDown() override {
    // Reset the `network_delegate_` to avoid a dangling pointer.
    network_delegate_ = nullptr;
  }

  TestNetworkDelegate* network_delegate() { return network_delegate_; }
  URLRequestContext* context() { return context_.get(); }
  SessionServiceImpl& service() { return *service_; }
  unexportable_keys::UnexportableKeyService* key_service() {
    return &unexportable_key_service_;
  }

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
  raw_ptr<TestNetworkDelegate> network_delegate_;
  std::unique_ptr<URLRequestContext> context_;
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_, crypto::UnexportableKeyProvider::Config()};
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  std::unique_ptr<SessionServiceImpl> service_;
};

class SessionServiceImplNoRefreshQuotaTest : public SessionServiceImplTest {
 public:
  SessionServiceImplNoRefreshQuotaTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kDeviceBoundSessions, {{"RefreshQuota", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SessionServiceImplTestWithFederatedSessions
    : public SessionServiceImplTest {
 public:
  SessionServiceImplTestWithFederatedSessions() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SessionServiceImplTestWithoutFederatedSessions
    : public SessionServiceImplTest {
 public:
  SessionServiceImplTestWithoutFederatedSessions() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
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

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);
}

TEST_F(SessionServiceImplTest, RegisterNullFetcher) {
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kNetError, kRefreshUrlString);
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

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  // NullFetcher, so should not be valid
  EXPECT_FALSE(maybe_deferral);
}

TEST_F(SessionServiceImplTest, SetChallengeForBoundSession) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(
      kSessionChallengeHeaderName,
      R"("challenge";id="SessionId", "challenge1";id="NonExisted")");
  headers->AddHeader(kSessionChallengeHeaderName, R"("challenge2")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());

  EXPECT_EQ(params.size(), 3U);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  for (const auto& param : params) {
    service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                          FirstPartySetMetadata(), param);
  }

  const Session* session =
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), "challenge");

  session = service().GetSession(
      {SchemefulSite(kTestUrl), Session::Id("NonExisted")});
  ASSERT_FALSE(session);
}

TEST_F(SessionServiceImplTest, SetChallengeForBoundSessionBlockedCookies) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(kSessionChallengeHeaderName,
                     R"("challenge";id="SessionId")");
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  network_delegate()->set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);

  ASSERT_EQ(params.size(), 1U);
  DbscRequest dbsc_request(request.get());
  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        FirstPartySetMetadata(), params[0]);

  const Session* session =
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(session->cached_challenge(), std::nullopt);
}

TEST_F(SessionServiceImplTest, ExpiryExtendedOnUser) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  Session* session =
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  session->set_expiry_date(base::Time::Now() + base::Days(1));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

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
  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

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
  headers->AddHeader(kSessionChallengeHeaderName,
                     "\"challenge\";id=\"SessionId\"");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1U);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<SessionAccess> future;
  DbscRequest dbsc_request(request.get());
  service().SetChallengeForBoundSession(
      future.GetRepeatingCallback<const SessionAccess&>(), dbsc_request,
      FirstPartySetMetadata(), params[0]);

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
  base::HistogramTester histograms;

  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto site = SchemefulSite(kTestUrl);
  auto session_id = Session::Id(kSessionId);

  ASSERT_TRUE(service().GetSession({site, session_id}));

  base::test::TestFuture<SessionAccess> future;
  service().DeleteSessionAndNotify(
      DeletionReason::kClearBrowsingData, {site, session_id},
      future.GetRepeatingCallback<const SessionAccess&>());

  SessionAccess access = future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kTermination);
  EXPECT_EQ(access.session_key.site, site);
  EXPECT_EQ(access.session_key.id, session_id);
  EXPECT_EQ(access.cookies, std::vector<std::string>{"test_cookie"});

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kClearBrowsingData, 1);
}

TEST_F(SessionServiceImplTest, DeleteAllSessionsByCreationTime) {
  base::HistogramTester histograms;
  net::SchemefulSite site(kTestUrl);

  AddSessionsForTesting({{"SessionA", kRefreshUrlString, kOrigin},
                         {"SessionB", kRefreshUrlString, kOrigin},
                         {"SessionC", kRefreshUrlString, kOrigin}});

  service()
      .GetSession({site, Session::Id("SessionA")})
      ->set_creation_date(base::Time::Now() - base::Days(6));
  service()
      .GetSession({site, Session::Id("SessionB")})
      ->set_creation_date(base::Time::Now() - base::Days(4));
  service()
      .GetSession({site, Session::Id("SessionC")})
      ->set_creation_date(base::Time::Now() - base::Days(2));

  base::RunLoop run_loop;
  service().DeleteAllSessions(DeletionReason::kStoragePartitionCleared,
                              base::Time::Now() - base::Days(5),
                              base::Time::Now() - base::Days(3),
                              /*origin_and_site_matcher=*/
                              base::NullCallback(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(service().GetSession({site, Session::Id("SessionA")}));
  EXPECT_FALSE(service().GetSession({site, Session::Id("SessionB")}));
  EXPECT_TRUE(service().GetSession({site, Session::Id("SessionC")}));

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kStoragePartitionCleared, 1);
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
  service().DeleteAllSessions(DeletionReason::kStoragePartitionCleared,
                              /*created_after_time=*/std::nullopt,
                              /*created_before_time=*/std::nullopt,
                              origin_and_site_matcher, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(service().GetSession({site_a, Session::Id(kSessionId)}));
  EXPECT_TRUE(service().GetSession({site_b, Session::Id(kSessionId)}));
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
  service().DeleteAllSessions(DeletionReason::kStoragePartitionCleared,
                              /*created_after_time=*/std::nullopt,
                              /*created_before_time=*/std::nullopt,
                              origin_and_site_matcher, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(service().GetSession({site, Session::Id(kSessionId)}));
  EXPECT_TRUE(service().GetSession({site, Session::Id(kSessionId2)}));
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestRestart) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  // Set up the fetcher for a successful refresh.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  SessionAccess expected_access{SessionAccess::AccessType::kCreation,
                                SessionKey(site, Session::Id(kSessionId))};
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))}));

  // Check that the request successfully refreshed.
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue_FatalError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession({site_1, Session::Id(kSessionId)}));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future_2;

  // Set up a null fetcher for failure refresh.
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kPersistentHttpError, kRefreshUrlString);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future_2.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site_1, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site_1, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}}));

  EXPECT_EQ(future_2.Take(), RefreshResult::kFatalError);
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue_NonFatalError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession({site_1, Session::Id(kSessionId)}));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future;

  // Set up a null fetcher for failure refresh.
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kNetError, kRefreshUrlString);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  ASSERT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site_1, Session::Id(kSessionId))}));

  // Check that the refresh failed.
  EXPECT_EQ(future.Take(), RefreshResult::kUnreachable);
}

TEST_F(SessionServiceImplTest, RefreshWithNewSessionId) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future;

  // Set up the fetcher for a failed refresh due to a new session ID
  // which doesn't equal to the refreshing one.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kInvalidSessionId, kRefreshUrlString);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}}));

  // Check session hits fatal error
  EXPECT_EQ(future.Take(), RefreshResult::kFatalError);

  ASSERT_FALSE(service().GetSession({site, Session::Id(kSessionId2)}));
  ASSERT_FALSE(service().GetSession({site, Session::Id(kSessionId)}));
}

TEST_F(SessionServiceImplTest, RefreshWithInvalidParams) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future;

  // Set up the fetcher for a successful refresh, but with invalid
  // parameters (e.g. doesn't specify any bound credentials).
  ScopedTestRegistrationFetcher scoped_test_fetcher(base::BindRepeating(
      [](RegistrationFetcher::RegistrationCompleteCallback callback) {
        std::move(callback).Run(
            nullptr, RegistrationResult(Session::CreateIfValid(SessionParams(
                         kSessionId, GURL(), "", SessionParams::Scope(),
                         std::vector<SessionParams::Credential>(),
                         unexportable_keys::UnexportableKeyId(),
                         /*allowed_refresh_initiators=*/{}))));
      }));
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))},
                  SessionAccess{SessionAccess::AccessType::kTermination,
                                SessionKey(site, Session::Id(kSessionId)),
                                std::vector<std::string>{"test_cookie"}}));

  // Check the session refresh fails.
  EXPECT_EQ(future.Take(), RefreshResult::kFatalError);
  ASSERT_FALSE(service().GetSession({site, Session::Id(kSessionId)}));
}

TEST_F(SessionServiceImplTest, SessionTerminationFromContinueFalse) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  ASSERT_TRUE(
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)}));

  auto scoped_fetcher = ScopedTestRegistrationFetcher::CreateWithTermination(
      kSessionId, kRefreshUrlString);
  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<RefreshResult> future;
  DbscRequest dbsc_request(request.get());
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  EXPECT_EQ(future.Take(), RefreshResult::kFatalError);
  EXPECT_FALSE(
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)}));
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
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<RefreshResult> future;

  RecordingNetLogObserver observer;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  DbscRequest dbsc_request(request.get());
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  EXPECT_EQ(
      observer.GetEntriesWithType(NetLogEventType::DBSC_REFRESH_REQUEST).size(),
      1u);
}

TEST_F(SessionServiceImplTest, RefreshUpdatesConfig) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<RefreshResult> future;

  RecordingNetLogObserver observer;
  // The refresh endpoint will return a config with a different refresh
  // URL, which we can use to test for persistence of the session config.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, "https://example.com/migrated-refresh", kOrigin);
  DbscRequest dbsc_request(request.get());
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  const Session* session =
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(session->refresh_url(),
            GURL("https://example.com/migrated-refresh"));
}

TEST_F(SessionServiceImplTest, SessionRefreshQuota) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // The first 6 refreshes succeed.
  DbscRequest dbsc_request(request.get());
  for (size_t i = 0; i < 6; i++) {
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
  }

  // The next refresh is throttled.
  {
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kRefreshQuotaExceeded);
  }

  // After 9 minutes, the quota is restored and the next refresh succeeds.
  FastForwardBy(base::Minutes(9));
  {
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
  }
}

TEST_F(SessionServiceImplTest, SessionSigningQuota) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDeviceBoundSessionSigningQuotaAndCaching);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  SessionKey session_key{SchemefulSite(GURL(kRefreshUrlString)),
                         Session::Id(kSessionId)};
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Repeated refreshes don't exceed the signing quota if they don't trigger
  // signing.
  DbscRequest dbsc_request(request.get());
  for (size_t i = 0; i < 10; i++) {
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
  }

  // The first 6 signings don't exceed the signing quota.
  for (size_t i = 0; i < 6; i++) {
    EXPECT_FALSE(service().SigningQuotaExceeded(session_key.site));
    service().AddSigningOccurrence(session_key.site);
  }
  // The next signing exceeds the signing quota.
  EXPECT_TRUE(service().SigningQuotaExceeded(session_key.site));

  // This affects other sessions on the same site, but not other sessions on a
  // different site.
  SessionKey session_key2{SchemefulSite(GURL(kRefreshUrlString)),
                          Session::Id(kSessionId2)};
  SessionKey session_key3{SchemefulSite(GURL(kRefreshUrlString2)),
                          Session::Id(kSessionId3)};
  EXPECT_TRUE(service().SigningQuotaExceeded(session_key2.site));
  EXPECT_FALSE(service().SigningQuotaExceeded(session_key3.site));

  // After 9 minutes, the quota is restored.
  FastForwardBy(base::Minutes(9));
  EXPECT_FALSE(service().SigningQuotaExceeded(session_key.site));
}

TEST_F(SessionServiceImplTest, LatestSignedRefreshChallenges) {
  SessionKey session_key1{SchemefulSite(GURL(kRefreshUrlString)),
                          Session::Id(kSessionId)};
  SessionKey session_key2{SchemefulSite(GURL(kRefreshUrlString)),
                          Session::Id(kSessionId2)};
  EXPECT_EQ(service().GetLatestSignedRefreshChallenge(session_key1), nullptr);

  SessionService::SignedRefreshChallenge signed_refresh_challenge = {
      .signed_challenge = "signed_challenge",
      .challenge = "challenge",
      .key_id = unexportable_keys::UnexportableKeyId()};
  service().SetLatestSignedRefreshChallenge(session_key1,
                                            signed_refresh_challenge);
  const SessionService::SignedRefreshChallenge* retrieved_challenge =
      service().GetLatestSignedRefreshChallenge(session_key1);
  EXPECT_EQ(retrieved_challenge->signed_challenge,
            signed_refresh_challenge.signed_challenge);
  EXPECT_EQ(retrieved_challenge->challenge, signed_refresh_challenge.challenge);
  EXPECT_EQ(retrieved_challenge->key_id, signed_refresh_challenge.key_id);
  EXPECT_EQ(service().GetLatestSignedRefreshChallenge(session_key2), nullptr);
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
  DbscRequest dbsc_request(request.get());
  for (size_t i = 0; i < 3; i++) {
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
  }
}

TEST_F(SessionServiceImplTest, SessionBackoff) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kTransientHttpError, kRefreshUrlString);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(service().ShouldDefer(dbsc_request, &extra_headers,
                                    FirstPartySetMetadata()));

  // Do four failing refreshes.
  for (size_t i = 0; i < 4; i++) {
    FastForwardBy(base::Minutes(5));
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kServerError);
  }

  // Backoff should prevent us from deferring anymore.
  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
  EXPECT_EQ(future.Take(), RefreshResult::kUnreachable);
}

TEST_F(SessionServiceImplTest, RepeatedDeferral) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kRefreshed);

  maybe_deferral = service().ShouldDefer(dbsc_request, &extra_headers,
                                         FirstPartySetMetadata());
  EXPECT_FALSE(maybe_deferral);
}

TEST_F(SessionServiceImplTest, AddsDebugHeader) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  AddSessionsForTesting({{kSessionId2, kRefreshUrlString, kOrigin}});
  AddSessionsForTesting({{kSessionId3, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Simulate failed refresh for two different sessions
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kUnreachable);
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId2)},
      RefreshResult::kRefreshQuotaExceeded);
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId3)},
      RefreshResult::kSigningQuotaExceeded);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  EXPECT_FALSE(maybe_deferral);

  std::optional<std::string> debug_header =
      extra_headers.GetHeader("Secure-Session-Skipped");
  EXPECT_TRUE(debug_header.has_value());
  EXPECT_EQ(*debug_header,
            "unreachable;session_identifier=\"SessionId\", "
            "quota_exceeded;session_identifier=\"SessionId2\", "
            "quota_exceeded;session_identifier=\"SessionId3\"");
}

TEST_F(SessionServiceImplTest, NoDebugHeaderOnSuccess) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Simulate success refresh.
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kRefreshed);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  EXPECT_FALSE(maybe_deferral);

  std::optional<std::string> debug_header =
      extra_headers.GetHeader("Secure-Session-Skipped");
  EXPECT_FALSE(debug_header.has_value());
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationSuccess) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint, kTestUrl,
      Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session exists.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_NE(relying_session, nullptr);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationWrongKey) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, "not_the_thumbprint",
      kTestRefreshUrl, Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationWrongSession) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint,
      kTestRefreshUrl, Session::Id("incorrect-provider-session"));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationWrongOrigin) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider, specifying a
  // subdomain of the site as the provider. Since the session applies to
  // all of `example.com`, the provider should be `example.com`, not
  // `subdomain.example.com`.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint,
      GURL("https://subdomain.example.com"), Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationInvalidUrl) {
  base::HistogramTester histograms;

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, "key-thumbprint",
      GURL("http:///"), Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kInvalidFederatedSessionUrl, 1);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationOpaqueOrigin) {
  base::HistogramTester histograms;

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, "key-thumbprint",
      GURL("data:text/html,session-provider"), Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kInvalidFederatedSessionUrl, 1);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationKeyUnrestored) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint, but leave it
  // unrestored.
  provider_session->set_unexportable_key_id(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));

  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));

  // Attempt a registration with a session provider
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint, kTestUrl,
      Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The relying session will not exist, since we did not have the
  // provider key restored.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  // Because we could not restore the key, we also deleted the provider session.
  EXPECT_EQ(service().GetSession({site, Session::Id(kSessionId)}), nullptr);
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kFailedToUnwrapKey, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey, 1);
}

TEST_F(SessionServiceImplTestWithoutFederatedSessions,
       IgnoresFederatedRegistration) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint, kTestUrl,
      Session::Id(kSessionId));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);
}

TEST_F(SessionServiceImplTest, EmptyResponseOnRegistration) {
  base::HistogramTester histograms;

  ScopedTestRegistrationFetcher scoped_test_fetcher(base::BindRepeating(
      [](RegistrationFetcher::RegistrationCompleteCallback callback) {
        std::move(callback).Run(
            nullptr, RegistrationResult(
                         SessionError{SessionError::kEmptySessionConfig}));
      }));
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

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());

  // Registration failed, so should not be valid
  EXPECT_FALSE(maybe_deferral);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kEmptySessionConfig, 1);
}

TEST_F(SessionServiceImplTest, EmptyResponseOnRefresh) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Defer the request.
  // Set AccessCallback for DeferRequestForRefresh().
  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future;

  // Set up the fetcher to return no response body.
  ScopedTestRegistrationFetcher scoped_test_fetcher(base::BindRepeating(
      [](RegistrationFetcher::RegistrationCompleteCallback callback) {
        std::move(callback).Run(
            nullptr,
            RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                               CookieAndLineAccessResultList()));
      }));
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Check access callback triggered by DeferRequestForRefresh.
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kUpdate,
                                SessionKey(site, Session::Id(kSessionId))}));

  // Check session still valid
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);

  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));
}

TEST_F(SessionServiceImplTest, SessionUsage) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kUnknown);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kNoUsage);

  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kDeferred);
}

}  // namespace

class SessionServiceImplWithStoreTest : public TestWithTaskEnvironment {
 public:
  SessionServiceImplWithStoreTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()),
        store_(std::make_unique<StrictMock<SessionStoreMock>>()),
        service_(unexportable_key_service_, context_.get(), store_.get()) {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
  }

  SessionServiceImpl& service() { return service_; }
  StrictMock<SessionStoreMock>& store() { return *store_; }

  unexportable_keys::ScopedMockUnexportableKeyProvider&
  SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<unexportable_keys::ScopedMockUnexportableKeyProvider>();
  }

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

  unexportable_keys::UnexportableKeyService* key_service() {
    return &unexportable_key_service_;
  }

 private:
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_, crypto::UnexportableKeyProvider::Config()};
  std::variant<crypto::ScopedFakeUnexportableKeyProvider,
               unexportable_keys::ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<StrictMock<SessionStoreMock>> store_;
  SessionServiceImpl service_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  Session* session = service().GetSession({site, Session::Id(kSessionId)});
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
      /*creds=*/{}, unexportable_keys::UnexportableKeyId(),
      /*allowed_refresh_initiators=*/{}));
  ASSERT_TRUE(session_or_error.has_value());
  std::unique_ptr<Session> session = std::move(*session_or_error);
  ASSERT_TRUE(session);

  // Complete loading. If we did not defer, we'd miss this session.
  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
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

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_TRUE(maybe_deferral->is_pending_initialization);

  // Now actually defer the request
  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Complete loading. We should now restart the request.
  FinishLoadingSessions(SessionStore::SessionsMap());

  EXPECT_EQ(future.Take(), RefreshResult::kInitializedService);
}

TEST_F(SessionServiceImplWithStoreTest, RequestDestroyedDuringAsyncKeyRestore) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  std::unique_ptr<Session> session =
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);

  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce([&](const SessionKey& session_key,
                    SessionStore::RestoreSessionBindingKeyCallback cb) {
        restore_key_callback = std::move(cb);
      });
  service().DeferRequestForRefresh(*dbsc_request, *maybe_deferral,
                                   base::DoNothing());
  // Simulate the request being cleaned up before the callback has been called.
  dbsc_request.reset();
  request.reset();
  ASSERT_TRUE(restore_key_callback);
  // Call the callback, and the test should not crash even though the request
  // was cleaned up.
  std::move(restore_key_callback).Run(unexportable_keys::UnexportableKeyId());
}

TEST_F(SessionServiceImplWithStoreTest, SessionKeyRestoredOnUse) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  std::unique_ptr<Session> session =
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession(_, _)).Times(1);
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(RunOnceCallback<1>(unexportable_keys::UnexportableKeyId()));

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   future.GetCallback());

  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest, NoSessionUsageDuringInitialization) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());

  EXPECT_EQ(request->device_bound_session_usage(), SessionUsage::kUnknown);
}

TEST_F(SessionServiceImplWithStoreTest, GarbageCollectsStaleKeys) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list(
      unexportable_keys::kUnexportableKeyDeletion);
  unexportable_keys::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // The first two keys are known to the service, but the third key is stale.
  const std::vector<uint8_t> kWrappedKey1 = {1, 2, 3};
  const std::vector<uint8_t> kWrappedKey2 = {4, 5, 6};
  const std::vector<uint8_t> kStaleWrappedKey = {7, 8, 9};

  EXPECT_CALL(mock_key_provider, GetAllSigningKeysSlowly).WillRepeatedly([=] {
    auto key1 = std::make_unique<unexportable_keys::MockUnexportableKey>();
    auto key2 = std::make_unique<unexportable_keys::MockUnexportableKey>();
    auto stale_key = std::make_unique<unexportable_keys::MockUnexportableKey>();

    ON_CALL(*key1, GetWrappedKey).WillByDefault(Return(kWrappedKey1));
    ON_CALL(*key2, GetWrappedKey).WillByDefault(Return(kWrappedKey2));
    ON_CALL(*stale_key, GetWrappedKey).WillByDefault(Return(kStaleWrappedKey));

    return base::ToVector<std::unique_ptr<crypto::UnexportableSigningKey>>({
        std::move(key1),
        std::move(key2),
        std::move(stale_key),
    });
  });

  // Obtain the corresponding key ids.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      std::vector<unexportable_keys::UnexportableKeyId>>>
      get_all_keys_future;
  key_service()->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      get_all_keys_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(
      std::vector<unexportable_keys::UnexportableKeyId> all_keys_ids,
      get_all_keys_future.Take());

  // Create a session map with two sessions, each associated with a known key.
  auto session1 =
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString));
  session1->set_unexportable_key_id(all_keys_ids[0]);
  auto session2 =
      Session::CreateFromProto(CreateSessionProto(kSessionId2, kUrlString2));
  session2->set_unexportable_key_id(all_keys_ids[1]);
  SessionStore::SessionsMap session_map;
  session_map[SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)}] =
      std::move(session1);
  session_map[SessionKey{SchemefulSite(kTestUrl2), Session::Id(kSessionId2)}] =
      std::move(session2);

  // Finish loading the sessions, and wait for the stale key to be deleted.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_key_provider, DeleteSigningKeySlowly(Eq(kWrappedKey1)))
      .Times(0);
  EXPECT_CALL(mock_key_provider, DeleteSigningKeySlowly(Eq(kWrappedKey2)))
      .Times(0);
  EXPECT_CALL(mock_key_provider, DeleteSigningKeySlowly(Eq(kStaleWrappedKey)))
      .WillOnce(Return(true));

  FinishLoadingSessions(std::move(session_map));
  // Advance time to allow StartGarbageCollection to run.
  FastForwardUntilNoTasksRemain();

  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "TotalKeyCount",
      3, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "UsedKeyCount",
      2, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyCount",
      1, 1);
  histograms.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyDeletionCount",
      1, 1);
}

TEST_F(SessionServiceImplWithStoreTest,
       GarbageCollectionDoesNotTriggerIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      unexportable_keys::kUnexportableKeyDeletion);
  unexportable_keys::MockUnexportableKeyProvider& mock_key_provider =
      SwitchToMockKeyProvider().mock();

  EXPECT_CALL(mock_key_provider, GetAllSigningKeysSlowly).Times(0);
  EXPECT_CALL(mock_key_provider, DeleteAllSigningKeysSlowly).Times(0);
  FinishLoadingSessions({});
}

TEST_F(SessionServiceImplWithStoreTest,
       FederatedProviderSessionKeyRestoredOnUse) {
  // Create the provider session
  SchemefulSite site(kTestUrl);

  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  std::unique_ptr<Session> provider_session =
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString));
  ASSERT_TRUE(provider_session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), provider_session->id()},
       std::move(provider_session)});
  FinishLoadingSessions(std::move(session_map));

  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableKeyId key = *key_future.Take();
  std::string key_thumbprint = CreateJwkThumbprint(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      *key_service()->GetSubjectPublicKeyInfo(key));

  // Attempt a registration with a session provider
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt, key_thumbprint, kTestUrl,
      Session::Id(kSessionId));
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(RunOnceCallback<1>(key));
  EXPECT_CALL(store(), SaveSession).Times(1);
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The relying session will exist, since we restored the provider key.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_NE(relying_session, nullptr);
  EXPECT_NE(service().GetSession({site, Session::Id(kSessionId)}), nullptr);
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kSuccess, 1);
}

TEST_F(SessionServiceImplTest, GoogleRegistrationLog) {
  base::HistogramTester histogram_tester;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL("https://accounts.google.com/"),
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST),
      /*original_request_initiator=*/std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.GoogleRegistrationIsFromStandard", true, 1);
}

TEST_F(SessionServiceImplTest, NoGoogleRegistrationLog) {
  base::HistogramTester histogram_tester;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL("https://notgoogle.com/"),
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST),
      /*original_request_initiator=*/std::nullopt);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.GoogleRegistrationIsFromStandard", 0);
}

TEST_F(SessionServiceImplTest, DeferringRefreshBlocksDeferring) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  // Defer the request.
  // Try deferring twice
  DbscRequest dbsc_request(request.get());
  service().DeferRequestForRefresh(dbsc_request, deferral, base::DoNothing());
  service().DeferRequestForRefresh(dbsc_request, deferral, base::DoNothing());

  // But only one refresh actually happened
  EXPECT_EQ(tracker.num_pending_refreshes(), 1);
}

TEST_F(SessionServiceImplTest, ProactiveRefreshBlocksDeferring) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Attach the required cookie, but make it expire very soon
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // We should not want to defer this request, but it should trigger a proactive
  // refresh
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Defer the request.
  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));
  base::test::TestFuture<RefreshResult> future;
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().DeferRequestForRefresh(*dbsc_request, deferral,
                                   future.GetCallback());

  // We still only do the proactive refresh
  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         /*maybe_stored_cookies=*/{}));
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::kAttempted, 1);
}

TEST_F(SessionServiceImplTest, ProactiveRefreshBlocksProactive) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Attach the required cookie, but make it expire very soon
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // We should not want to defer this request, but it should trigger a proactive
  // refresh
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Another request should not do another proactive refresh
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  dbsc_request = std::make_unique<DbscRequest>(request.get());

  maybe_deferral = service().ShouldDefer(*dbsc_request, &extra_headers,
                                         FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::kAttempted, 1);
  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::kExistingProactiveRefresh,
      1);
}

TEST_F(SessionServiceImplTest, DeferringRefreshBlocksProactive) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  // Defer the request.
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().DeferRequestForRefresh(*dbsc_request, deferral, base::DoNothing());

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Attach the required cookie, but make it expire very soon. This will
  // trigger proactive refresh.
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  dbsc_request = std::make_unique<DbscRequest>(request.get());

  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  HttpRequestHeaders extra_headers;
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::kExistingDeferringRefresh,
      1);
}

TEST_F(SessionServiceImplTest, FailedProactiveRefreshBlocksProactiveRefresh) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Attach the required cookie, but make it expire very soon
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // We should not want to defer this request, but it should trigger a proactive
  // refresh
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Cause the proactive refresh to fail
  tracker.ResolvePendingRefresh(
      RegistrationResult(SessionError(SessionError::kTransientHttpError)));

  // Another request should not do another proactive refresh
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  dbsc_request = std::make_unique<DbscRequest>(request.get());

  maybe_deferral = service().ShouldDefer(*dbsc_request, &extra_headers,
                                         FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 0);

  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::kAttempted, 1);
  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt",
      SessionServiceImpl::ProactiveRefreshAttempt::
          kPreviousFailedProactiveRefresh,
      1);
}

TEST_F(SessionServiceImplTest, SessionDeletionDuringRefresh_ConfigChange) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().DeferRequestForRefresh(*dbsc_request, deferral, base::DoNothing());

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Delete the session
  {
    base::RunLoop run_loop;
    service().DeleteSessionAndNotify(
        DeletionReason::kClearBrowsingData,
        {SchemefulSite(kTestUrl), Session::Id(kSessionId)},
        base::IgnoreArgs<const SessionAccess&>(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Complete the refresh with a new session
  SessionParams::Scope scope;
  scope.origin = "https://example.com";
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateIfValid(SessionParams(
          kSessionId, kTestUrl, "https://example.com/refresh", std::move(scope),
          /*creds=*/{}, unexportable_keys::UnexportableKeyId(),
          /*allowed_refresh_initiators=*/{})));
  ASSERT_TRUE(session);

  tracker.ResolvePendingRefresh(
      RegistrationResult(std::move(session),
                         /*maybe_stored_cookies=*/{}));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RefreshResult",
                                SessionError::kSessionDeletedDuringRefresh, 1);
}

TEST_F(SessionServiceImplTest,
       SessionDeletionDuringRefresh_NoSessionConfigChange) {
  base::HistogramTester histograms;

  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().DeferRequestForRefresh(*dbsc_request, deferral, base::DoNothing());

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Delete the session
  {
    base::RunLoop run_loop;
    service().DeleteSessionAndNotify(
        DeletionReason::kClearBrowsingData,
        {SchemefulSite(kTestUrl), Session::Id(kSessionId)},
        base::IgnoreArgs<const SessionAccess&>(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Complete the refresh
  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         /*maybe_stored_cookies=*/{}));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RefreshResult",
                                SessionError::kSessionDeletedDuringRefresh, 1);
}

}  // namespace net::device_bound_sessions
