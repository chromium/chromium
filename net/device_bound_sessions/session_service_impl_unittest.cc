// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include <algorithm>
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
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/optional_ref.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/keypair.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/device_bound_sessions/challenge_result.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/mock_session_store.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/device_bound_sessions/single_sign_on_key_manager.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/log/test_net_log.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/device_bound_session_mode.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
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
constexpr char kRegisterUrlString[] = "https://example.com/register";
const GURL kTestRegisterUrl(kRegisterUrlString);
constexpr char kSessionId[] = "SessionId";
constexpr char kOrigin[] = "https://example.com";

constexpr char kUrlString2[] = "https://example2.com";
const GURL kTestUrl2(kUrlString2);
constexpr char kRefreshUrlString2[] = "https://example2.com/refresh";
const GURL kTestRefreshUrl2(kRefreshUrlString2);
constexpr char kSessionId2[] = "SessionId2";
constexpr char kOrigin2[] = "https://example2.com";

constexpr char kSessionId3[] = "SessionId3";

constexpr char kChallenge[] = "challenge";

constexpr char kSessionChallengeHeaderName[] = "Secure-Session-Challenge";

constexpr char kRelyingPartySessionId[] = "RelyingPartySessionId";
constexpr char kRelyingPartyOrigin[] = "https://rp.test";
constexpr char kIdentityProviderOrigin[] = "https://provider.test";
constexpr char kProviderKey[] = "key_digest_123";

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
  session_proto.set_wrapped_key("mock_wrapped_key");
  return session_proto;
}

// Returns a session for `kRelyingPartyOrigin` bound to `key_id`.
base::expected<std::unique_ptr<Session>, SessionError>
CreateRelyingPartySession(std::string_view session_id,
                          unexportable_keys::UnexportableSigningKeyId key_id) {
  return Session::CreateIfValid(SessionParams{
      .session_id = std::string(session_id),
      .fetcher_url = GURL(kRelyingPartyOrigin).Resolve("/refresh"),
      .refresh_url = base::StrCat({kRelyingPartyOrigin, "/refresh"}),
      .scope =
          {
              .include_site = true,
              .origin = kRelyingPartyOrigin,
          },
      .credentials =
          {
              {
                  .name = "test_cookie",
                  .attributes = "secure",
              },
          },
      .key_id = key_id,
  });
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

struct CapturedRequestContext {
  SiteForCookies site_for_cookies;
  IsolationInfo isolation_info;
  std::optional<url::Origin> initiator;
};

void ExpectContext(const CapturedRequestContext& captured,
                   const SiteForCookies& expected_site_for_cookies,
                   const IsolationInfo& expected_isolation_info,
                   const std::optional<url::Origin>& expected_initiator) {
  EXPECT_TRUE(
      captured.site_for_cookies.IsEquivalent(expected_site_for_cookies));
  EXPECT_TRUE(
      captured.isolation_info.IsEqualForTesting(expected_isolation_info));
  EXPECT_EQ(captured.initiator, expected_initiator);
}

std::unique_ptr<URLRequest> CreateRequestWithContext(
    URLRequestContext* context,
    const GURL& url,
    const url::Origin& origin,
    URLRequest::Delegate* delegate) {
  const SiteForCookies site_for_cookies = SiteForCookies::FromOrigin(origin);
  const IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, origin, origin, site_for_cookies);
  std::unique_ptr<URLRequest> request =
      context->CreateRequest(url, IDLE, delegate, kDummyAnnotation,
                             net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(site_for_cookies);
  request->set_isolation_info(isolation_info);
  request->set_initiator(origin);
  return request;
}

class RequestContextCapturingInterceptor : public URLRequestInterceptor {
 public:
  explicit RequestContextCapturingInterceptor(
      base::RepeatingCallback<void(CapturedRequestContext)> on_intercept,
      bool auto_advance = true)
      : auto_advance_(auto_advance), on_intercept_(std::move(on_intercept)) {}

  ~RequestContextCapturingInterceptor() override = default;

  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    CapturedRequestContext context{
        .site_for_cookies = request->site_for_cookies(),
        .isolation_info = request->isolation_info(),
        .initiator = request->initiator(),
    };
    if (on_intercept_) {
      on_intercept_.Run(std::move(context));
    }
    return std::make_unique<URLRequestTestJob>(request, "HTTP/1.1 200 OK\n\n",
                                               /*response_data=*/"",
                                               auto_advance_);
  }

 private:
  const bool auto_advance_ = true;
  base::RepeatingCallback<void(CapturedRequestContext)> on_intercept_;
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
        /*restricted_sites=*/std::vector<SchemefulSite>(),
        /*has_cookie_access_cb=*/base::NullCallback(),
        /*client_cert_handler=*/base::DoNothing());
  }

  void TearDown() override {
    // Reset the `network_delegate_` to avoid a dangling pointer.
    network_delegate_ = nullptr;
    net::URLRequestFilter::GetInstance()->ClearHandlers();
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
          GURL(url_str), {crypto::sign::ECDSA_SHA256}, "challenge",
          /*authorization=*/std::nullopt);
      service().RegisterBoundSession(
          base::DoNothing(), std::move(fetch_param),
          IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
          SiteForCookies(), NetLogWithSource(),
          /*original_request_initiator=*/std::nullopt);
    }
  }

  void SetService(std::unique_ptr<SessionServiceImpl> service) {
    service_ = std::move(service);
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
    AddScopedFeatureList().InitAndEnableFeatureWithParameters(
        net::features::kDeviceBoundSessions, {{"RefreshQuota", "false"}});
  }
};

class SessionServiceImplTestWithFederatedSessions
    : public SessionServiceImplTest {
 public:
  SessionServiceImplTestWithFederatedSessions() {
    AddScopedFeatureList().InitAndEnableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
  }
};

class SessionServiceImplTestWithoutFederatedSessions
    : public SessionServiceImplTest {
 public:
  SessionServiceImplTestWithoutFederatedSessions() {
    AddScopedFeatureList().InitAndDisableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
  }
};

TEST_F(SessionServiceImplTest, RegisterSuccess) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);
}

TEST_F(SessionServiceImplTest, RegisterNullFetcher) {
  auto scoped_null_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kNetError, kRefreshUrlString);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  DbscRequest dbsc_request(request.get());
  for (const auto& param : params) {
    service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                          param);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  network_delegate()->set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);

  ASSERT_EQ(params.size(), 1U);
  DbscRequest dbsc_request(request.get());
  service().SetChallengeForBoundSession(base::DoNothing(), dbsc_request,
                                        params[0]);

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers);

  EXPECT_GT(session->expiry_date(), base::Time::Now() + base::Days(399));
}

TEST_F(SessionServiceImplTest, NullAccessObserver) {
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The access observer was null, so no call is expected
}

TEST_F(SessionServiceImplTest, AccessObserverCalledOnRegistration) {
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  base::test::TestFuture<SessionAccess> future;
  service().RegisterBoundSession(
      future.GetRepeatingCallback<const SessionAccess&>(),
      std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<SessionAccess> future;
  request->SetDeviceBoundSessionAccessCallback(
      future.GetRepeatingCallback<const SessionAccess&>());
  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers);

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  base::test::TestFuture<SessionAccess> future;
  DbscRequest dbsc_request(request.get());
  service().SetChallengeForBoundSession(
      future.GetRepeatingCallback<const SessionAccess&>(), dbsc_request,
      params[0]);

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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kInvalidFetcherUrl, kRefreshUrlString);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
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
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      {crypto::sign::ECDSA_SHA256},
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

  SessionParams params{
      .session_id = kSessionId,
      .fetcher_url = kTestUrl,
      .refresh_url = kRefreshUrlString,
      .scope = {.origin = kOrigin},
  };

  base::test::TestFuture<SessionError::ErrorType> add_session_future;
  service().AddSession(SchemefulSite(kTestUrl), std::move(params), wrapped_key,
                       add_session_future.GetCallback());
  EXPECT_EQ(add_session_future.Take(), SessionError::kSuccess);
}

TEST_F(SessionServiceImplTest, AddSessionReplacesExistingSession) {
  base::HistogramTester histograms;
  net::SchemefulSite site(kTestUrl);
  SessionKey session_key{site, Session::Id(kSessionId)};

  // Initial registration of session.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  ASSERT_TRUE(service().GetSession(session_key));

  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription subscription =
      service().AddEventObserver(event_callback.Get());

  {
    InSequence seq;
    EXPECT_CALL(event_callback, Run).WillOnce([](const SessionEvent& event) {
      ASSERT_TRUE(std::holds_alternative<TerminationEventDetails>(
          event.event_type_details));
      EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
      EXPECT_EQ(event.session_id, kSessionId);
      EXPECT_TRUE(event.succeeded);
      const auto& details =
          std::get<TerminationEventDetails>(event.event_type_details);
      EXPECT_EQ(details.deletion_reason, DeletionReason::kReplaced);
    });
    EXPECT_CALL(event_callback, Run).WillOnce([](const SessionEvent& event) {
      ASSERT_TRUE(std::holds_alternative<CreationEventDetails>(
          event.event_type_details));
      EXPECT_EQ(event.site, SchemefulSite(kTestUrl));
      EXPECT_EQ(event.session_id, kSessionId);
      EXPECT_TRUE(event.succeeded);
    });
  }

  // Registering again with same session id replaces the existing session.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  EXPECT_TRUE(service().GetSession(session_key));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kReplaced, 1);
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      })
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
  service().ShouldDefer(*dbsc_request, &extra_headers);

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
  service().ShouldDefer(*dbsc_request, &extra_headers);

  // Defer the request.
  auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));
  base::test::TestFuture<RefreshResult> future;
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
                                        params[0]);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
                                        params[0]);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
                                        params[0]);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
                                        params[0]);
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

// Verifies that the session's creation date isn't updated when its
// configuration changes at a refresh.
// Regression test for https://crbug.com/512365319.
TEST_F(SessionServiceImplTest, RefreshedSessionKeepsCreationDate) {
  net::SchemefulSite site(kTestUrl);

  // Register a session on the site.
  AddSessionsForTesting({{"SessionA", kRefreshUrlString, kOrigin}});
  Session* session = service().GetSession({site, Session::Id("SessionA")});
  ASSERT_TRUE(session);
  base::Time original_creation_time = base::Time::Now();

  // Fast-forward time by 10 days to emulate an "old" session.
  FastForwardBy(base::Days(10));
  // Verify session is still at the old creation date before the refresh.
  EXPECT_EQ(session->creation_date(), original_creation_time);

  // Perform a refresh request.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "SessionA", kRefreshUrlString, kOrigin);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> refresh_future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id("SessionA")),
      refresh_future.GetCallback());
  EXPECT_EQ(refresh_future.Take(), RefreshResult::kRefreshed);

  // Check that the refresh updated the session while preserving its original
  // creation date.
  Session* refreshed_session =
      service().GetSession({site, Session::Id("SessionA")});
  ASSERT_TRUE(refreshed_session);
  EXPECT_EQ(refreshed_session->creation_date(), original_creation_time);

  // Fast-forward time by another 15 minutes so that the refresh is now 15
  // minutes in the past.
  FastForwardBy(base::Minutes(15));

  // User triggers a "Clear Browsing Data" for the "Last Hour".
  // Deletion time range: [Now - 1 Hour, Now].
  base::Time delete_begin = base::Time::Now() - base::Hours(1);
  base::Time delete_end = base::Time::Now();

  base::RunLoop run_loop;
  service().DeleteAllSessions(DeletionReason::kStoragePartitionCleared,
                              delete_begin, delete_end,
                              /*origin_and_site_matcher=*/
                              base::NullCallback(), run_loop.QuitClosure());
  run_loop.Run();

  // The session should be preserved because it was created 10 days ago.
  EXPECT_TRUE(service().GetSession({site, Session::Id("SessionA")}));
}

// Verifies that the session's attestation key state is preserved when its
// configuration changes at a refresh.
TEST_F(SessionServiceImplTest, RefreshedSessionKeepsAttestationKey) {
  net::SchemefulSite site(kTestUrl);

  // Generate a fake attestation key.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableAttestationKeyId>>
      generate_future;
  key_service()->GenerateAttestationKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kUserBlocking,
      generate_future.GetCallback());
  auto key_or_error = generate_future.Get();
  ASSERT_TRUE(key_or_error.has_value());
  unexportable_keys::UnexportableAttestationKeyId attestation_key_id =
      *key_or_error;

  // Register a session on the site, with the attestation key.
  {
    auto custom_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
        [](unexportable_keys::UnexportableAttestationKeyId attestation_key_id,
           const std::string& session_id, const std::string& refresh_url_string,
           const std::string& origin_string,
           RegistrationFetcher::RegistrationCompleteCallback callback) {
          std::move(callback).Run(
              /*fetcher=*/nullptr,
              RegistrationResult(Session::CreateIfValid(SessionParams{
                  .session_id = session_id,
                  .fetcher_url = GURL(refresh_url_string),
                  .refresh_url = refresh_url_string,
                  .scope = {.include_site = true, .origin = origin_string},
                  .credentials = {{
                      .name = "test_cookie",
                      .attributes = "secure",
                  }},
                  .attestation_key_id = attestation_key_id,
              })));
        },
        attestation_key_id, "SessionA", kRefreshUrlString, kOrigin));

    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        GURL(kRefreshUrlString), {crypto::sign::ECDSA_SHA256}, "challenge",
        /*authorization=*/std::nullopt);
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(),
        /*original_request_initiator=*/std::nullopt);
  }

  Session* session = service().GetSession({site, Session::Id("SessionA")});
  ASSERT_TRUE(session);
  EXPECT_THAT(session->maybe_unexportable_attestation_key_id(),
              base::test::ValueIs(attestation_key_id));

  // Perform a refresh request. The refresh response does NOT have attestation.
  auto scoped_test_refresh_fetcher =
      ScopedTestRegistrationFetcher::CreateWithSuccess(
          "SessionA", kRefreshUrlString, kOrigin);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> refresh_future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id("SessionA")),
      refresh_future.GetCallback());
  EXPECT_EQ(refresh_future.Take(), RefreshResult::kRefreshed);

  // Check that the refresh updated the session while preserving its
  // attestation key state.
  Session* refreshed_session =
      service().GetSession({site, Session::Id("SessionA")});
  ASSERT_TRUE(refreshed_session);
  EXPECT_THAT(refreshed_session->maybe_unexportable_attestation_key_id(),
              base::test::ValueIs(attestation_key_id));
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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

TEST_F(SessionServiceImplTest, DeferWithPersistentKeyRestoreError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site(kTestUrl);
  SessionKey session_key(site, Session::Id(kSessionId));
  Session* session = service().GetSession(session_key);
  ASSERT_TRUE(session);

  // Set the key to a persistent error.
  session->set_unexportable_key_id(base::unexpected(
      unexportable_keys::ServiceError::kAlgorithmNotSupported));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  FakeDeviceBoundSessionObserver observer;
  request->SetDeviceBoundSessionAccessCallback(observer.GetCallback());

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());

  // Should fail immediately and delete the session.
  EXPECT_EQ(future.Get(), RefreshResult::kFatalError);
  EXPECT_FALSE(service().GetSession(session_key));

  // Verify observer notifications (update then termination).
  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(
          SessionAccess{SessionAccess::AccessType::kUpdate, session_key},
          SessionAccess{SessionAccess::AccessType::kTermination, session_key,
                        std::vector<std::string>{"test_cookie"}}));
}

TEST_F(SessionServiceImplTest, TestDeferWithRequestContinue_NonFatalError) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  SchemefulSite site_1(kTestUrl);
  ASSERT_TRUE(service().GetSession({site_1, Session::Id(kSessionId)}));

  // Create a request to kTestUrl and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
        std::move(callback).Run(nullptr,
                                RegistrationResult(Session::CreateIfValid(
                                    SessionParams{.session_id = kSessionId})));
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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

TEST_F(SessionServiceImplTest, SessionSigningQuota) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  SessionKey session_key{SchemefulSite(GURL(kRefreshUrlString)),
                         Session::Id(kSessionId)};
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
  base::HistogramTester histogram_tester;
  for (size_t i = 0; i < 6; i++) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        session_key.site, SigningQuotaOperation::kRefresh));
  }
  // The next signing exceeds the signing quota.
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      session_key.site, SigningQuotaOperation::kRefresh));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded.Refresh"),
      BucketsAre(base::Bucket(1, 1), base::Bucket(2, 1), base::Bucket(3, 1),
                 base::Bucket(4, 1), base::Bucket(5, 1), base::Bucket(6, 1),
                 base::Bucket(7, 1)));

  // This affects other sessions on the same site, but not other sessions on a
  // different site.
  SessionKey session_key2{SchemefulSite(GURL(kRefreshUrlString)),
                          Session::Id(kSessionId2)};
  SessionKey session_key3{SchemefulSite(GURL(kRefreshUrlString2)),
                          Session::Id(kSessionId3)};
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      session_key2.site, SigningQuotaOperation::kRefresh));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      session_key3.site, SigningQuotaOperation::kRefresh));

  // After 9 minutes, the quota is restored.
  FastForwardBy(base::Minutes(9));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      session_key.site, SigningQuotaOperation::kRefresh));

  // Both refusals on `session_key.site` happened in the same out-of-quota
  // episode, so only the first one is recorded.
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.SigningQuotaExceededLastResult2",
      SessionError::kSuccess, 1);
}

TEST_F(SessionServiceImplTest, SessionSigningQuotaAttestedRegistration) {
  const SchemefulSite site{GURL(kRefreshUrlString)};
  base::HistogramTester histogram_tester;

  // Each attested registration costs 3 units out of the 6-unit quota.
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_FALSE(
      service().TryConsumeSigningQuota(site, SigningQuotaOperation::kRefresh));

  // The refused refresh is in the same out-of-quota episode as the refused
  // attested registration, so it isn't recorded.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded."
          "AttestedRegistration"),
      BucketsAre(base::Bucket(3, 1), base::Bucket(6, 1), base::Bucket(9, 1)));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded.Refresh", 0);

  FastForwardBy(base::Minutes(9));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));

  // The earlier signings aged out, so the refusal in the new window starts a
  // new out-of-quota episode and is recorded again.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded."
          "AttestedRegistration"),
      BucketsAre(base::Bucket(3, 2), base::Bucket(6, 2), base::Bucket(9, 2)));
}

TEST_F(SessionServiceImplTest,
       SessionSigningQuotaEpisodeEndsWhenSigningsAgeOut) {
  const SchemefulSite site{GURL(kRefreshUrlString)};
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(
      service().TryConsumeSigningQuota(site, SigningQuotaOperation::kRefresh));
  EXPECT_TRUE(
      service().TryConsumeSigningQuota(site, SigningQuotaOperation::kRefresh));
  FastForwardBy(base::Minutes(5));
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        site, SigningQuotaOperation::kRefresh));
  }
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));

  // The first two refreshes age out, freeing 2 units. That ends the episode
  // even though the site is still out of quota for an attested registration.
  FastForwardBy(base::Minutes(4));
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded."
                  "AttestedRegistration"),
              BucketsAre(base::Bucket(7, 1), base::Bucket(9, 1)));
}

TEST_F(SessionServiceImplTest, SessionSigningQuotaNoPartialConsumption) {
  const SchemefulSite site{GURL(kRefreshUrlString)};
  base::HistogramTester histogram_tester;

  // Consume 4 units, leaving 2 units in the quota.
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kRegistration));

  // An attested registration (cost 3) does not fit and must not consume the
  // remaining 2 units.
  EXPECT_FALSE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kAttestedRegistration));

  // Two cost-1 operations still succeed before the quota is exhausted.
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      site, SigningQuotaOperation::kRegistration));
  EXPECT_TRUE(
      service().TryConsumeSigningQuota(site, SigningQuotaOperation::kRefresh));
  EXPECT_FALSE(
      service().TryConsumeSigningQuota(site, SigningQuotaOperation::kRefresh));

  // Operations that fit don't end the out-of-quota episode that the refused
  // attested registration started, so the final refused refresh isn't
  // recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded."
                  "AttestedRegistration"),
              BucketsAre(base::Bucket(3, 1), base::Bucket(7, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded.Registration"),
      BucketsAre(base::Bucket(4, 1), base::Bucket(5, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded.Refresh"),
              BucketsAre(base::Bucket(6, 1)));
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
  SessionKey session_key{SchemefulSite(GURL(kTestUrl)),
                         Session::Id(kSessionId)};

  // Add signings in the future
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        [] { return base::Time::Max(); }, nullptr, nullptr);
    for (size_t i = 0; i < 6; ++i) {
      EXPECT_TRUE(service().TryConsumeSigningQuota(
          session_key.site, SigningQuotaOperation::kRefresh));
    }
  }

  // Back to present time, verify these future signings are discarded.
  EXPECT_TRUE(service().TryConsumeSigningQuota(
      session_key.site, SigningQuotaOperation::kRefresh));
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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

TEST_F(SessionServiceImplNoRefreshQuotaTest, SigningQuotaDisabled) {
  const SchemefulSite site{GURL(kRefreshUrlString)};
  base::HistogramTester histogram_tester;

  // Three attested registrations (9 units) fit because the quota is disabled.
  for (size_t i = 0; i < 3; i++) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        site, SigningQuotaOperation::kAttestedRegistration));
  }
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Net.DeviceBoundSessions.SigningQuotaUnitsNeeded."),
              IsEmpty());
}

TEST_F(SessionServiceImplTest, SessionBackoff) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kTransientHttpError, kRefreshUrlString);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  EXPECT_TRUE(service().ShouldDefer(dbsc_request, &extra_headers));

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);
  EXPECT_FALSE(maybe_deferral->is_pending_initialization);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kRefreshed);

  maybe_deferral = service().ShouldDefer(dbsc_request, &extra_headers);
  EXPECT_FALSE(maybe_deferral);
}

TEST_F(SessionServiceImplTest, AddsDebugHeader) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  AddSessionsForTesting({{kSessionId2, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // Simulate failed refresh for two different sessions
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kUnreachable);
  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId2)},
      RefreshResult::kSigningQuotaExceeded);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  EXPECT_FALSE(maybe_deferral);

  std::optional<std::string> debug_header =
      extra_headers.GetHeader("Secure-Session-Skipped");
  EXPECT_TRUE(debug_header.has_value());
  EXPECT_EQ(*debug_header,
            "unreachable;session_identifier=\"SessionId\", "
            "quota_exceeded;session_identifier=\"SessionId2\"");
}

TEST_F(SessionServiceImplTest, NoDebugHeaderOnSuccess) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

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
      service().ShouldDefer(dbsc_request, &extra_headers);
  EXPECT_FALSE(maybe_deferral);

  std::optional<std::string> debug_header =
      extra_headers.GetHeader("Secure-Session-Skipped");
  EXPECT_FALSE(debug_header.has_value());
}

TEST_F(SessionServiceImplTest, NoDebugHeaderOnInScopeRefreshNotYetNeeded) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  request->AddDeviceBoundSessionDeferral(
      SessionKey{SchemefulSite(kTestUrl), Session::Id(kSessionId)},
      RefreshResult::kInScopeRefreshNotYetNeeded);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  ASSERT_OK_AND_ASSIGN(crypto::keypair::PublicKey public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           *key_service()->GetSubjectPublicKeyInfo(key)));
  ASSERT_OK_AND_ASSIGN(std::string key_thumbprint,
                       CreateJwkThumbprint(public_key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = key_thumbprint,
          .provider_url = kTestUrl,
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session exists.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_NE(relying_session, nullptr);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationSuccessRsa) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Create an RSA provider key and the correct thumbprint
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::RSA_PKCS1_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(unexportable_keys::UnexportableSigningKeyId key,
                       key_future.Take());
  ASSERT_OK_AND_ASSIGN(crypto::keypair::PublicKey public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           *key_service()->GetSubjectPublicKeyInfo(key)));
  ASSERT_TRUE(public_key.IsRsa());
  ASSERT_OK_AND_ASSIGN(std::string key_thumbprint,
                       CreateJwkThumbprint(public_key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::RSA_PKCS1_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = key_thumbprint,
          .provider_url = kTestUrl,
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  provider_session->set_unexportable_key_id(key);

  base::HistogramTester histograms;

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = "not_the_thumbprint",
          .provider_url = kTestRefreshUrl,
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kFederatedKeyThumbprintMismatch,
                                1);
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationInvalidProviderKey) {
  // Create the provider session
  SchemefulSite site(kTestUrl);
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  Session* provider_session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_NE(provider_session, nullptr);

  // Bind the provider session to a key id that is unknown to the key service,
  // so that computing its thumbprint fails.
  provider_session->set_unexportable_key_id(
      unexportable_keys::UnexportableSigningKeyId());

  base::HistogramTester histograms;

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = "key-thumbprint",
          .provider_url = kTestRefreshUrl,
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kInvalidFederatedKey, 1);
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
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  ASSERT_OK_AND_ASSIGN(crypto::keypair::PublicKey public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           *key_service()->GetSubjectPublicKeyInfo(key)));
  ASSERT_OK_AND_ASSIGN(std::string key_thumbprint,
                       CreateJwkThumbprint(public_key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = key_thumbprint,
          .provider_url = kTestRefreshUrl,
          .provider_session_id = Session::Id("incorrect-provider-session")});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  ASSERT_OK_AND_ASSIGN(crypto::keypair::PublicKey public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           *key_service()->GetSubjectPublicKeyInfo(key)));
  ASSERT_OK_AND_ASSIGN(std::string key_thumbprint,
                       CreateJwkThumbprint(public_key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider, specifying a
  // subdomain of the site as the provider. Since the session applies to
  // all of `example.com`, the provider should be `example.com`, not
  // `subdomain.example.com`.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = key_thumbprint,
          .provider_url = GURL("https://subdomain.example.com"),
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = "key-thumbprint",
          .provider_url = GURL("http:///"),
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the relying session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kInvalidFederatedSessionUrl, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
          Pair("Net.DeviceBoundSessions.RegistrationDuration.Federated.Failure",
               1)));
}

TEST_F(SessionServiceImplTestWithFederatedSessions,
       FederatedRegistrationOpaqueOrigin) {
  base::HistogramTester histograms;

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = "key-thumbprint",
          .provider_url = GURL("data:text/html,session-provider"),
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key = *key_future.Take();
  ASSERT_OK_AND_ASSIGN(crypto::keypair::PublicKey public_key,
                       crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                           *key_service()->GetSubjectPublicKeyInfo(key)));
  ASSERT_OK_AND_ASSIGN(std::string key_thumbprint,
                       CreateJwkThumbprint(public_key));
  provider_session->set_unexportable_key_id(key);

  // Attempt a registration with a session provider
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{
          .provider_key = key_thumbprint,
          .provider_url = kTestUrl,
          .provider_session_id = Session::Id(kSessionId)});
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      kTestUrl, {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);

  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);

  // Registration failed, so should not be valid
  EXPECT_FALSE(maybe_deferral);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kEmptySessionConfig, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
          Pair(
              "Net.DeviceBoundSessions.RegistrationDuration.Standalone.Failure",
              1)));
}

TEST_F(SessionServiceImplTest, EmptyResponseOnRefresh) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  base::HistogramTester histograms;

  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  // The request needs to be samesite for it to be considered
  // candidate for deferral.
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RefreshDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RefreshDuration", 1),
          Pair("Net.DeviceBoundSessions.RefreshDuration.MissingCookie.Success",
               1)));
}

TEST_F(SessionServiceImplTest, SessionUsage) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  service().ShouldDefer(dbsc_request, &extra_headers);

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);

  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  service().ShouldDefer(dbsc_request, &extra_headers);

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_device_bound_session_mode(GetParam());

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);

  if (GetParam() == net::DeviceBoundSessionMode::kAllowed) {
    EXPECT_TRUE(deferral.has_value());
  } else {
    EXPECT_FALSE(deferral.has_value());
  }
}

TEST_P(SessionServiceImplRequestModeTest, HandleRegistrationHeader) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_device_bound_session_mode(GetParam());

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("Secure-Session-Registration",
                     "(ES256);path=\"register\";challenge=\"challenge\"");

  // Let registration succeed if request is made.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  DbscRequest dbsc_request(request.get());
  service().HandleResponseHeaders(dbsc_request, headers.get());

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_device_bound_session_mode(GetParam());

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("Secure-Session-Challenge",
                     "\"challenge\";id=\"" + std::string(kSessionId) + "\"");

  DbscRequest dbsc_request(request.get());
  service().HandleResponseHeaders(dbsc_request, headers.get());

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
                 /*restricted_sites=*/std::vector<SchemefulSite>(),
                 /*has_cookie_access_cb=*/base::NullCallback(),
                 /*client_cert_handler=*/base::DoNothing()) {
    AddScopedFeatureList().InitAndEnableFeature(
        net::features::kDeviceBoundSessionsFederatedRegistration);
  }

  SessionServiceImpl& service() { return service_; }
  StrictMock<SessionStoreMock>& store() { return *store_; }

  crypto::ScopedMockUnexportableKeyProvider& SwitchToMockKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    return scoped_key_provider_
        .emplace<crypto::ScopedMockUnexportableKeyProvider>();
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

  // Returns the number of registration and refresh fetches in progress.
  size_t GetOngoingRegistrationFetchesCount() {
    return service().registration_fetchers_.size();
  }

  struct FederatedRegistrationSetup {
    SessionKey provider_session_key;
    unexportable_keys::UnexportableSigningKeyId provider_key;
    RegistrationFetcherParam fetch_param;
  };

  // Finishes loading sessions with a provider session `kSessionId` on
  // `kTestUrl` and generates a key for it, which is not installed on the
  // session (using it requires a restore). Returns params to register the
  // federated session "RelyingSession" on https://rp.com with that provider.
  FederatedRegistrationSetup SetUpFederatedRegistration() {
    EXPECT_CALL(store(), LoadSessions).Times(1);
    service().LoadSessionsAsync();

    std::unique_ptr<Session> session =
        Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString))
            .value();
    SessionKey provider_session_key{SchemefulSite(kTestUrl), session->id()};
    SessionStore::SessionsMap session_map;
    session_map.insert({provider_session_key, std::move(session)});
    FinishLoadingSessions(std::move(session_map));

    base::test::TestFuture<unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableSigningKeyId>>
        key_future;
    key_service()->GenerateSigningKeySlowlyAsync(
        {crypto::sign::ECDSA_SHA256},
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        key_future.GetCallback());
    unexportable_keys::UnexportableSigningKeyId key = key_future.Take().value();
    std::string key_thumbprint =
        CreateJwkThumbprint(
            crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
                *key_service()->GetSubjectPublicKeyInfo(key))
                .value())
            .value();

    return FederatedRegistrationSetup{
        .provider_session_key = std::move(provider_session_key),
        .provider_key = key,
        .fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
            kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
            /*authorization=*/std::nullopt,
            ProviderRegistrationParams{
                .provider_key = std::move(key_thumbprint),
                .provider_url = kTestUrl,
                .provider_session_id = Session::Id(kSessionId)}),
    };
  }

  SessionError::ErrorType OnRefreshRequestCompletionInternal(
      SessionService::OnAccessCallback on_access_callback,
      const SessionKey& session_key,
      RegistrationFetcher* fetcher,
      RegistrationResult result) {
    return service().OnRefreshRequestCompletionInternal(
        std::move(on_access_callback), session_key, fetcher, std::move(result));
  }

  void CallOnGetCookiesForPrewarm(
      const GURL& url,
      std::vector<SessionKey> matching_sessions,
      SessionService::PrewarmCallback callback,
      const CookieAccessResultList& cookies,
      const CookieAccessResultList& excluded_cookies) {
    service().OnGetCookiesForPrewarm(url, std::move(matching_sessions),
                                     std::move(callback), cookies,
                                     excluded_cookies);
  }

  void CallOnGetCookiesAfterProactiveRefresh(
      const SessionKey& session_key,
      RefreshResult refresh_result,
      const CookieAccessResultList& cookies,
      const CookieAccessResultList& excluded_cookies,
      base::OnceCallback<void(RefreshResult, base::Time)> callback) {
    std::vector<base::OnceCallback<void(SessionServiceImpl::PrewarmResult)>>
        callbacks;
    callbacks.push_back(base::BindOnce(
        [](base::OnceCallback<void(RefreshResult, base::Time)> cb,
           SessionServiceImpl::PrewarmResult result) {
          std::move(cb).Run(result.result, result.earliest_next_refresh_time);
        },
        std::move(callback)));
    service().OnGetCookiesAfterProactiveRefresh(
        session_key, std::move(callbacks), refresh_result, cookies,
        excluded_cookies);
  }

  // Loads a session for `kTestUrl` whose binding key has not been restored, so
  // that refreshing it first restores the key asynchronously. The key restore
  // is expected once, and its callback is saved to `restore_key_callback`.
  void LoadSessionWithUnrestoredKey(
      SessionStore::RestoreSessionBindingKeyCallback* restore_key_callback) {
    EXPECT_CALL(store(), LoadSessions);
    service().LoadSessionsAsync();
    ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<Session> session,
        Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
    SessionStore::SessionsMap session_map;
    session_map.insert({SessionKey{SchemefulSite(kTestUrl), session->id()},
                        std::move(session)});
    FinishLoadingSessions(std::move(session_map));

    EXPECT_CALL(
        store(),
        RestoreSessionBindingKey(
            SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
            unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
        .WillOnce(SaveArgByMove<2>(restore_key_callback));
  }

  // Defers a new request on the session loaded by
  // `LoadSessionWithUnrestoredKey()`, then destroys the request.
  void DeferRequestThenDestroyIt(
      SessionService::RefreshCompleteCallback callback) {
    net::TestDelegate delegate;
    std::unique_ptr<URLRequest> request = CreateRequestWithContext(
        context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate);
    DbscRequest dbsc_request(request.get());
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        std::move(callback));
  }

  // Loads sessions "session_1" and "session_2" for `kTestUrl`, along with
  // `other_sessions`, then defers `request` on "session_1", whose key restore
  // is left pending. Like `URLRequestHttpJob`, the request is re-evaluated
  // synchronously when its deferral is unblocked, and it is deferred again on
  // "session_2". The key restore for "session_2" fails synchronously, which
  // deletes that session and runs `callback`. Sessions 1 and 2 are each
  // expected to be deleted from the store once.
  void LoadSessionsAndDeferReentrantRequest(
      URLRequest* request,
      SessionService::RefreshCompleteCallback callback,
      SessionStore::SessionsMap other_sessions = {}) {
    const SchemefulSite site(kTestUrl);
    const SessionKey session_key_1{site, Session::Id("session_1")};
    const SessionKey session_key_2{site, Session::Id("session_2")};

    EXPECT_CALL(store(), LoadSessions).Times(1);
    service().LoadSessionsAsync();
    ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<Session> session_1,
        Session::CreateFromProto(CreateSessionProto("session_1", kUrlString)));
    ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<Session> session_2,
        Session::CreateFromProto(CreateSessionProto("session_2", kUrlString)));
    SessionStore::SessionsMap session_map = std::move(other_sessions);
    session_map.insert({session_key_1, std::move(session_1)});
    session_map.insert({session_key_2, std::move(session_2)});
    FinishLoadingSessions(std::move(session_map));

    HttpRequestHeaders extra_headers;
    DbscRequest dbsc_request(request);
    std::optional<SessionService::DeferralParams> deferral =
        service().ShouldDefer(dbsc_request, &extra_headers);
    ASSERT_TRUE(deferral);
    EXPECT_EQ(deferral->session_id, session_key_1.id);

    // Key restore for session 1 stays pending; for session 2 it fails inline.
    EXPECT_CALL(
        store(),
        RestoreSessionBindingKey(
            session_key_1,
            unexportable_keys::BackgroundTaskPriority::kUserBlocking, _));
    EXPECT_CALL(
        store(),
        RestoreSessionBindingKey(
            session_key_2,
            unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
        .WillOnce(RunOnceCallback<2>(
            base::unexpected(unexportable_keys::ServiceError::kKeyNotFound)));
    EXPECT_CALL(store(), DeleteSession(session_key_1)).Times(1);
    EXPECT_CALL(store(), DeleteSession(session_key_2)).Times(1);

    // Model `URLRequestHttpJob`: on unblock, record the deferral and
    // synchronously re-evaluate the request, deferring it again on session 2.
    service().DeferRequestForRefresh(
        dbsc_request, *deferral,
        base::BindLambdaForTesting(
            [this, request, session_key_1, session_key_2,
             callback = std::move(callback)](RefreshResult result) mutable {
              EXPECT_EQ(result, RefreshResult::kFatalError);
              EXPECT_EQ(service().GetSession(session_key_1), nullptr);
              request->AddDeviceBoundSessionDeferral(session_key_1, result);
              DbscRequest restarted_request(request);
              HttpRequestHeaders restarted_extra_headers;
              std::optional<SessionService::DeferralParams> next_deferral =
                  service().ShouldDefer(restarted_request,
                                        &restarted_extra_headers);
              ASSERT_TRUE(next_deferral);
              EXPECT_EQ(next_deferral->session_id, session_key_2.id);
              service().DeferRequestForRefresh(
                  restarted_request, *next_deferral,
                  base::BindLambdaForTesting(
                      [this, session_key_2, callback = std::move(callback)](
                          RefreshResult inner_result) mutable {
                        EXPECT_EQ(service().GetSession(session_key_2), nullptr);
                        std::move(callback).Run(inner_result);
                      }));
            }));
  }

  void TearDown() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
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
               crypto::ScopedMockUnexportableKeyProvider>
      scoped_key_provider_;
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
    EXPECT_CALL(store(), SaveSession);
    EXPECT_CALL(store(), DeleteSession).Times(1);
  }

  // Will invoke the store's load session method.
  service().LoadSessionsAsync();

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  // Will invoke the store's save session method.
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  auto site = SchemefulSite(kTestUrl);
  Session* session = service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(GetSiteSessionsCount(site), 1u);
  session->set_expiry_date(base::Time::Now() - base::Days(1));
  // Will invoke the store's delete session method.
  EXPECT_EQ(GetSiteSessionsCount(site), 0u);
}

TEST_F(SessionServiceImplWithStoreTest,
       AddSessionReplacesExistingSessionInStore) {
  base::HistogramTester histograms;
  EXPECT_CALL(store(), LoadSessions)
      .Times(1)
      .WillOnce(
          Invoke(this, &SessionServiceImplWithStoreTest::OnSessionsLoaded));
  EXPECT_CALL(store(), SaveSession(SchemefulSite(kTestUrl), _,
                                   SessionStore::SaveSessionMode::kNewSession))
      .Times(1);

  service().LoadSessionsAsync();

  {
    auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
        kSessionId, kRefreshUrlString, kOrigin);
    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
        /*authorization=*/std::nullopt);
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(),
        /*original_request_initiator=*/std::nullopt);
  }

  auto site = SchemefulSite(kTestUrl);
  SessionKey session_key{site, Session::Id(kSessionId)};
  ASSERT_TRUE(service().GetSession(session_key));

  // When registering again with the same session id, the store should be
  // notified to delete the old session, and then save the new session.
  {
    InSequence seq;
    EXPECT_CALL(store(), DeleteSession(session_key));
    EXPECT_CALL(
        store(),
        SaveSession(site, _, SessionStore::SaveSessionMode::kNewSession));
  }

  {
    auto scoped_test_fetcher2 =
        ScopedTestRegistrationFetcher::CreateWithSuccess(
            kSessionId, kRefreshUrlString, kOrigin);
    auto fetch_param2 = RegistrationFetcherParam::CreateInstanceForTesting(
        kTestUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
        /*authorization=*/std::nullopt);
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param2),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(),
        /*original_request_initiator=*/std::nullopt);
  }

  EXPECT_TRUE(service().GetSession(session_key));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kReplaced, 1);
}

TEST_F(SessionServiceImplWithStoreTest, GetAllSessionsWaitsForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Request sessions, which should wait until we finish loading.
  base::test::TestFuture<std::vector<SessionKey>> future;
  service().GetAllSessionsAsync(
      future.GetCallback<const std::vector<SessionKey>&>());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = "session_id",
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
  ASSERT_TRUE(session);

  // Complete loading. If we did not defer, we'd miss this session.
  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // But we did defer, so we found it.
  EXPECT_THAT(future.Take(), UnorderedElementsAre(ExpectId("session_id")));
}

TEST_F(SessionServiceImplWithStoreTest,
       DeleteAllSessionsWaitsForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Call DeleteAllSessions, which should wait until we finish loading.
  base::test::TestFuture<void> future;
  service().DeleteAllSessions(DeletionReason::kStoragePartitionCleared,
                              std::nullopt, std::nullopt, base::NullCallback(),
                              future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = "session_id",
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
  ASSERT_TRUE(session);

  // Expected deletion call when DeleteAllSessions finally runs.
  EXPECT_CALL(store(), DeleteSession).Times(1);

  // Complete loading. The session is loaded, but then DeleteAllSessions should
  // run.
  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // The DeleteAllSessions callback should have run.
  EXPECT_TRUE(future.Wait());

  // The session should be deleted.
  EXPECT_EQ(GetSiteSessionsCount(SchemefulSite(kTestUrl)), 0u);
}

// Deleting a session unblocks its deferred requests synchronously. A restarted
// request can then defer on another session whose key restore fails
// synchronously, which deletes that session while DeleteAllSessions() is still
// iterating.
TEST_F(SessionServiceImplWithStoreTest,
       DeleteAllSessionsUnblocksRequestTriggeringKeyRestoreFailure) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  base::test::TestFuture<RefreshResult> final_refresh_future;
  ASSERT_NO_FATAL_FAILURE(LoadSessionsAndDeferReentrantRequest(
      request.get(), final_refresh_future.GetCallback()));

  const SchemefulSite site(kTestUrl);
  const SessionKey session_key_1{site, Session::Id("session_1")};
  const SessionKey session_key_2{site, Session::Id("session_2")};
  const base::Time creation_time_1 = base::Time::Now() - base::Hours(2);
  const base::Time creation_time_2 = base::Time::Now() - base::Hours(1);
  service().GetSession(session_key_1)->set_creation_date(creation_time_1);
  service().GetSession(session_key_2)->set_creation_date(creation_time_2);

  // Only session 1 matches the filter (`created_before_time` is inclusive).
  base::test::TestFuture<void> delete_future;
  service().DeleteAllSessions(DeletionReason::kClearBrowsingData,
                              /*created_after_time=*/std::nullopt,
                              /*created_before_time=*/creation_time_1,
                              base::NullCallback(),
                              delete_future.GetCallback());

  EXPECT_TRUE(delete_future.Wait());
  ASSERT_TRUE(final_refresh_future.IsReady());
  EXPECT_EQ(final_refresh_future.Take(), RefreshResult::kFatalError);
  EXPECT_EQ(service().GetSession(session_key_1), nullptr);
  EXPECT_EQ(service().GetSession(session_key_2), nullptr);
}

// Deleting an expired session in GetSessionsForSite() unblocks a request that
// synchronously re-enters the service before the deletion finishes.
TEST_F(SessionServiceImplWithStoreTest,
       GetSessionsForSiteExpiredSessionUnblocksRequestReentrantly) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  const SchemefulSite site(kTestUrl);
  const SessionKey session_key_1{site, Session::Id("session_1")};
  const SessionKey session_key_2{site, Session::Id("session_2")};
  const SessionKey session_key_3{site, Session::Id("session_3")};
  // Also load a live session 3, which sorts after the other two sessions.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session_3,
      Session::CreateFromProto(CreateSessionProto("session_3", kUrlString)));
  SessionStore::SessionsMap other_sessions;
  other_sessions.insert({session_key_3, std::move(session_3)});
  base::test::TestFuture<RefreshResult> final_refresh_future;
  ASSERT_NO_FATAL_FAILURE(LoadSessionsAndDeferReentrantRequest(
      request.get(), final_refresh_future.GetCallback(),
      std::move(other_sessions)));

  Session* session_1 = service().GetSession(session_key_1);
  ASSERT_TRUE(session_1);
  session_1->set_expiry_date(base::Time::Now() - base::Seconds(1));

  // Calls GetSessionsForSite(), which deletes the expired session 1 (and
  // re-entrantly deletes session 2) and returns the remaining live session 3.
  EXPECT_EQ(GetSiteSessionsCount(site), 1u);
  ASSERT_TRUE(final_refresh_future.IsReady());
  EXPECT_EQ(final_refresh_future.Take(), RefreshResult::kFatalError);
  EXPECT_EQ(service().GetSession(session_key_1), nullptr);
  EXPECT_EQ(service().GetSession(session_key_2), nullptr);
  EXPECT_NE(service().GetSession(session_key_3), nullptr);
}

TEST_F(SessionServiceImplWithStoreTest,
       DeleteSessionAndNotifyWaitsForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Call DeleteSessionAndNotify, which should wait until we finish loading.
  SessionKey session_key{SchemefulSite(kTestUrl), Session::Id("session_id")};
  service().DeleteSessionAndNotify(DeletionReason::kStoragePartitionCleared,
                                   session_key, base::NullCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = "session_id",
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
  ASSERT_TRUE(session);

  // Expected deletion call when DeleteSessionAndNotify finally runs.
  EXPECT_CALL(store(), DeleteSession).Times(1);

  // Complete loading.
  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // The session should be deleted.
  EXPECT_EQ(GetSiteSessionsCount(SchemefulSite(kTestUrl)), 0u);
}

TEST_F(SessionServiceImplWithStoreTest, RequestsWaitForSessionsToLoad) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  // Create a request that should be deferred due to initialization not
  // having completed.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
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

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  auto dbsc_request = std::make_unique<DbscRequest>(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(*dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);

  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback));
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

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create request1 that should be deferred due to the session
  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate1, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request1->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers1;
  auto dbsc_request1 = std::make_unique<DbscRequest>(request1.get());
  std::optional<SessionService::DeferralParams> deferral1 =
      service().ShouldDefer(*dbsc_request1, &extra_headers1);
  ASSERT_TRUE(deferral1);
  EXPECT_EQ(**deferral1->session_id, kSessionId);

  // Now actually defer request1 to trigger RestoreSessionBindingKey
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback));
  service().DeferRequestForRefresh(*dbsc_request1, *deferral1,
                                   base::DoNothing());
  ASSERT_TRUE(restore_key_callback);

  // Create request2 and defer it while RestoreSessionBindingKey is pending
  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate2, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request2->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers2;
  DbscRequest dbsc_request2(request2.get());
  std::optional<SessionService::DeferralParams> deferral2 =
      service().ShouldDefer(dbsc_request2, &extra_headers2);
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

TEST_F(SessionServiceImplWithStoreTest,
       WaiterPromotedIfTriggerCanceledDuringKeyRestorationUsesWaiterContext) {
  // Start loading.
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateFromProto(
                           CreateSessionProto(kSessionId, kRefreshUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  base::test::TestFuture<CapturedRequestContext> intercept_future;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRefreshUrl, std::make_unique<RequestContextCapturingInterceptor>(
                           intercept_future.GetRepeatingCallback()));

  // Create request1 (context A) that should be deferred due to missing cookie.
  const url::Origin origin1 = url::Origin::Create(kTestUrl);
  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      CreateRequestWithContext(context(), kTestUrl, origin1, &delegate1);
  auto dbsc_request1 = std::make_unique<DbscRequest>(request1.get());

  const auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  // Defer request1 to trigger asynchronous RestoreSessionBindingKey.
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback));
  service().DeferRequestForRefresh(*dbsc_request1, deferral, base::DoNothing());
  ASSERT_TRUE(restore_key_callback);

  // Create request2 (context B) and defer it while RestoreSessionBindingKey is
  // pending.
  const url::Origin origin2 = url::Origin::Create(GURL("https://other.test"));
  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      CreateRequestWithContext(context(), kTestUrl, origin2, &delegate2);

  ASSERT_FALSE(
      request1->site_for_cookies().IsEquivalent(request2->site_for_cookies()));
  ASSERT_FALSE(
      request1->isolation_info().IsEqualForTesting(request2->isolation_info()));

  DbscRequest dbsc_request2(request2.get());
  base::test::TestFuture<RefreshResult> future2;
  service().DeferRequestForRefresh(dbsc_request2, deferral,
                                   future2.GetCallback());

  // Cancel/destroy request1 before the key restoration completes.
  dbsc_request1.reset();
  request1.reset();

  EXPECT_CALL(store(), SaveSession);

  // Resolve the key restoration.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());

  // The promoted request2's context (context B) should have been passed to the
  // fetcher.
  ExpectContext(intercept_future.Take(), request2->site_for_cookies(),
                request2->isolation_info(), origin2);

  EXPECT_EQ(future2.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest,
       DrainDeferredRequestsWhenAllWaitersCanceledDuringKeyRestore) {
  AddScopedFeatureList().InitAndEnableFeature(
      features::kDeviceBoundSessionsDrainDeferredOnAllCanceled);
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  ASSERT_NO_FATAL_FAILURE(LoadSessionWithUnrestoredKey(&restore_key_callback));

  // The first request starts restoring the key, and the second one waits for
  // the same refresh. Both are destroyed before the key is restored.
  base::test::TestFuture<RefreshResult> trigger_future;
  DeferRequestThenDestroyIt(trigger_future.GetCallback());
  ASSERT_TRUE(restore_key_callback);
  base::test::TestFuture<RefreshResult> waiter_future;
  DeferRequestThenDestroyIt(waiter_future.GetCallback());

  // With no request left to refresh for, the queue is drained.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());
  EXPECT_EQ(trigger_future.Take(), RefreshResult::kCanceled);
  EXPECT_EQ(waiter_future.Take(), RefreshResult::kCanceled);

  // So a later request starts a new refresh instead of waiting on the abandoned
  // one.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate);
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest,
       AllWaitersCanceledDuringKeyRestoreDisabledByKillSwitch) {
  AddScopedFeatureList().InitAndDisableFeature(
      features::kDeviceBoundSessionsDrainDeferredOnAllCanceled);
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  ASSERT_NO_FATAL_FAILURE(LoadSessionWithUnrestoredKey(&restore_key_callback));

  base::test::TestFuture<RefreshResult> trigger_future;
  DeferRequestThenDestroyIt(trigger_future.GetCallback());
  ASSERT_TRUE(restore_key_callback);

  // The abandoned refresh is left in the queue.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());
  EXPECT_FALSE(trigger_future.IsReady());

  // So a later request waits on it instead of starting a new refresh.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate);
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(GetOngoingRegistrationFetchesCount(), 0u);
}

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_DeferredRequestsDestroyedDuringKeyRestoration) {
  AddScopedFeatureList().InitAndEnableFeature(
      features::kDeviceBoundSessionsDrainDeferredOnAllCanceled);
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  ASSERT_NO_FATAL_FAILURE(LoadSessionWithUnrestoredKey(&restore_key_callback));

  DeferRequestThenDestroyIt(base::DoNothing());
  ASSERT_TRUE(restore_key_callback);

  // The pre-warm waits for the pending refresh instead of starting its own.
  base::test::TestFuture<SessionPrewarmResult> prewarm_future;
  service().PrewarmSessionsForUrl(kTestUrl, prewarm_future.GetCallback());
  EXPECT_FALSE(prewarm_future.IsReady());

  // Draining the abandoned refresh completes the pre-warm too.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());
  EXPECT_THAT(prewarm_future.Get().results,
              ElementsAre(RefreshResult::kCanceled));
}

TEST_F(SessionServiceImplWithStoreTest, SessionKeyRestoredOnUse) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(
          RunOnceCallback<2>(unexportable_keys::UnexportableSigningKeyId()));

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   future.GetCallback());

  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest, RefreshDurationWithKeyRestoration) {
  base::HistogramTester histograms;
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create a request that should be deferred due to the session
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);
  EXPECT_EQ(**maybe_deferral->session_id, kSessionId);

  // Now actually defer the request
  constexpr base::TimeDelta kFetcherResponseDuration = base::Milliseconds(250);
  constexpr base::TimeDelta kKeyRestoreDuration = base::Milliseconds(100);
  constexpr base::TimeDelta kTotalExpectedDuration =
      kFetcherResponseDuration + kKeyRestoreDuration;

  auto scoped_test_fetcher =
      ScopedTestRegistrationFetcher(base::BindLambdaForTesting(
          [&](RegistrationFetcher::RegistrationCompleteCallback callback) {
            FastForwardBy(kFetcherResponseDuration);
            std::move(callback).Run(
                nullptr, RegistrationResult(
                             RegistrationResult::NoSessionConfigChange(), {}));
          }));
  EXPECT_CALL(store(), SaveSession);
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce([&](const SessionKey&,
                    unexportable_keys::BackgroundTaskPriority,
                    SessionStore::RestoreSessionBindingKeyCallback callback) {
        FastForwardBy(kKeyRestoreDuration);
        std::move(callback).Run(unexportable_keys::UnexportableSigningKeyId());
      });

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   future.GetCallback());
  EXPECT_EQ(future.Take(), RefreshResult::kRefreshed);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RefreshDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RefreshDuration", 1),
          Pair("Net.DeviceBoundSessions.RefreshDuration.MissingCookie.Success",
               1)));
  histograms.ExpectUniqueTimeSample("Net.DeviceBoundSessions.RefreshDuration",
                                    kTotalExpectedDuration, 1);
  histograms.ExpectUniqueTimeSample(
      "Net.DeviceBoundSessions.RefreshDuration.MissingCookie.Success",
      kTotalExpectedDuration, 1);
}

TEST_F(SessionServiceImplWithStoreTest, RecoveryFromTransientSigningError) {
  // Start loading
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  // Create Request A
  net::TestDelegate delegate_a;
  std::unique_ptr<URLRequest> request_a =
      context()->CreateRequest(kTestUrl, IDLE, &delegate_a, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request_a->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers_a;
  DbscRequest dbsc_request_a(request_a.get());
  std::optional<SessionService::DeferralParams> maybe_deferral_a =
      service().ShouldDefer(dbsc_request_a, &extra_headers_a);
  ASSERT_TRUE(maybe_deferral_a);

  // Mock transient failure for Request A
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback_a;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback_a));

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
      context()->CreateRequest(kTestUrl, IDLE, &delegate_b, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request_b->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers_b;
  DbscRequest dbsc_request_b(request_b.get());
  std::optional<SessionService::DeferralParams> maybe_deferral_b =
      service().ShouldDefer(dbsc_request_b, &extra_headers_b);
  ASSERT_TRUE(maybe_deferral_b);

  // Mock success for Request B
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(
          RunOnceCallback<2>(unexportable_keys::UnexportableSigningKeyId()));

  base::test::TestFuture<RefreshResult> future_b;
  service().DeferRequestForRefresh(dbsc_request_b, *maybe_deferral_b,
                                   future_b.GetCallback());

  EXPECT_EQ(future_b.Take(), RefreshResult::kRefreshed);
}

// A transient key restore error unblocks the waiting requests. A request that
// is deferred on the same session while they are being unblocked waits for its
// own refresh instead of failing with the first one.
TEST_F(SessionServiceImplWithStoreTest,
       TransientKeyRestoreErrorDoesNotFailReentrantDeferral) {
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback_a;
  ASSERT_NO_FATAL_FAILURE(
      LoadSessionWithUnrestoredKey(&restore_key_callback_a));

  net::TestDelegate delegate_a;
  std::unique_ptr<URLRequest> request_a = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate_a);
  DbscRequest dbsc_request_a(request_a.get());
  net::TestDelegate delegate_b;
  std::unique_ptr<URLRequest> request_b = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate_b);
  DbscRequest dbsc_request_b(request_b.get());
  const auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));

  // Request B is deferred on the same session when request A is unblocked.
  base::test::TestFuture<RefreshResult> future_a;
  base::test::TestFuture<RefreshResult> future_b;
  service().DeferRequestForRefresh(
      dbsc_request_a, deferral,
      base::BindLambdaForTesting([this, &future_a, &dbsc_request_b, &deferral,
                                  &future_b](RefreshResult result) {
        future_a.SetValue(result);
        service().DeferRequestForRefresh(dbsc_request_b, deferral,
                                         future_b.GetCallback());
      }));
  ASSERT_TRUE(restore_key_callback_a);

  // Request B starts a second key restore, which stays pending.
  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback_b;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  SessionKey(SchemefulSite(kTestUrl), Session::Id(kSessionId)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback_b));
  std::move(restore_key_callback_a)
      .Run(base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));
  EXPECT_EQ(future_a.Take(), RefreshResult::kTransientSigningError);
  ASSERT_FALSE(future_b.IsReady());
  ASSERT_TRUE(restore_key_callback_b);

  // Request B is unblocked when its own refresh completes.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);
  EXPECT_CALL(store(), SaveSession);
  std::move(restore_key_callback_b)
      .Run(unexportable_keys::UnexportableSigningKeyId());
  EXPECT_EQ(future_b.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_KeyRestorationFailure) {
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  SessionKey session_key{SchemefulSite(kTestUrl), Session::Id(kSessionId)};
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(unexportable_keys::ServiceError::kKeyNotFound)));

  EXPECT_CALL(store(), DeleteSession(session_key)).Times(1);

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results, ElementsAre(RefreshResult::kFatalError));
}

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_SessionDeletedDuringKeyRestoration) {
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  SessionKey session_key{SchemefulSite(kTestUrl), Session::Id(kSessionId)};

  SessionStore::RestoreSessionBindingKeyCallback restore_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(SaveArgByMove<2>(&restore_callback));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  // Now delete the session while key restoration is pending.
  EXPECT_CALL(store(), DeleteSession(session_key)).Times(1);
  service().DeleteSessionAndNotify(DeletionReason::kClearBrowsingData,
                                   session_key, base::DoNothing());

  // Running the restore callback afterwards should safely handle the deleted
  // session without crashing or hanging, resolving the prewarm future with
  // kFatalError.
  std::move(restore_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results, ElementsAre(RefreshResult::kFatalError));
}

TEST_F(SessionServiceImplWithStoreTest,
       DeferredRequestSessionDeletedDuringKeyRestoration) {
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  const SessionKey session_key{SchemefulSite(kTestUrl), session->id()};
  SessionStore::SessionsMap session_map;
  session_map.insert({session_key, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());
  HttpRequestHeaders extra_headers;
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);

  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback));
  base::test::TestFuture<RefreshResult> refresh_future;
  service().DeferRequestForRefresh(dbsc_request, *maybe_deferral,
                                   refresh_future.GetCallback());
  ASSERT_TRUE(restore_key_callback);
  EXPECT_FALSE(refresh_future.IsReady());

  // Deleting the session while its key is being restored unblocks the
  // deferred request.
  EXPECT_CALL(store(), DeleteSession(session_key)).Times(1);
  service().DeleteSessionAndNotify(DeletionReason::kClearBrowsingData,
                                   session_key, base::DoNothing());
  EXPECT_EQ(refresh_future.Get(), RefreshResult::kFatalError);

  // Finishing the restore afterwards runs the `kMissingCookie` refresh with no
  // key, which must return early without crashing.
  std::move(restore_key_callback)
      .Run(unexportable_keys::UnexportableSigningKeyId());
}

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_SessionDeletedDuringCookieFetch) {
  SessionKey non_existent_key{SchemefulSite(kTestUrl), Session::Id("deleted")};

  base::test::TestFuture<SessionPrewarmResult> future;
  CallOnGetCookiesForPrewarm(kTestUrl, {non_existent_key}, future.GetCallback(),
                             {}, {});

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results, ElementsAre(RefreshResult::kFatalError));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplWithStoreTest, FederatedRegistrationKeyUnrestored) {
  auto [provider_session_key, key, fetch_param] = SetUpFederatedRegistration();

  Session* provider_session = service().GetSession(provider_session_key);
  ASSERT_NE(provider_session, nullptr);

  // Leave the provider key unrestored.
  provider_session->set_unexportable_key_id(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));

  // Attempt a registration with a session provider
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");

  // Mock persistent failure for RestoreSessionBindingKey
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  provider_session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(unexportable_keys::ServiceError::kKeyNotFound)));

  EXPECT_CALL(store(), DeleteSession(provider_session_key)).Times(1);

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The relying session will not exist
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_EQ(relying_session, nullptr);

  // Because we failed to restore the key with a persistent error, we also
  // deleted the provider session.
  EXPECT_EQ(service().GetSession(provider_session_key), nullptr);
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kFailedToUnwrapKey, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey, 1);
}

TEST_F(SessionServiceImplWithStoreTest,
       FederatedProviderSessionDeletedDuringKeyRestore) {
  auto [provider_session_key, key, fetch_param] = SetUpFederatedRegistration();

  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");

  SessionStore::RestoreSessionBindingKeyCallback restore_key_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  provider_session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(SaveArgByMove<2>(&restore_key_callback));

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
  ASSERT_TRUE(restore_key_callback);

  // Delete the provider session while its key is being restored.
  EXPECT_CALL(store(), DeleteSession(provider_session_key)).Times(1);
  service().DeleteSessionAndNotify(DeletionReason::kClearBrowsingData,
                                   provider_session_key, base::DoNothing());

  // The registration still reports a result once the restore finishes.
  std::move(restore_key_callback).Run(key);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
          Pair("Net.DeviceBoundSessions.RegistrationDuration.Federated.Failure",
               1)));
  EXPECT_EQ(service().GetSession({SchemefulSite(GURL("https://rp.com")),
                                  Session::Id("RelyingSession")}),
            nullptr);
}

// A transient error restoring the provider's key for a federated registration
// doesn't unblock the requests waiting on a refresh of the provider session,
// which restores the key itself.
TEST_F(SessionServiceImplWithStoreTest,
       TransientFederatedKeyRestoreErrorDoesNotUnblockRefreshStartedEarlier) {
  auto [provider_session_key, provider_key, fetch_param] =
      SetUpFederatedRegistration();

  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kUrlString, kOrigin);

  // The registration starts restoring the provider key.
  SessionStore::RestoreSessionBindingKeyCallback federated_restore_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  provider_session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(SaveArgByMove<2>(&federated_restore_callback));
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);
  ASSERT_TRUE(federated_restore_callback);

  // A request is deferred on the provider session, and its refresh restores
  // the key too.
  SessionStore::RestoreSessionBindingKeyCallback refresh_restore_callback;
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  provider_session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _))
      .WillOnce(SaveArgByMove<2>(&refresh_restore_callback));
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate);
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> refresh_future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      refresh_future.GetCallback());
  ASSERT_TRUE(refresh_restore_callback);

  // The registration's restore fails with a transient error. The registration
  // fails, but the request keeps waiting for its refresh.
  std::move(federated_restore_callback)
      .Run(base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey, 1);
  ASSERT_FALSE(refresh_future.IsReady());

  // The refresh completes once its own restore succeeds.
  EXPECT_CALL(store(), SaveSession);
  std::move(refresh_restore_callback).Run(provider_key);
  EXPECT_EQ(refresh_future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplWithStoreTest, NoSessionUsageDuringInitialization) {
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);

  EXPECT_EQ(request->device_bound_session_usage().size(), 0);
}

TEST_F(SessionServiceImplWithStoreTest,
       FederatedProviderSessionKeyRestoredOnUse) {
  auto [provider_session_key, key, fetch_param] = SetUpFederatedRegistration();

  // Attempt a registration with a session provider
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      "RelyingSession", "https://rp.com/refresh", "https://rp.com");
  EXPECT_CALL(store(),
              RestoreSessionBindingKey(
                  provider_session_key,
                  unexportable_keys::BackgroundTaskPriority::kUserVisible, _))
      .WillOnce(RunOnceCallback<2>(key));
  EXPECT_CALL(store(), SaveSession);
  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // The relying session will exist, since we restored the provider key.
  Session* relying_session = service().GetSession(
      {SchemefulSite(GURL("https://rp.com")), Session::Id("RelyingSession")});
  EXPECT_NE(relying_session, nullptr);
  EXPECT_NE(service().GetSession(provider_session_key), nullptr);
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kSuccess, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
          Pair("Net.DeviceBoundSessions.RegistrationDuration.Federated.Success",
               1)));
}

TEST_F(SessionServiceImplWithStoreTest, RefreshNoConfigChangeSavesToStore) {
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = "session_id",
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
  ASSERT_TRUE(session);
  base::Time original_expiry = session->expiry_date();
  SchemefulSite site(kTestUrl);
  SessionKey session_key{site, session->id()};
  SessionStore::SessionsMap session_map;
  session_map.insert({session_key, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  base::TimeDelta delay = base::Days(1);
  FastForwardBy(delay);
  base::Time expected_expiry = original_expiry + delay;

  EXPECT_CALL(
      store(),
      SaveSession(site, Property(&Session::expiry_date, Eq(expected_expiry)),
                  SessionStore::SaveSessionMode::kRefresh))
      .Times(1);

  OnRefreshRequestCompletionInternal(
      base::NullCallback(), session_key,
      /*fetcher=*/nullptr,
      RegistrationResult(RegistrationResult::NoSessionConfigChange(), {}));

  Session* stored_session = service().GetSession(session_key);
  ASSERT_TRUE(stored_session);
  EXPECT_EQ(stored_session->expiry_date(), expected_expiry);
}

TEST_F(SessionServiceImplWithStoreTest,
       RefreshNoConfigChangeDisabledByKillSwitch) {
  AddScopedFeatureList().InitAndDisableFeature(
      features::kDeviceBoundSessionsPersistExpiryOnRefresh);

  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = "session_id",
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
  ASSERT_TRUE(session);
  base::Time original_expiry = session->expiry_date();
  SchemefulSite site(kTestUrl);
  SessionKey session_key{site, session->id()};
  SessionStore::SessionsMap session_map;
  session_map.insert({session_key, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  base::TimeDelta delay = base::Days(1);
  FastForwardBy(delay);
  base::Time expected_expiry = original_expiry + delay;

  EXPECT_CALL(store(),
              SaveSession(site, _, SessionStore::SaveSessionMode::kRefresh))
      .Times(0);

  OnRefreshRequestCompletionInternal(
      base::NullCallback(), session_key,
      /*fetcher=*/nullptr,
      RegistrationResult(RegistrationResult::NoSessionConfigChange(), {}));

  Session* stored_session = service().GetSession(session_key);
  ASSERT_TRUE(stored_session);
  EXPECT_EQ(stored_session->expiry_date(), expected_expiry);
}

TEST_F(SessionServiceImplTest, GoogleRegistrationLog) {
  base::HistogramTester histogram_tester;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL("https://accounts.google.com/"), {crypto::sign::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST),
      /*original_request_initiator=*/std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.GoogleRegistrationIsFromStandard", true, 1);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
          Pair(
              "Net.DeviceBoundSessions.RegistrationDuration.Standalone.Success",
              1)));
}

TEST_F(SessionServiceImplTest, NoGoogleRegistrationLog) {
  base::HistogramTester histogram_tester;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL("https://notgoogle.com/"), {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate1, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request1->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request1(request1.get());

  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate2, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
  auto deferral1 = service().ShouldDefer(dbsc_request1, &extra_headers1);
  EXPECT_FALSE(deferral1.has_value());

  HttpRequestHeaders extra_headers2;
  auto deferral2 = service().ShouldDefer(dbsc_request2, &extra_headers2);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      service().ShouldDefer(*dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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

// A proactive refresh started from ShouldDefer() can complete synchronously
// and delete the session that ShouldDefer() is iterating over.
TEST_F(SessionServiceImplTest,
       ShouldDeferHandlesSessionDeletedBySynchronousProactiveRefresh) {
  base::HistogramTester histograms;
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin},
                         {kSessionId3, kRefreshUrlString, kOrigin}});

  // Every refresh fails inline with an error that deletes the session.
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kSigningError, kRefreshUrlString);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  // The bound cookie expires soon, which triggers a proactive refresh instead
  // of a deferral.
  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  request->set_maybe_sent_cookies({{*cookie, CookieAccessResult()}});

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  EXPECT_FALSE(service().ShouldDefer(dbsc_request, &extra_headers));

  const SchemefulSite site(kTestUrl);
  EXPECT_FALSE(service().GetSession({site, Session::Id(kSessionId)}));
  EXPECT_FALSE(service().GetSession({site, Session::Id(kSessionId3)}));
  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.DeletionReason",
                                DeletionReason::kRefreshFatalError, 2);
}

// A refresh error that deletes the session removes it before the deferred
// requests are unblocked, because they can synchronously re-enter the service.
TEST_F(SessionServiceImplTest,
       FatalRefreshErrorDeletesSessionBeforeUnblockingRequests) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kSigningError, kRefreshUrlString);
  const SessionKey session_key{SchemefulSite(kTestUrl),
                               Session::Id(kSessionId)};

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request = CreateRequestWithContext(
      context(), kTestUrl, url::Origin::Create(kTestUrl), &delegate);
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> future;
  bool session_found_while_unblocking = true;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      base::BindLambdaForTesting([this, &session_found_while_unblocking,
                                  &session_key, &future](RefreshResult result) {
        session_found_while_unblocking = !!service().GetSession(session_key);
        future.SetValue(result);
      }));

  EXPECT_EQ(future.Take(), RefreshResult::kFatalError);
  EXPECT_FALSE(session_found_while_unblocking);
  EXPECT_EQ(service().GetSession(session_key), nullptr);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      service().ShouldDefer(*dbsc_request, &extra_headers);
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Another request should not do another proactive refresh
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  dbsc_request = std::make_unique<DbscRequest>(request.get());

  maybe_deferral = service().ShouldDefer(*dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      service().ShouldDefer(*dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      service().ShouldDefer(*dbsc_request, &extra_headers);
  ASSERT_FALSE(maybe_deferral);

  EXPECT_EQ(tracker.num_pending_refreshes(), 1);

  // Cause the proactive refresh to fail
  tracker.ResolvePendingRefresh(
      RegistrationResult(SessionError(SessionError::kTransientHttpError)));

  // Another request should not do another proactive refresh
  dbsc_request.reset();
  request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  request->set_maybe_sent_cookies({{*cookie.get(), access_result}});
  dbsc_request = std::make_unique<DbscRequest>(request.get());

  maybe_deferral = service().ShouldDefer(*dbsc_request, &extra_headers);
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
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RefreshDuration"),
      ElementsAre(
          Pair("Net.DeviceBoundSessions.RefreshDuration", 1),
          Pair("Net.DeviceBoundSessions.RefreshDuration.Proactive.Failure",
               1)));
}

TEST_F(SessionServiceImplTest, NoProactiveRefreshNeededYet) {
  // Register a session with kSessionId.
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto site = SchemefulSite(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  // Create a request and try to defer it.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
      service().ShouldDefer(*dbsc_request, &extra_headers);
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(SessionParams{
                           .session_id = kSessionId,
                           .fetcher_url = kTestUrl,
                           .refresh_url = kRefreshUrlString,
                           .scope = {.origin = kOrigin},
                       }));
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
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
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

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_PendingInitialization) {
  EXPECT_CALL(store(), LoadSessions).Times(1);
  service().LoadSessionsAsync();

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Set up a valid cookie so the loaded session doesn't need an immediate
  // refresh.
  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; HttpOnly; SameSite=Lax; Max-Age=500",
      base::Time::Now(), std::nullopt, std::nullopt, CookieSourceType::kHTTP,
      &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Session> session,
      Session::CreateFromProto(CreateSessionProto(kSessionId, kUrlString)));
  ASSERT_TRUE(session);

  SessionStore::SessionsMap session_map;
  session_map.insert(
      {SessionKey{SchemefulSite(kTestUrl), session->id()}, std::move(session)});
  FinishLoadingSessions(std::move(session_map));

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kInScopeRefreshNotYetNeeded));
  EXPECT_FALSE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_NoMatchingSessions) {
  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().results.empty());
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_InvalidUrl) {
  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(GURL("invalid-url"), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().results.empty());
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_FreshCookies) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kInScopeRefreshNotYetNeeded));
  ASSERT_FALSE(future.Get().earliest_next_refresh_time.is_max());
  EXPECT_NEAR((future.Get().earliest_next_refresh_time - base::Time::Now())
                  .InSecondsF(),
              380.0, 2.0);
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_MissingCookies) {
  base::HistogramTester histograms;
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RefreshResult.Proactive",
      RefreshResult::kRefreshed, 1);
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_ExpiringCookies) {
  base::HistogramTester histograms;
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=10", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_EQ(future.Get().earliest_next_refresh_time, base::Time::Now());
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RefreshResult.Proactive",
      RefreshResult::kRefreshed, 1);
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_MultiSessionBarrier) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin},
                         {kSessionId2, kRefreshUrlString, kOrigin}});

  auto scoped_dynamic_fetcher =
      ScopedTestRegistrationFetcher::CreateWithDynamicCallback(
          base::BindRepeating(
              [](const std::string& session_id,
                 const std::string& refresh_url_string,
                 RegistrationFetcher::RegistrationCompleteCallback callback) {
                std::move(callback).Run(
                    nullptr, RegistrationResult(SessionError{
                                 SessionError::kTransientHttpError}));
              },
              std::string(kSessionId), std::string(kRefreshUrlString)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kServerError,
                                   RefreshResult::kServerError));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_SessionInBackoff) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kTransientHttpError, kRefreshUrlString);

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());

  for (size_t i = 0; i < 4; i++) {
    FastForwardBy(base::Minutes(5));
    base::test::TestFuture<RefreshResult> future;
    service().DeferRequestForRefresh(
        dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
        future.GetCallback());
    EXPECT_EQ(future.Take(), RefreshResult::kServerError);
  }

  base::test::TestFuture<SessionPrewarmResult> prewarm_future;
  service().PrewarmSessionsForUrl(kTestUrl, prewarm_future.GetCallback());
  ASSERT_TRUE(prewarm_future.Wait());
  EXPECT_THAT(prewarm_future.Get().results,
              testing::ElementsAre(RefreshResult::kUnreachable));
  EXPECT_TRUE(prewarm_future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_NotifiesObserver) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  base::test::TestFuture<SessionAccess> observer_future;
  auto subscription = service().AddObserver(
      kTestUrl,
      base::BindRepeating(
          [](base::test::TestFuture<SessionAccess>* future,
             const SessionAccess& access) { future->SetValue(access); },
          &observer_future));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  SessionAccess access = observer_future.Take();
  EXPECT_EQ(access.access_type, SessionAccess::AccessType::kUpdate);
  EXPECT_EQ(access.session_key,
            SessionKey(SchemefulSite(GURL(kOrigin)), Session::Id(kSessionId)));
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_SigningQuotaExceeded) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  // Complete one refresh first, so that a refused prewarm would have a last
  // result to record in `SigningQuotaExceededLastResult2`.
  {
    auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
        kSessionId, kRefreshUrlString, kOrigin);
    base::test::TestFuture<SessionPrewarmResult> refresh_future;
    service().PrewarmSessionsForUrl(kTestUrl, refresh_future.GetCallback());
    EXPECT_THAT(refresh_future.Get().results,
                testing::ElementsAre(RefreshResult::kRefreshed));
  }

  for (size_t i = 0; i < 6; i++) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        SchemefulSite(GURL(kOrigin)), SigningQuotaOperation::kRefresh));
  }

  base::HistogramTester histogram_tester;
  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kSigningQuotaExceeded));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
  // A prewarm refused up front doesn't record any signing quota histogram.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Net.DeviceBoundSessions.SigningQuota"),
              IsEmpty());
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_SigningQuotaExceededWithFreshCookies) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());
  for (size_t i = 0; i < 6; i++) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        SchemefulSite(GURL(kOrigin)), SigningQuotaOperation::kRefresh));
  }

  // No refresh is needed yet, so the exhausted quota doesn't matter.
  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kInScopeRefreshNotYetNeeded));
  EXPECT_FALSE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_SigningQuotaExceededJoinsRefreshInFlight) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future1;
  service().PrewarmSessionsForUrl(kTestUrl, future1.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  // Exhaust the quota while the refresh is in flight.
  for (size_t i = 0; i < 6; i++) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        SchemefulSite(GURL(kOrigin)), SigningQuotaOperation::kRefresh));
  }

  // The second prewarm joins the refresh in flight instead of being refused.
  base::test::TestFuture<SessionPrewarmResult> future2;
  service().PrewarmSessionsForUrl(kTestUrl, future2.GetCallback());
  // Wait for the cookie store query for `future2` to finish processing.
  base::test::TestFuture<const CookieList&> cookie_future;
  context()->cookie_store()->GetAllCookiesAsync(cookie_future.GetCallback());
  ASSERT_TRUE(cookie_future.Wait());
  EXPECT_EQ(tracker.num_pending_refreshes(), 1u);
  EXPECT_FALSE(future2.IsReady());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());
  EXPECT_THAT(future2.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_SigningQuotaExceededJoinsDeferredRefresh) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request(request.get());
  base::test::TestFuture<RefreshResult> deferred_future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      deferred_future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  // Exhaust the quota while the deferred refresh is in flight.
  for (size_t i = 0; i < 6; ++i) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        SchemefulSite(GURL(kOrigin)), SigningQuotaOperation::kRefresh));
  }

  // The prewarm joins the deferred refresh instead of being refused.
  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  // Wait for the cookie store query for `future` to finish processing.
  base::test::TestFuture<const CookieList&> cookie_future;
  context()->cookie_store()->GetAllCookiesAsync(cookie_future.GetCallback());
  ASSERT_TRUE(cookie_future.Wait());
  EXPECT_EQ(tracker.num_pending_refreshes(), 1u);
  EXPECT_FALSE(future.IsReady());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));
  EXPECT_EQ(deferred_future.Get(), RefreshResult::kRefreshed);
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_SigningQuotaOneUnitLeftStartsRefresh) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId, kRefreshUrlString, kOrigin);

  // Use 5 of the 6 units, so exactly one refresh still fits.
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_TRUE(service().TryConsumeSigningQuota(
        SchemefulSite(GURL(kOrigin)), SigningQuotaOperation::kRefresh));
  }

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_SessionDeletedDuringRefresh) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  // Run tasks so OnGetCookiesForPrewarm runs and starts the refresh.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  // While the refresh is pending, delete the session.
  service().DeleteSessionAndNotify(
      DeletionReason::kClearBrowsingData,
      {SchemefulSite(kTestUrl), Session::Id(kSessionId)}, base::DoNothing());

  // The future should be resolved immediately with kFatalError when the session
  // is deleted.
  ASSERT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kFatalError));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_DifferingResults) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  // Add a second session that requires "missing_cookie".
  auto fetch_param2 = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL(kRefreshUrlString), {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  {
    auto scoped_test_fetcher2 =
        ScopedTestRegistrationFetcher(base::BindRepeating(
            [](RegistrationFetcher::RegistrationCompleteCallback callback) {
              std::move(callback).Run(
                  nullptr,
                  RegistrationResult(Session::CreateIfValid(SessionParams{
                      .session_id = std::string(kSessionId2),
                      .fetcher_url = GURL(kRefreshUrlString),
                      .refresh_url = std::string(kRefreshUrlString),
                      .scope = {.include_site = true,
                                .origin = std::string(kOrigin)},
                      .credentials =
                          {
                              {
                                  .name = "missing_cookie",
                                  .attributes = "secure",
                              },
                          },
                  })));
            }));
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param2),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(),
        /*original_request_initiator=*/std::nullopt);
  }

  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kSessionId2, kRefreshUrlString, kOrigin);

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(
      future.Get().results,
      testing::UnorderedElementsAre(RefreshResult::kInScopeRefreshNotYetNeeded,
                                    RefreshResult::kRefreshed));
  ASSERT_FALSE(future.Get().earliest_next_refresh_time.is_max());
  EXPECT_NEAR((future.Get().earliest_next_refresh_time - base::Time::Now())
                  .InSecondsF(),
              380.0, 2.0);
}

TEST_F(SessionServiceImplTest, PrewarmSessionsForUrl_OverlappingCalls) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future1;
  service().PrewarmSessionsForUrl(kTestUrl, future1.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  // Issue a second overlapping call for the same URL while the refresh is
  // pending.
  base::test::TestFuture<SessionPrewarmResult> future2;
  service().PrewarmSessionsForUrl(kTestUrl, future2.GetCallback());

  // Wait for the cookie store query for `future2` to finish processing.
  base::test::TestFuture<const CookieList&> cookie_future;
  context()->cookie_store()->GetAllCookiesAsync(cookie_future.GetCallback());
  ASSERT_TRUE(cookie_future.Wait());
  // No new refresh should be started.
  EXPECT_EQ(tracker.num_pending_refreshes(), 1u);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Complete the refresh.
  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());
  EXPECT_THAT(future1.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_THAT(future2.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_ProactiveRefreshReturnsEarliestNextRefreshTime) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  ASSERT_FALSE(future.Get().earliest_next_refresh_time.is_max());
  EXPECT_NEAR((future.Get().earliest_next_refresh_time - base::Time::Now())
                  .InSecondsF(),
              380.0, 2.0);
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_ProactiveRefreshWithShortCookieLifetime) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=60", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_EQ(future.Get().earliest_next_refresh_time, base::Time::Now());
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_ProactiveRefreshWithSessionCookie) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  CookieInclusionStatus status;
  auto cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure", base::Time::Now(), std::nullopt,
      std::nullopt, CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  base::test::TestFuture<CookieAccessResult> cookie_future;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future.Get().status.IsInclude());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(SessionServiceImplWithStoreTest,
       PrewarmSessionsForUrl_ProactiveRefreshSessionDeletedDuringCookieFetch) {
  SessionKey non_existent_key{SchemefulSite(kTestUrl), Session::Id("deleted")};

  base::test::TestFuture<RefreshResult, base::Time> future;
  CallOnGetCookiesAfterProactiveRefresh(non_existent_key,
                                        RefreshResult::kRefreshed, {}, {},
                                        future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<0>(), RefreshResult::kFatalError);
  EXPECT_TRUE(future.Get<1>().is_max());
}

TEST_F(SessionServiceImplTest,
       PrewarmSessionsForUrl_ProactiveRefreshCookieFetchFailsOrMissing) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  EXPECT_TRUE(future.Get().earliest_next_refresh_time.is_max());
}

TEST_F(
    SessionServiceImplTest,
    PrewarmSessionsForUrl_ProactiveRefreshMultipleCookiesDifferentLifetimes) {
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      GURL(kRefreshUrlString), {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  {
    auto scoped_test_fetcher =
        ScopedTestRegistrationFetcher(base::BindRepeating(
            [](RegistrationFetcher::RegistrationCompleteCallback callback) {
              std::move(callback).Run(
                  nullptr,
                  RegistrationResult(Session::CreateIfValid(SessionParams{
                      .session_id = std::string(kSessionId),
                      .fetcher_url = GURL(kRefreshUrlString),
                      .refresh_url = std::string(kRefreshUrlString),
                      .scope = {.include_site = true,
                                .origin = std::string(kOrigin)},
                      .credentials =
                          {
                              {
                                  .name = "cookie1",
                                  .attributes = "secure",
                              },
                              {
                                  .name = "cookie2",
                                  .attributes = "secure",
                              },
                          },
                  })));
            }));
    service().RegisterBoundSession(
        base::DoNothing(), std::move(fetch_param),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(),
        /*original_request_initiator=*/std::nullopt);
  }

  RefreshTracker tracker;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      &RefreshTracker::Refresh, base::Unretained(&tracker)));

  base::test::TestFuture<SessionPrewarmResult> future;
  service().PrewarmSessionsForUrl(kTestUrl, future.GetCallback());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return tracker.num_pending_refreshes() == 1u; }));

  CookieInclusionStatus status1;
  auto cookie1 = CanonicalCookie::Create(
      kTestUrl, "cookie1=v1; Secure; Max-Age=500", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status1);
  ASSERT_TRUE(cookie1);
  base::test::TestFuture<CookieAccessResult> cookie_future1;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie1), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future1.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future1.Get().status.IsInclude());

  CookieInclusionStatus status2;
  auto cookie2 = CanonicalCookie::Create(
      kTestUrl, "cookie2=v2; Secure; Max-Age=300", base::Time::Now(),
      std::nullopt, std::nullopt, CookieSourceType::kHTTP, &status2);
  ASSERT_TRUE(cookie2);
  base::test::TestFuture<CookieAccessResult> cookie_future2;
  context()->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie2), kTestUrl, CookieOptions::MakeAllInclusive(),
      cookie_future2.GetCallback(), std::nullopt);
  ASSERT_TRUE(cookie_future2.Get().status.IsInclude());

  tracker.ResolvePendingRefresh(
      RegistrationResult(RegistrationResult::NoSessionConfigChange(),
                         CookieAndLineAccessResultList()));

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get().results,
              testing::ElementsAre(RefreshResult::kRefreshed));
  ASSERT_FALSE(future.Get().earliest_next_refresh_time.is_max());
  EXPECT_NEAR((future.Get().earliest_next_refresh_time - base::Time::Now())
                  .InSecondsF(),
              180.0, 2.0);
}

class SessionServiceImplPreProvisionedKeyTest : public SessionServiceImplTest {
 public:
  SessionServiceImplPreProvisionedKeyTest() {
    AddScopedFeatureList().InitAndEnableFeature(
        net::features::kDeviceBoundSessionsForSingleSignOn);
  }

  void SetUp() override {
    SessionServiceImplTest::SetUp();
    SetService(std::make_unique<SessionServiceImpl>(
        *key_service(), context(),
        /*store=*/nullptr,
        /*restricted_sites=*/std::vector<SchemefulSite>(),
        /*has_cookie_access_cb=*/
        base::BindLambdaForTesting([&](const CookieAccessCheckParams& params) {
          return allow_cookie_access_;
        }),
        /*client_cert_handler=*/base::DoNothing()));
  }

  void SetCookieAccess(bool allow) { allow_cookie_access_ = allow; }

  bool AddPreProvisionedKey(
      const url::Origin& rp_origin,
      std::string_view provider_key,
      const url::Origin& provider_origin,
      unexportable_keys::UnexportableSigningKeyId key_id) {
    return service().sso_key_manager_.AddPreProvisionedKey(
        rp_origin, provider_key, provider_origin, key_id);
  }

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(
      std::string_view provider_key,
      base::optional_ref<const url::Origin> original_request_initiator) {
    return service().sso_key_manager_.FindPreProvisionedKey(
        provider_key, original_request_initiator);
  }

 private:
  bool allow_cookie_access_ = true;
};

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       DeleteAllSessionsClearsPreProvisionedKeys) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  auto provider_origin = url::Origin::Create(GURL("https://provider.test"));

  constexpr size_t kNumKeysToGenerate = 2;

  std::vector<unexportable_keys::UnexportableSigningKeyId> key_ids;
  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    base::test::TestFuture<unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableSigningKeyId>>
        key_future;
    key_service()->GenerateSigningKeySlowlyAsync(
        {crypto::sign::ECDSA_SHA256},
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        key_future.GetCallback());
    unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();
    key_ids.push_back(key_id);

    std::string provider_key = base::NumberToString(i);
    // Keys should be added successfully.
    EXPECT_TRUE(
        AddPreProvisionedKey(rp_origin, provider_key, provider_origin, key_id));
    EXPECT_TRUE(key_service()->GetWrappedKey(key_id).has_value());
  }

  // Delete potential zombie pre-provisioned keys for example.test.
  base::RunLoop run_loop;
  service().DeleteAllSessions(DeletionReason::kClearBrowsingData,
                              /*created_after_time=*/std::nullopt,
                              /*created_before_time=*/std::nullopt,
                              /*origin_and_site_matcher=*/base::NullCallback(),
                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify that all cleared pre-provisioned keys were deleted from the key
  // service.
  EXPECT_TRUE(std::ranges::none_of(
      key_ids, [&](unexportable_keys::UnexportableSigningKeyId key_id) {
        return key_service()->GetWrappedKey(key_id).has_value();
      }));

  // Re-adding an earlier `provider_key` succeeds once the keys were purged.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_TRUE(AddPreProvisionedKey(rp_origin, "0", provider_origin, key_id));
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       DeleteAllSessionsUnmatchedDoesNotDelete) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  auto provider_origin = url::Origin::Create(GURL("https://provider.test"));

  // Add a bound session to example.test.
  AddSessionsForTesting(
      {{kSessionId, "https://example.test/refresh", "https://example.test"}});

  constexpr size_t kNumKeysToGenerate = 2;

  std::vector<unexportable_keys::UnexportableSigningKeyId> key_ids;
  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    base::test::TestFuture<unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableSigningKeyId>>
        key_future;
    key_service()->GenerateSigningKeySlowlyAsync(
        {crypto::sign::ECDSA_SHA256},
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        key_future.GetCallback());
    unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();
    key_ids.push_back(key_id);

    std::string provider_key = base::NumberToString(i);
    // Keys should be added successfully.
    EXPECT_TRUE(
        AddPreProvisionedKey(rp_origin, provider_key, provider_origin, key_id));
    EXPECT_TRUE(key_service()->GetWrappedKey(key_id).has_value());
  }

  // Attempt to delete pre-provisioned keys but provide a matcher that does not
  // match the current site (example.test).
  base::RunLoop run_loop;
  auto origin_and_site_matcher = base::BindRepeating(
      [](const url::Origin& origin, const net::SchemefulSite& site) {
        return site.Serialize() == "https://unmatched.test";
      });

  service().DeleteAllSessions(DeletionReason::kClearBrowsingData,
                              /*created_after_time=*/std::nullopt,
                              /*created_before_time=*/std::nullopt,
                              origin_and_site_matcher, run_loop.QuitClosure());
  run_loop.Run();

  // Verify that none of the pre-provisioned keys were deleted from the key
  // service.
  for (const auto& key_id : key_ids) {
    EXPECT_TRUE(key_service()->GetWrappedKey(key_id).has_value());
  }

  // Since the keys were not purged, re-adding `"0"` fails as a duplicate.
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_FALSE(AddPreProvisionedKey(rp_origin, "0", provider_origin, key_id));
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnSuccess) {
  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  auto rp_url = GURL(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);
  auto provider_origin = url::Origin::Create(GURL(kIdentityProviderOrigin));

  // Add a pre-provisioned key (in a real scenario this would be done via
  // Secure-Session-GenerateKey header).
  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  FakeDeviceBoundSessionObserver observer;
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher(base::BindRepeating(
      [](unexportable_keys::UnexportableSigningKeyId key_id,
         RegistrationFetcher::RegistrationCompleteCallback callback) {
        std::move(callback).Run(
            /*fetcher=*/nullptr, RegistrationResult(CreateRelyingPartySession(
                                     kRelyingPartySessionId, key_id)));
      },
      key_id));

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      observer.GetCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), rp_origin);

  // Validate the relying session exists and has the expected key ID.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});
  ASSERT_NE(relying_session, nullptr);
  EXPECT_EQ(relying_session->id().value(), kRelyingPartySessionId);
  EXPECT_EQ(relying_session->unexportable_key_id(), key_id);

  // Validate that access observer received creation notification.
  ASSERT_EQ(observer.notifications().size(), 1u);
  EXPECT_EQ(observer.notifications()[0].access_type,
            SessionAccess::AccessType::kCreation);
  EXPECT_EQ(observer.notifications()[0].session_key.id.value(),
            kRelyingPartySessionId);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kSuccess, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
                  Pair("Net.DeviceBoundSessions.RegistrationDuration."
                       "SingleSignOn.Success",
                       1)));

  // Validate that the pre-provisioned key was consumed upon session creation.
  EXPECT_THAT(FindPreProvisionedKey(kProviderKey, rp_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnKeyClearedWhileInFlight) {
  auto provider_origin = url::Origin::Create(GURL(kIdentityProviderOrigin));
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  FakeDeviceBoundSessionObserver observer;
  base::MockCallback<SessionService::OnEventCallback> event_callback;
  base::CallbackListSubscription event_subscription =
      service().AddEventObserver(event_callback.Get());
  EXPECT_CALL(event_callback, Run(_))
      .WillOnce([&rp_url](const SessionEvent& event) {
        EXPECT_EQ(event.site, SchemefulSite(rp_url));
        EXPECT_EQ(event.session_id, std::nullopt);
        EXPECT_FALSE(event.succeeded);
        ASSERT_TRUE(std::holds_alternative<CreationEventDetails>(
            event.event_type_details));
        const auto& details =
            std::get<CreationEventDetails>(event.event_type_details);
        EXPECT_EQ(details.fetch_error,
                  SessionError::kPreProvisionedKeyNotFound);
        EXPECT_EQ(details.new_session_display, std::nullopt);
      });
  base::HistogramTester histograms;
  RegistrationFetcher::RegistrationCompleteCallback held_callback;
  auto scoped_test_fetcher =
      ScopedTestRegistrationFetcher(base::BindLambdaForTesting(
          [&](RegistrationFetcher::RegistrationCompleteCallback callback) {
            held_callback = std::move(callback);
          }));

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      observer.GetCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), rp_origin);
  ASSERT_TRUE(held_callback);

  // Clear the RP's site data while the registration fetch is in flight.
  base::RunLoop clear_loop;
  service().DeleteAllSessions(
      DeletionReason::kClearBrowsingData, /*created_after_time=*/std::nullopt,
      /*created_before_time=*/std::nullopt,
      base::BindRepeating(
          [](const url::Origin& target_origin, const url::Origin& origin,
             const net::SchemefulSite& site) {
            return origin == target_origin;
          },
          rp_origin),
      clear_loop.QuitClosure());
  clear_loop.Run();
  EXPECT_THAT(
      key_service()->GetWrappedKey(key_id),
      base::test::ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));

  std::move(held_callback)
      .Run(/*fetcher=*/nullptr, RegistrationResult(CreateRelyingPartySession(
                                    kRelyingPartySessionId, key_id)));

  EXPECT_EQ(service().GetSession(
                {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)}),
            nullptr);
  EXPECT_TRUE(observer.notifications().empty());

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kPreProvisionedKeyNotFound, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kPreProvisionedKeyNotFound, 1);
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnConcurrentRegistrationsWithSameKey) {
  auto provider_origin = url::Origin::Create(GURL(kIdentityProviderOrigin));
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  base::HistogramTester histograms;
  std::vector<RegistrationFetcher::RegistrationCompleteCallback> held_callbacks;
  auto scoped_test_fetcher =
      ScopedTestRegistrationFetcher(base::BindLambdaForTesting(
          [&](RegistrationFetcher::RegistrationCompleteCallback callback) {
            held_callbacks.push_back(std::move(callback));
          }));

  for (int i = 0; i < 2; ++i) {
    auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
        rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
        /*authorization=*/std::nullopt,
        ProviderRegistrationParams{.provider_key{kProviderKey}},
        AttestationMode::kNone, rp_origin);
    service().RegisterBoundSession(
        SessionService::OnAccessCallback(), std::move(fetch_param),
        IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
        SiteForCookies(), NetLogWithSource(), rp_origin);
  }
  ASSERT_EQ(held_callbacks.size(), 2u);

  std::move(held_callbacks[0])
      .Run(/*fetcher=*/nullptr,
           RegistrationResult(CreateRelyingPartySession("session_1", key_id)));
  std::move(held_callbacks[1])
      .Run(/*fetcher=*/nullptr,
           RegistrationResult(CreateRelyingPartySession("session_2", key_id)));

  EXPECT_NE(
      service().GetSession({SchemefulSite(rp_url), Session::Id("session_1")}),
      nullptr);
  EXPECT_EQ(
      service().GetSession({SchemefulSite(rp_url), Session::Id("session_2")}),
      nullptr);
  EXPECT_TRUE(key_service()->GetWrappedKey(key_id).has_value());

  histograms.ExpectTotalCount("Net.DeviceBoundSessions.RegistrationResult", 2);
  histograms.ExpectBucketCount("Net.DeviceBoundSessions.RegistrationResult",
                               SessionError::kSuccess, 1);
  histograms.ExpectBucketCount("Net.DeviceBoundSessions.RegistrationResult",
                               SessionError::kPreProvisionedKeyNotFound, 1);
  histograms.ExpectTotalCount(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn", 2);
  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kPreProvisionedKeyNotFound, 1);
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnKeyNotFound) {
  FakeDeviceBoundSessionObserver observer;
  base::HistogramTester histograms;
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);

  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kRelyingPartySessionId, base::StrCat({kRelyingPartyOrigin, "/refresh"}),
      kRelyingPartyOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      observer.GetCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), url::Origin::Create(rp_url));

  // Validate the session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});
  EXPECT_EQ(relying_session, nullptr);
  EXPECT_TRUE(observer.notifications().empty());

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kPreProvisionedKeyNotFound, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kPreProvisionedKeyNotFound, 1);
  EXPECT_THAT(
      histograms.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.RegistrationDuration"),
      ElementsAre(Pair("Net.DeviceBoundSessions.RegistrationDuration", 1),
                  Pair("Net.DeviceBoundSessions.RegistrationDuration."
                       "SingleSignOn.Failure",
                       1)));
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnMissingInitiator) {
  auto provider_origin = url::Origin::Create(GURL(kIdentityProviderOrigin));
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kRelyingPartySessionId, base::StrCat({kRelyingPartyOrigin, "/refresh"}),
      kRelyingPartyOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  // Validate the session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kInvalidPreProvisionedKeyInitiatorMissing, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kInvalidPreProvisionedKeyInitiatorMissing, 1);
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnNoCookieAccess) {
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);
  auto provider_origin = url::Origin::Create(GURL("https://provider.test"));

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  // Add the key while cookie access is allowed.
  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  // Disallow cookie access.
  SetCookieAccess(false);

  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kRelyingPartySessionId, base::StrCat({kRelyingPartyOrigin, "/refresh"}),
      kRelyingPartyOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), rp_origin);

  // Validate the session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult",
      SessionError::kPreProvisionedKeyAccessNotGranted, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kPreProvisionedKeyAccessNotGranted, 1);
}

TEST_F(SessionServiceImplPreProvisionedKeyTest,
       RegisterBoundSessionSingleSignOnFetcherError) {
  GURL rp_url(kRelyingPartyOrigin);
  auto rp_origin = url::Origin::Create(rp_url);
  auto provider_origin = url::Origin::Create(GURL(kIdentityProviderOrigin));

  base::test::TestFuture<unexportable_keys::ServiceErrorOr<
      unexportable_keys::UnexportableSigningKeyId>>
      key_future;
  key_service()->GenerateSigningKeySlowlyAsync(
      {crypto::sign::ECDSA_SHA256},
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      key_future.GetCallback());
  unexportable_keys::UnexportableSigningKeyId key_id = *key_future.Take();

  EXPECT_TRUE(
      AddPreProvisionedKey(rp_origin, kProviderKey, provider_origin, key_id));

  base::HistogramTester histograms;
  auto scoped_fetcher = ScopedTestRegistrationFetcher::CreateWithFailure(
      SessionError::kNetError, base::StrCat({kRelyingPartyOrigin, "/refresh"}));
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey}},
      AttestationMode::kNone, rp_origin);

  service().RegisterBoundSession(
      SessionService::OnAccessCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), rp_origin);

  // Validate the session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});
  EXPECT_EQ(relying_session, nullptr);

  histograms.ExpectUniqueSample("Net.DeviceBoundSessions.RegistrationResult",
                                SessionError::kNetError, 1);
  histograms.ExpectUniqueSample(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn",
      SessionError::kNetError, 1);

  // A failed fetch keeps the key, so the RP can retry the registration.
  EXPECT_THAT(FindPreProvisionedKey(kProviderKey, rp_origin),
              base::test::ValueIs(key_id));
  EXPECT_OK(key_service()->GetWrappedKey(key_id));
}

class SessionServiceImplTestWithoutSingleSignOn
    : public SessionServiceImplTest {
 public:
  SessionServiceImplTestWithoutSingleSignOn() {
    AddScopedFeatureList().InitAndDisableFeature(
        net::features::kDeviceBoundSessionsForSingleSignOn);
  }
};

TEST_F(SessionServiceImplTestWithoutSingleSignOn,
       IgnoresRegistrationIfSingleSignOnDisabledAndProviderKeyPresent) {
  GURL rp_url(kRelyingPartyOrigin);
  GURL provider_url(kIdentityProviderOrigin);
  FakeDeviceBoundSessionObserver observer;
  base::HistogramTester histograms;
  auto scoped_test_fetcher = ScopedTestRegistrationFetcher::CreateWithSuccess(
      kRelyingPartySessionId, base::StrCat({kRelyingPartyOrigin, "/refresh"}),
      kRelyingPartyOrigin);
  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      rp_url.Resolve("/register"), {crypto::sign::ECDSA_SHA256}, kChallenge,
      /*authorization=*/std::nullopt,
      ProviderRegistrationParams{.provider_key{kProviderKey},
                                 .provider_url{provider_url}},
      AttestationMode::kNone, url::Origin::Create(provider_url));

  service().RegisterBoundSession(
      observer.GetCallback(), std::move(fetch_param),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt), SiteForCookies(),
      NetLogWithSource(), url::Origin::Create(rp_url));

  // Validate the session does not exist.
  Session* relying_session = service().GetSession(
      {SchemefulSite(rp_url), Session::Id(kRelyingPartySessionId)});

  EXPECT_EQ(relying_session, nullptr);
  EXPECT_TRUE(observer.notifications().empty());

  histograms.ExpectTotalCount("Net.DeviceBoundSessions.RegistrationResult", 0);
  histograms.ExpectTotalCount(
      "Net.DeviceBoundSessions.RegistrationResult.Standalone", 0);
  histograms.ExpectTotalCount(
      "Net.DeviceBoundSessions.RegistrationResult.SingleSignOn", 0);
}

TEST_F(SessionServiceImplTest,
       DeferredRequestRefreshTimeout_UnblocksSingleRequestAndPreservesSession) {
  URLRequestFailedJob::AddUrlHandlerForHostname("example.com");

  GURL hanging_refresh_url = URLRequestFailedJob::GetMockHttpsUrlForHostname(
      ERR_IO_PENDING, "example.com");
  AddSessionsForTesting({{kSessionId, hanging_refresh_url.spec(), kOrigin}});

  const SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context()->CreateRequest(kTestUrl, IDLE, &delegate, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));

  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  ASSERT_TRUE(maybe_deferral);

  base::test::TestFuture<RefreshResult> future;
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), RefreshResult::kUnreachable);

  // Assert that the session credentials are still preserved.
  const Session* session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(*session->id(), kSessionId);
}

TEST_F(
    SessionServiceImplTest,
    DeferredRequestRefreshTimeout_UnblocksMultipleRequestsAndPreservesSession) {
  URLRequestFailedJob::AddUrlHandlerForHostname("example.com");

  GURL hanging_refresh_url = URLRequestFailedJob::GetMockHttpsUrlForHostname(
      ERR_IO_PENDING, "example.com");
  AddSessionsForTesting({{kSessionId, hanging_refresh_url.spec(), kOrigin}});

  const SchemefulSite site(kTestUrl);
  ASSERT_TRUE(service().GetSession({site, Session::Id(kSessionId)}));

  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate1, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request1->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request1(request1.get());

  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate2, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request2->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request2(request2.get());

  net::TestDelegate delegate3;
  std::unique_ptr<URLRequest> request3 =
      context()->CreateRequest(kTestUrl, IDLE, &delegate3, kDummyAnnotation,
                               net::handles::kInvalidNetworkHandle);
  request3->set_site_for_cookies(SiteForCookies::FromUrl(kTestUrl));
  DbscRequest dbsc_request3(request3.get());

  base::test::TestFuture<RefreshResult> future1;
  base::test::TestFuture<RefreshResult> future2;
  base::test::TestFuture<RefreshResult> future3;

  service().DeferRequestForRefresh(
      dbsc_request1, SessionService::DeferralParams(Session::Id(kSessionId)),
      future1.GetCallback());
  service().DeferRequestForRefresh(
      dbsc_request2, SessionService::DeferralParams(Session::Id(kSessionId)),
      future2.GetCallback());
  service().DeferRequestForRefresh(
      dbsc_request3, SessionService::DeferralParams(Session::Id(kSessionId)),
      future3.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_FALSE(future3.IsReady());

  FastForwardBy(base::Seconds(20));

  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future2.IsReady());
  EXPECT_TRUE(future3.IsReady());
  EXPECT_EQ(future1.Take(), RefreshResult::kUnreachable);
  EXPECT_EQ(future2.Take(), RefreshResult::kUnreachable);
  EXPECT_EQ(future3.Take(), RefreshResult::kUnreachable);

  // Assert that the session credentials are still preserved.
  const Session* session =
      service().GetSession({site, Session::Id(kSessionId)});
  ASSERT_TRUE(session);
  EXPECT_EQ(*session->id(), kSessionId);
}

TEST_F(SessionServiceImplTest,
       DeferRequestPassesSiteForCookiesAndIsolationInfoToFetcher) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://other.test"));
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      CreateRequestWithContext(context(), kTestUrl, expected_origin, &delegate);

  base::test::TestFuture<CapturedRequestContext> intercept_future;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRefreshUrl, std::make_unique<RequestContextCapturingInterceptor>(
                           intercept_future.GetRepeatingCallback()));

  base::test::TestFuture<RefreshResult> refresh_future;
  DbscRequest dbsc_request(request.get());
  service().DeferRequestForRefresh(
      dbsc_request, SessionService::DeferralParams(Session::Id(kSessionId)),
      refresh_future.GetCallback());

  ExpectContext(intercept_future.Take(), request->site_for_cookies(),
                request->isolation_info(), expected_origin);
  EXPECT_EQ(refresh_future.Take(), RefreshResult::kRefreshed);
}

TEST_F(SessionServiceImplTest,
       ProactiveRefreshPassesSiteForCookiesAndIsolationInfoToFetcher) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://sub.example.com"));

  // Create a same-site request with an explicit isolation context.
  net::TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      CreateRequestWithContext(context(), kTestUrl, expected_origin, &delegate);

  // Attach the required cookie, but make it expire very soon.
  CookieInclusionStatus status;
  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      kTestUrl, "test_cookie=v; Secure; Max-Age=1", base::Time::Now(),
      /*server_time=*/std::nullopt, /*cookie_partition_key=*/std::nullopt,
      CookieSourceType::kHTTP, &status);
  ASSERT_TRUE(cookie);
  request->set_maybe_sent_cookies({{*cookie, CookieAccessResult()}});

  base::test::TestFuture<CapturedRequestContext> intercept_future;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRefreshUrl, std::make_unique<RequestContextCapturingInterceptor>(
                           intercept_future.GetRepeatingCallback()));

  // The request shouldn't be deferred, but it should trigger a proactive
  // refresh.
  HttpRequestHeaders extra_headers;
  DbscRequest dbsc_request(request.get());
  std::optional<SessionService::DeferralParams> maybe_deferral =
      service().ShouldDefer(dbsc_request, &extra_headers);
  EXPECT_FALSE(maybe_deferral.has_value());

  ExpectContext(intercept_future.Take(), request->site_for_cookies(),
                request->isolation_info(), expected_origin);
}

TEST_F(SessionServiceImplTest,
       PrewarmPassesSiteForCookiesAndIsolationInfoToFetcher) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  base::test::TestFuture<CapturedRequestContext> intercept_future;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRefreshUrl, std::make_unique<RequestContextCapturingInterceptor>(
                           intercept_future.GetRepeatingCallback()));

  base::test::TestFuture<SessionPrewarmResult> prewarm_future;
  service().PrewarmSessionsForUrl(kTestUrl, prewarm_future.GetCallback());

  const url::Origin expected_origin = url::Origin::Create(kTestUrl);
  ExpectContext(intercept_future.Take(),
                SiteForCookies::FromOrigin(expected_origin),
                IsolationInfo::CreateForInternalRequest(expected_origin),
                expected_origin);
  EXPECT_THAT(prewarm_future.Take().results,
              ElementsAre(RefreshResult::kRefreshed));
}

TEST_F(SessionServiceImplTest,
       RegisterBoundSessionPassesSiteForCookiesAndIsolationInfoToFetcher) {
  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://other.test"));
  const SiteForCookies expected_site_for_cookies =
      SiteForCookies::FromOrigin(expected_origin);
  const IsolationInfo expected_isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, expected_origin, expected_origin,
      expected_site_for_cookies);

  base::test::TestFuture<CapturedRequestContext> intercept_future;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRegisterUrl, std::make_unique<RequestContextCapturingInterceptor>(
                            intercept_future.GetRepeatingCallback()));

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      kTestRegisterUrl, {crypto::sign::ECDSA_SHA256}, "challenge",
      /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::DoNothing(), std::move(fetch_param), expected_isolation_info,
      expected_site_for_cookies, NetLogWithSource(), expected_origin);

  ExpectContext(intercept_future.Take(), expected_site_for_cookies,
                expected_isolation_info, expected_origin);
}

TEST_F(SessionServiceImplTest,
       WaiterDoesNotTriggerFetchAndLaterRefreshUsesNewTriggerContext) {
  AddSessionsForTesting({{kSessionId, kRefreshUrlString, kOrigin}});

  base::test::TestFuture<CapturedRequestContext> intercept_future(
      base::test::TestFutureMode::kQueue);
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      kTestRefreshUrl, std::make_unique<RequestContextCapturingInterceptor>(
                           intercept_future.GetRepeatingCallback()));

  const url::Origin origin1 = url::Origin::Create(kTestUrl);
  net::TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 =
      CreateRequestWithContext(context(), kTestUrl, origin1, &delegate1);
  DbscRequest dbsc_request1(request1.get());

  const url::Origin origin2 = url::Origin::Create(GURL("https://other.test"));
  net::TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 =
      CreateRequestWithContext(context(), kTestUrl, origin2, &delegate2);
  DbscRequest dbsc_request2(request2.get());

  ASSERT_FALSE(
      request1->site_for_cookies().IsEquivalent(request2->site_for_cookies()));
  ASSERT_FALSE(
      request1->isolation_info().IsEqualForTesting(request2->isolation_info()));

  const auto deferral = SessionService::DeferralParams(Session::Id(kSessionId));
  base::test::TestFuture<RefreshResult> future1;
  base::test::TestFuture<RefreshResult> future2;

  // Defer request1 (triggers the first refresh) and request2 (joins as waiter).
  service().DeferRequestForRefresh(dbsc_request1, deferral,
                                   future1.GetCallback());
  service().DeferRequestForRefresh(dbsc_request2, deferral,
                                   future2.GetCallback());

  ExpectContext(intercept_future.Take(), request1->site_for_cookies(),
                request1->isolation_info(), origin1);
  EXPECT_FALSE(intercept_future.IsReady());

  // Both requests are unblocked when refresh completes.
  EXPECT_EQ(future1.Take(), RefreshResult::kRefreshed);
  EXPECT_EQ(future2.Take(), RefreshResult::kRefreshedAsWaiter);

  // When the waiter (request2) subsequently triggers a refresh, its own
  // isolation context and SiteForCookies must be used.
  base::test::TestFuture<RefreshResult> future2_retry;
  service().DeferRequestForRefresh(dbsc_request2, deferral,
                                   future2_retry.GetCallback());

  ExpectContext(intercept_future.Take(), request2->site_for_cookies(),
                request2->isolation_info(), origin2);
  EXPECT_FALSE(intercept_future.IsReady());

  EXPECT_EQ(future2_retry.Take(), RefreshResult::kRefreshed);
}
}  // namespace net::device_bound_sessions
