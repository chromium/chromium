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
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/challenge_result.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/mock_session_store.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/log/test_net_log.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/device_bound_session_mode.h"
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
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArgByMove;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

namespace net::device_bound_sessions {

namespace {

constexpr unexportable_keys::BackgroundTaskOrigin kTaskOrigin =
    unexportable_keys::BackgroundTaskOrigin::kDeviceBoundSessionCredentials;

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
  explicit SessionServiceImplTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : WithTaskEnvironment(time_source) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto network_delegate = std::make_unique<TestNetworkDelegate>();
    network_delegate_ = network_delegate.get();
    context_builder->set_network_delegate(std::move(network_delegate));
    context_ = context_builder->Build();
  }

  void SetUp() override {
    service_ = std::make_unique<SessionServiceImpl>(
        unexportable_key_service_, context_.get(),
        /*store=*/nullptr,
        /*restricted_sites=*/std::vector<SchemefulSite>());
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
      task_manager_, kTaskOrigin, crypto::UnexportableKeyProvider::Config()};
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

TEST_F(SessionServiceImplTest, EventObserverOnRegistrationSuccess) {
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    const auto& details =
        std::get<CreationEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_TRUE(details.new_session_display.has_value());
    EXPECT_EQ(details.new_session_display->key.id.value(), kSessionId);
  });
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
}

TEST_F(SessionServiceImplTest, EventObserverOnRegistrationFailure) {
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, std::nullopt);
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    const auto& details =
        std::get<CreationEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kInvalidFetcherUrl);
    ASSERT_FALSE(details.new_session_display.has_value());
  });
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kInvalidFetcherUrl, kRefreshUrlString);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
}

TEST_F(SessionServiceImplTest,
       EventObserverOnRegistrationCapturedFailedRequest) {
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    const auto& details =
        std::get<CreationEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kPersistentHttpError);
    ASSERT_TRUE(details.failed_request.has_value());
    EXPECT_EQ(details.failed_request->request_url, kTestUrl);
    EXPECT_EQ(details.failed_request->net_error, OK);
    EXPECT_EQ(details.failed_request->response_error, 404);
    EXPECT_EQ(details.failed_request->response_error_body, "Not Found");
  });

  SessionError error(SessionError::kPersistentHttpError);
  FailedRequest failed_request;
  failed_request.request_url = kTestUrl;
  failed_request.net_error = OK;
  failed_request.response_error = 404;
  failed_request.response_error_body = "Not Found";
  error.failed_request = std::move(failed_request);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      [](std::optional<FailedRequest> failed_request,
         SessionError::ErrorType error_type,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        SessionError error(error_type);
        error.failed_request = std::move(failed_request);
        std::move(callback).Run(nullptr, RegistrationResult(std::move(error)));
      },
      error.failed_request, error.type));
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
}

TEST_F(SessionServiceImplTest, EventObserverOnAddSession) {
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  std::vector<uint8_t> wrapped_key = key_service()->GetWrappedKey(key).value();

  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<CreationEventDetails>(event.event_type_details));
    const auto& details =
        std::get<CreationEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_TRUE(details.new_session_display.has_value());
    EXPECT_EQ(details.new_session_display->key.id.value(), kSessionId);
  });

  SessionParams::Scope scope;
  scope.origin = kOrigin;
  SessionParams params(
      kSessionId, kTestUrl, kRefreshUrlString, std::move(scope),
      /*creds=*/{}, unexportable_keys::UnexportableSigningKeyId(),
      /*allowed_refresh_initiators=*/{});

  base::test::TestFuture<SessionError::ErrorType> add_session_future;
  service().AddSession(SchemefulSite(kTestUrl), std::move(params), wrapped_key,
                       add_session_future.GetCallback());
  EXPECT_EQ(add_session_future.Take(), SessionError::kSuccess);
}

TEST_F(SessionServiceImplTest, NoCallbackIfEventObserverRemoved) {
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  // Subscription goes out of scope, which should remove the observer.
  {
    base::CallbackListSubscription subscription =
        service().AddEventObserver(event_callback.Get());
  }
  EXPECT_CALL(event_callback, Run(_)).Times(0);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
}

TEST_F(SessionServiceImplTest, EventObserverOnRefresh) {
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

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_TRUE(details.new_session_display.has_value());
    EXPECT_EQ(details.new_session_display->key.id.value(), kSessionId);
    EXPECT_FALSE(details.was_fully_proactive_refresh);
  });

  base::test::TestFuture<RefreshResult> future;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
}

TEST_F(SessionServiceImplTest, EventObserverOnRefreshNoSessionChange) {
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

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_FALSE(details.new_session_display.has_value());
    EXPECT_FALSE(details.was_fully_proactive_refresh);
  });

  base::test::TestFuture<RefreshResult> future;
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
}

TEST_F(SessionServiceImplTest, EventObserverOnRefreshTermination) {
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

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_))
      .WillOnce([](const SessionEvent& event) {
        EXPECT_TRUE(std::holds_alternative<TerminationEventDetails>(
            event.event_type_details));
        EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
        EXPECT_EQ(event.session_id, kSessionId);
        EXPECT_TRUE(event.succeeded);
        ASSERT_TRUE(std::holds_alternative<TerminationEventDetails>(
            event.event_type_details));
        const auto& details =
            std::get<TerminationEventDetails>(event.event_type_details);
        EXPECT_EQ(details.deletion_reason, DeletionReason::kServerRequested);
      })
      .WillOnce([](const SessionEvent& event) {
        EXPECT_TRUE(std::holds_alternative<RefreshEventDetails>(
            event.event_type_details));
        EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
        EXPECT_EQ(event.session_id, kSessionId);
        EXPECT_FALSE(event.succeeded);
        ASSERT_TRUE(std::holds_alternative<RefreshEventDetails>(
            event.event_type_details));
        const auto& details =
            std::get<RefreshEventDetails>(event.event_type_details);
        EXPECT_EQ(details.fetch_error,
                  SessionError::kServerRequestedTermination);
        ASSERT_FALSE(details.new_session_display.has_value());
        EXPECT_FALSE(details.was_fully_proactive_refresh);
      });

  base::test::TestFuture<RefreshResult> future;
  auto scoped_fetcher = ScopedTestRegistrationFetcher::CreateWithTermination(
      kSessionId, kRefreshUrlString);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
}

TEST_F(SessionServiceImplTest, EventObserverOnRefreshTransientError) {
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

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kTransientHttpError);
    ASSERT_FALSE(details.new_session_display.has_value());
    EXPECT_FALSE(details.was_fully_proactive_refresh);
  });

  base::test::TestFuture<RefreshResult> future;
  auto scoped_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kTransientHttpError, kRefreshUrlString);
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
}

TEST_F(SessionServiceImplTest, EventObserverOnRefreshCapturedFailedRequest) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kTransientHttpError);
    ASSERT_TRUE(details.failed_request.has_value());
    EXPECT_EQ(details.failed_request->request_url, kTestRefreshUrl);
    EXPECT_EQ(details.failed_request->net_error, OK);
    EXPECT_EQ(details.failed_request->response_error, 500);
    EXPECT_EQ(details.failed_request->response_error_body,
              "Internal Server Error");
  });

  SessionError error(SessionError::kTransientHttpError);
  FailedRequest failed_request;
  failed_request.request_url = kTestRefreshUrl;
  failed_request.net_error = OK;
  failed_request.response_error = 500;
  failed_request.response_error_body = "Internal Server Error";
  error.failed_request = std::move(failed_request);

  base::test::TestFuture<RefreshResult> future;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      [](std::optional<FailedRequest> failed_request,
         SessionError::ErrorType error_type,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        SessionError error(error_type);
        error.failed_request = std::move(failed_request);
        std::move(callback).Run(nullptr, RegistrationResult(std::move(error)));
      },
      error.failed_request, error.type));
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
}

TEST_F(SessionServiceImplTest, EventObserverOnProactiveRefresh) {
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

  // Attach the required cookie, but make it expire very soon.
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // Trigger proactive refresh.
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().ShouldDefer(*dbsc_request, &extra_headers, FirstPartySetMetadata());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_FALSE(details.new_session_display.has_value());
    EXPECT_TRUE(details.was_fully_proactive_refresh);
  });

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         /*maybe_stored_cookies=*/{}));
}

TEST_F(SessionServiceImplTest, EventObserverOnProactiveAndDeferredRefresh) {
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

  // Attach the required cookie, but make it expire very soon.
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // Trigger proactive refresh.
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().ShouldDefer(*dbsc_request, &extra_headers, FirstPartySetMetadata());

  // Defer the request.
  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));
  base::test::TestFuture<RefreshResult> future;
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  dbsc_request = std::make_unique<DbscRequest>(request.get());
  service().DeferRequestForRefresh(*dbsc_request, deferral,
                                   future.GetCallback());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(
        std::holds_alternative<RefreshEventDetails>(event.event_type_details));
    const auto& details =
        std::get<RefreshEventDetails>(event.event_type_details);
    EXPECT_EQ(details.fetch_error, SessionError::kSuccess);
    ASSERT_FALSE(details.new_session_display.has_value());
    EXPECT_FALSE(details.was_fully_proactive_refresh);
  });

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         /*maybe_stored_cookies=*/{}));
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshedAsWaiter);
}

TEST_F(SessionServiceImplTest, EventObserverOnChallenge) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(kSessionChallengeHeaderName,
                     R"("challenge";id="SessionId")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1u);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_TRUE(event.succeeded);
    ASSERT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    const auto& details =
        std::get<ChallengeEventDetails>(event.event_type_details);
    EXPECT_EQ(details.challenge_result, ChallengeResult::kSuccess);
    EXPECT_EQ(details.challenge, "challenge");
  });

  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        FirstPartySetMetadata(), params[0]);
}

TEST_F(SessionServiceImplTest, EventObserverOnChallenge_NoSessionId) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(kSessionChallengeHeaderName, R"("challenge")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1u);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_FALSE(event.session_id.has_value());
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    const auto& details =
        std::get<ChallengeEventDetails>(event.event_type_details);
    EXPECT_EQ(details.challenge_result, ChallengeResult::kNoSessionId);
    EXPECT_EQ(details.challenge, "challenge");
  });

  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        FirstPartySetMetadata(), params[0]);
}

TEST_F(SessionServiceImplTest, EventObserverOnChallenge_NoSessionMatch) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(kSessionChallengeHeaderName,
                     R"("challenge";id="SessionId2")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1u);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, "SessionId2");
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    const auto& details =
        std::get<ChallengeEventDetails>(event.event_type_details);
    EXPECT_EQ(details.challenge_result, ChallengeResult::kNoSessionMatch);
    EXPECT_EQ(details.challenge, "challenge");
  });

  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        FirstPartySetMetadata(), params[0]);
}

TEST_F(SessionServiceImplTest, EventObserverOnChallenge_CantSetBoundCookie) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  scoped_refptr<net::HttpResponseHeaders> headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  headers->AddHeader(kSessionChallengeHeaderName,
                     R"("challenge";id="SessionId")");

  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(kTestUrl, headers.get());
  ASSERT_EQ(params.size(), 1u);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  network_delegate()->set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);
  DbscRequest dbsc_request(request.get());

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_)).WillOnce([](const SessionEvent& event) {
    EXPECT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
    EXPECT_EQ(event.session_id, kSessionId);
    EXPECT_FALSE(event.succeeded);
    ASSERT_TRUE(std::holds_alternative<ChallengeEventDetails>(
        event.event_type_details));
    const auto& details =
        std::get<ChallengeEventDetails>(event.event_type_details);
    EXPECT_EQ(details.challenge_result, ChallengeResult::kCantSetBoundCookie);
    EXPECT_EQ(details.challenge, "challenge");
  });

  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        FirstPartySetMetadata(), params[0]);
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

TEST_F(SessionServiceImplTest, GetAllSessionDisplays) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin},
                         {kSessionId2, kRefreshUrlString2, kOrigin2}});

  base::test::TestFuture<std::vector<SessionDisplay>> future;
  service().GetAllSessionDisplaysAsync(
      future.GetCallback<const std::vector<SessionDisplay>&>());
  std::vector<SessionDisplay> session_displays = future.Take();
  ASSERT_EQ(session_displays.size(), 2);
  EXPECT_EQ(std::string(session_displays[0].key.id), kSessionId);
  EXPECT_EQ(session_displays[0].refresh_url, kRefreshUrlString);
  EXPECT_EQ(session_displays[0].inclusion_rules.origin, kOrigin);
  EXPECT_EQ(std::string(session_displays[1].key.id), kSessionId2);
  EXPECT_EQ(session_displays[1].refresh_url, kRefreshUrlString2);
  EXPECT_EQ(session_displays[1].inclusion_rules.origin, kOrigin2);
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
                         unexportable_keys::UnexportableSigningKeyId(),
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

class SessionServiceImplSystemTimeTest : public SessionServiceImplTest {
 public:
  // NOTE: We can't use MOCK_TIME here, since the task environment does not
  // support going back in time.
  SessionServiceImplSystemTimeTest()
      : SessionServiceImplTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
};

TEST_F(SessionServiceImplSystemTimeTest, PrunesFutureSignings) {
  base::test::ScopedFeatureList feature_list(
      features::kDeviceBoundSessionSigningQuotaAndCaching);
  SessionKey session_key{SchemefulSite(GURL(kTestUrl)),
                         Session::Id(kSessionId)};

  // Add signings in the future
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        [] { return base::Time::Max(); }, nullptr, nullptr);
    for (size_t i = 0; i < 10; ++i) {
      service().AddSigningOccurrence(session_key.site);
    }
  }

  // Back to present time, verify these future signings are discarded.
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
      .key_id = unexportable_keys::UnexportableSigningKeyId()};
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
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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


TEST_F(SessionServiceImplTestWithoutFederatedSessions,
       IgnoresFederatedRegistration) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);

  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  service().ShouldDefer(dbsc_request, &extra_headers, FirstPartySetMetadata());

  SessionKey session_key{SchemefulSite(kTestRefreshUrl),
                         Session::Id(kSessionId)};
  EXPECT_EQ(request->device_bound_session_usage().size(), 1);
  EXPECT_EQ(request->device_bound_session_usage().at(session_key),
            SessionUsage::kDeferred);
}

class SessionServiceImplRequestModeTest
    : public SessionServiceImplTest,
      public ::testing::WithParamInterface<net::DeviceBoundSessionMode> {};

TEST_P(SessionServiceImplRequestModeTest, ShouldDefer) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_device_bound_session_mode(GetParam());

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> deferral =
      service().ShouldDefer(dbsc_request, &extra_headers,
                            FirstPartySetMetadata());

  if (GetParam() == net::DeviceBoundSessionMode::kAllowed) {
    EXPECT_TRUE(deferral.has_value());
  } else {
    EXPECT_FALSE(deferral.has_value());
  }
}

TEST_P(SessionServiceImplRequestModeTest, HandleRegistrationHeader) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_device_bound_session_mode(GetParam());

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("Secure-Session-Registration",
                     "(ES256);path=\"register\";challenge=\"challenge\"");

  // Let registration succeed if request is made.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  DbscRequest dbsc_request(request.get());
  service().HandleResponseHeaders(dbsc_request, headers.get(),
                                  FirstPartySetMetadata());

  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());

  if (GetParam() == net::DeviceBoundSessionMode::kDisabled) {
    EXPECT_THAT(future.Get(), IsEmpty());
  } else {
    EXPECT_THAT(future.Get(), Not(IsEmpty()));
  }
}

TEST_P(SessionServiceImplRequestModeTest, HandleChallengeHeader) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  SessionKey session_key{SchemefulSite(GURL(kOrigin)), Session::Id(kSessionId)};
  Session* session = service().GetSession(session_key);
  ASSERT_TRUE(session);
  EXPECT_FALSE(session->cached_challenge().has_value());

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_device_bound_session_mode(GetParam());

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("Secure-Session-Challenge",
                     "\"challenge\";id=\"" + std::string(kSessionId) + "\"");

  DbscRequest dbsc_request(request.get());
  service().HandleResponseHeaders(dbsc_request, headers.get(),
                                  FirstPartySetMetadata());

  if (GetParam() == net::DeviceBoundSessionMode::kDisabled) {
    EXPECT_FALSE(session->cached_challenge().has_value());
  } else {
    EXPECT_TRUE(session->cached_challenge().has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SessionServiceImplRequestModeTest,
    ::testing::Values(net::DeviceBoundSessionMode::kDisabled,
                      net::DeviceBoundSessionMode::kBypassDeferral,
                      net::DeviceBoundSessionMode::kAllowed));

}  // namespace

class SessionServiceImplWithStoreTest : public TestWithTaskEnvironment {
 public:
  SessionServiceImplWithStoreTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()),
        store_(std::make_unique<StrictMock<SessionStoreMock>>()),
        service_(unexportable_key_service_,
                 context_.get(),
                 store_.get(),
                 /*restricted_sites=*/std::vector<SchemefulSite>()) {
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
      task_manager_, kTaskOrigin, crypto::UnexportableKeyProvider::Config()};
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
    EXPECT_CALL(store(), SaveSession);
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
      /*creds=*/{}, unexportable_keys::UnexportableSigningKeyId(),
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
      .WillOnce(SaveArgByMove<1>(&restore_key_callback));
  service().DeferRequestForRefresh(*dbsc_request, *maybe_deferral,
                                   base::DoNothing());
  // Simulate the request being cleaned up before the callback has been called.
  dbsc_request.reset();
  request.reset();
  ASSERT_TRUE(restore_key_callback);
  // Call the callback, and the test should not crash even though the request
  // was cleaned up.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());
}

TEST_F(SessionServiceImplWithStoreTest,
       WaiterRequestResumedIfTriggerCanceledDuringKeyRestoration) {
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

  // Create request1 that should be deferred due to the session
  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate1, kDummyAnnotation);
  request1->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers1;
  auto dbsc_request1 = std::make_unique<DbscRequest>(request1.get());
  std::optional<SessionService::DeferralParams> deferral1 =
      service().ShouldDefer(*dbsc_request1, &extra_headers1,
                            FirstPartySetMetadata());
  ASSERT_TRUE(deferral1);
  EXPECT_EQ(**deferral1->session_id, kSessionId);

  // Now actually defer request1 to trigger RestoreSessionBindingKey
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(SaveArgByMove<1>(&restore_key_callback));
  service().DeferRequestForRefresh(*dbsc_request1, *deferral1,
                                   base::DoNothing());
  ASSERT_TRUE(restore_key_callback);

  // Create request2 and defer it while RestoreSessionBindingKey is pending
  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate2, kDummyAnnotation);
  request2->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers2;
  DbscRequest dbsc_request2(request2.get());
  std::optional<SessionService::DeferralParams> deferral2 =
      service().ShouldDefer(dbsc_request2, &extra_headers2,
                            FirstPartySetMetadata());
  ASSERT_TRUE(deferral2);

  base::test::TestFuture<RefreshResult> future2;
  service().DeferRequestForRefresh(dbsc_request2, *deferral2,
                                   future2.GetCallback());

  // Now cancel/destroy request1 before the key restoration completes
  dbsc_request1.reset();
  request1.reset();

  // Set expectations for the refresh completion
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);

  // Resolve the key restoration.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());

  EXPECT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Take(), RefreshResult::kRefreshed);
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
  EXPECT_CALL(store(), SaveSession);
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(
          RunOnceCallback<1>(unexportable_keys::UnexportableSigningKeyId()));

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   future.GetCallback());

  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest, RecoveryFromTransientSigningError) {
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

  // Create Request A
  net::TestDelegate delegate_a;
  std::unique_ptr<URLRequest> request_a =
      context()->CreateRequest(kTestUrl, IDLE, &delegate_a, kDummyAnnotation);
  request_a->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers_a;
  DbscRequest dbsc_request_a(request_a.get());
  std::optional<SessionService::DeferralParams> maybe_deferral_a =
      service().ShouldDefer(dbsc_request_a, &extra_headers_a,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral_a);

  // Mock transient failure for Request A
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback_a;
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(SaveArgByMove<1>(&restore_key_callback_a));

  base::test::TestFuture<RefreshResult> future_a;
  service().DeferRequestForRefresh(dbsc_request_a, *maybe_deferral_a,
                                   future_a.GetCallback());

  ASSERT_TRUE(restore_key_callback_a);
  // Fail with transient error
  std::move(restore_key_callback_a)
      .Run(base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));

  EXPECT_EQ(future_a.Take(), RefreshResult::kTransientSigningError);

  // Create Request B
  net::TestDelegate delegate_b;
  std::unique_ptr<URLRequest> request_b =
      context()->CreateRequest(kTestUrl, IDLE, &delegate_b, kDummyAnnotation);
  request_b->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers_b;
  DbscRequest dbsc_request_b(request_b.get());
  std::optional<SessionService::DeferralParams> maybe_deferral_b =
      service().ShouldDefer(dbsc_request_b, &extra_headers_b,
                            FirstPartySetMetadata());
  ASSERT_TRUE(maybe_deferral_b);

  // Mock success for Request B
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(
          RunOnceCallback<1>(unexportable_keys::UnexportableSigningKeyId()));

  base::test::TestFuture<RefreshResult> future_b;
  service().DeferRequestForRefresh(dbsc_request_b, *maybe_deferral_b,
                                   future_b.GetCallback());

  EXPECT_EQ(future_b.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest, FederatedRegistrationKeyUnrestored) {
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

  Session* provider_session =
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create the provider key and the correct thumbprint, but leave it
  // unrestored.
  provider_session->set_unexportable_key_id(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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

  // Mock persistent failure for RestoreSessionBindingKey
  EXPECT_CALL(
      store(),
      RestoreSessionBindingKey(
          SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)), _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(unexportable_keys::ServiceError::kKeyNotFound)));

  EXPECT_CALL(store(), DeleteSession(SessionKey(SchemefulSite(kTestUrl),
                                                Session::Id(kSessionId))))
      .Times(1);

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The relying session will not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  // Because we failed to restore the key with a persistent error, we also
  // deleted the provider session.
  EXPECT_EQ(
      service().GetSession({SchemefulSite(kTestUrl), Session::Id(kSessionId)}),
      nullptr);
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kFailedToUnwrapKey, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey, 1);
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

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);
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

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
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
  EXPECT_CALL(store(), SaveSession);
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

TEST_F(SessionServiceImplTest, DeferredWaitersCanTriggerAnotherRefresh) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create two requests.
  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate1, kDummyAnnotation);
  request1->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request1(request1.get());

  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate2, kDummyAnnotation);
  request2->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request2(request2.get());

  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  base::test::TestFuture<RefreshResult> future1;
  base::test::TestFuture<RefreshResult> future2;

  // Defer the first request. This should trigger a refresh.
  service().DeferRequestForRefresh(dbsc_request1, deferral,
                                   future1.GetCallback());

  // Defer the second request. This should NOT trigger a refresh.
  service().DeferRequestForRefresh(dbsc_request2, deferral,
                                   future2.GetCallback());

  // Only one refresh actually happened.
  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Resolve the refresh successfully.
  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         /*maybe_stored_cookies=*/{}));

  // Verify callbacks.
  RefreshResult result1 = future1.Take();
  EXPECT_EQ(result1, RefreshResult::kRefreshed);

  RefreshResult result2 = future2.Take();
  EXPECT_EQ(result2, RefreshResult::kRefreshedAsWaiter);

  // Simulate what `URLRequestHttpJob` does when it receives the callback.
  SessionKey session_key{site, Session::Id(kSessionId)};
  request1->AddDeviceBoundSessionDeferral(session_key, result1);
  request2->AddDeviceBoundSessionDeferral(session_key, result2);

  // Verify that `ShouldDefer()` returns false for the first request
  // (already refreshed) but true for the second one (waiter gets a second
  // chance).
  HttpRequestHeaders extra_headers1;
  auto deferral1 = service().ShouldDefer(dbsc_request1, &extra_headers1,
                                         FirstPartySetMetadata());
  EXPECT_FALSE(deferral1.has_value());

  HttpRequestHeaders extra_headers2;
  auto deferral2 = service().ShouldDefer(dbsc_request2, &extra_headers2,
                                         FirstPartySetMetadata());
  EXPECT_TRUE(deferral2.has_value());
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
  SessionKey session_key{site, Session::Id(kSessionId)};
  EXPECT_EQ(request->device_bound_session_usage().size(), 1);
  EXPECT_EQ(request->device_bound_session_usage().at(session_key),
            SessionUsage::kInScopeProactiveRefreshAttempted);

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
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshedAsWaiter);

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

  SessionKey session_key{site, Session::Id(kSessionId)};
  EXPECT_EQ(request->device_bound_session_usage().size(), 1);
  EXPECT_EQ(request->device_bound_session_usage().at(session_key),
            SessionUsage::kInScopeProactiveRefreshNotPossible);
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

TEST_F(SessionServiceImplTest, NoProactiveRefreshNeededYet) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and try to defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Attach the required cookie. Set expiration for 10 minutes for now (beyond
  // the proactive refresh threshold).
  CookieInclusionStatus status;
  auto source = CookieSourceType::kHTTP;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=600", base::Time::Now(),
      std::nullopt, std::nullopt, source, &status);
  ASSERT_TRUE(cookie);
  CookieAccessResult access_result;
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});

  // We should not want to defer this request, and it should not trigger a
  // proactive refresh.
  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers,
                            FirstPartySetMetadata());
  ASSERT_FALSE(maybe_deferral);
  SessionKey session_key{site, Session::Id(kSessionId)};
  EXPECT_EQ(request->device_bound_session_usage().size(), 1);
  EXPECT_EQ(request->device_bound_session_usage().at(session_key),
            SessionUsage::kInScopeRefreshNotYetNeeded);
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
          /*creds=*/{}, unexportable_keys::UnexportableSigningKeyId(),
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
