// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_manager.h"

#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/session_service_impl.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

using net::device_bound_sessions::RegistrationFetcher;
using net::device_bound_sessions::RegistrationFetcherParam;
using net::device_bound_sessions::ScopedTestRegistrationFetcher;
using net::device_bound_sessions::Session;
using net::device_bound_sessions::SessionAccess;
using net::device_bound_sessions::SessionKey;
using net::device_bound_sessions::SessionParams;
using net::device_bound_sessions::SessionServiceImpl;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

class FakeDeviceBoundSessionObserver
    : public mojom::DeviceBoundSessionAccessObserver {
 public:
  const std::vector<SessionAccess>& notifications() const {
    return notifications_;
  }

  mojo::PendingRemote<mojom::DeviceBoundSessionAccessObserver>
  GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForNotification() {
    base::RunLoop run_loop;
    on_access_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // public mojom::DeviceBoundSessionAccessObserver
  void OnDeviceBoundSessionAccessed(const SessionAccess& access) override {
    notifications_.push_back(access);

    if (on_access_callback_) {
      std::move(on_access_callback_).Run();
    }
  }

  void Clone(
      mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
          observer) override {
    NOTREACHED();
  }

 private:
  mojo::Receiver<mojom::DeviceBoundSessionAccessObserver> receiver_{this};
  std::vector<SessionAccess> notifications_;
  base::OnceClosure on_access_callback_;
};

class DeviceBoundSessionManagerTest : public ::testing::Test {
 public:
  DeviceBoundSessionManagerTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()),
        service_(std::make_unique<SessionServiceImpl>(unexportable_key_service_,
                                                      context_.get(),
                                                      /*store=*/nullptr)),
        cookie_manager_(std::make_unique<CookieManager>(
            context_.get(),
            nullptr,
            base::MakeRefCounted<SessionCleanupCookieStore>(
                base::MakeRefCounted<net::SQLitePersistentCookieStore>(
                    base::FilePath(),
                    base::SingleThreadTaskRunner::GetCurrentDefault(),
                    base::SingleThreadTaskRunner::GetCurrentDefault(),
                    false,
                    nullptr,
                    false)),
            nullptr,
            nullptr)),
        manager_(DeviceBoundSessionManager::Create(service_.get(),
                                                   cookie_manager_.get())) {}

  DeviceBoundSessionManager& manager() { return *manager_; }
  CookieManager& cookie_manager() { return *cookie_manager_; }
  SessionServiceImpl& service() { return *service_; }

  std::vector<uint8_t> GetWrappedKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        generate_key_future;
    auto supported_algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        supported_algorithm,
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        generate_key_future.GetCallback());
    return *unexportable_key_service_.GetWrappedKey(*generate_key_future.Get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  std::unique_ptr<net::URLRequestContext> context_;
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_, crypto::UnexportableKeyProvider::Config()};
  std::unique_ptr<SessionServiceImpl> service_;
  std::unique_ptr<CookieManager> cookie_manager_;
  std::unique_ptr<DeviceBoundSessionManager> manager_;
};

MATCHER(IsInclude, "") {
  return arg.IsInclude();
}

TEST_F(DeviceBoundSessionManagerTest, ObserverNotifiesChangeOnlyOnSite) {
  ScopedTestRegistrationFetcher scoped_fetcher =
      ScopedTestRegistrationFetcher::CreateWithSuccess(
          "SessionId", "https://example.com/refresh", "https://example.com");

  GURL url("https://example.com");
  net::SchemefulSite site(url);

  FakeDeviceBoundSessionObserver observer, off_site_observer;
  manager().AddObserver(url, observer.GetPendingRemote());
  manager().AddObserver(GURL("https://not-example.com"),
                        off_site_observer.GetPendingRemote());

  auto fetch_param = RegistrationFetcherParam::CreateInstanceForTesting(
      url, {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
      "challenge", /*authorization=*/std::nullopt);
  service().RegisterBoundSession(
      base::NullCallback(), std::move(fetch_param),
      net::IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      net::NetLogWithSource(), /*original_request_initiator=*/std::nullopt);

  observer.WaitForNotification();

  EXPECT_THAT(
      observer.notifications(),
      ElementsAre(SessionAccess{SessionAccess::AccessType::kCreation,
                                SessionKey(site, Session::Id("SessionId"))}));

  EXPECT_THAT(off_site_observer.notifications(), IsEmpty());
}

TEST_F(DeviceBoundSessionManagerTest, CreateBoundSessions) {
  GURL url("https://example.com/path");
  std::string session_id = "session123";

  std::vector<SessionParams::Scope::Specification> specifications;
  specifications.emplace_back(
      SessionParams::Scope::Specification::Type::kInclude, "sub.example.com",
      "/path");
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.specifications = std::move(specifications);
  scope.origin = url::Origin::Create(url).Serialize();

  SessionParams params(
      session_id, url, "https://example.com/refresh", std::move(scope),
      {SessionParams::Credential{"test_cookie", "SameSite=Strict"}},
      unexportable_keys::UnexportableKeyId(), {"example.com"});

  net::CookieInclusionStatus status;
  auto cookie = net::CanonicalCookie::Create(
      url, "test_cookie=value", base::Time::Now(), std::nullopt,
      std::nullopt /* cookie_partition_key */, net::CookieSourceType::kHTTP,
      &status);
  ASSERT_TRUE(cookie);
  std::vector<net::CanonicalCookie> cookies_to_set;
  cookies_to_set.push_back(*cookie);

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  std::vector<SessionParams> params_list;
  params_list.push_back(std::move(params));

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  manager().CreateBoundSessions(std::move(params_list), GetWrappedKey(),
                                cookies_to_set, cookie_options,
                                create_future.GetCallback());
  EXPECT_THAT(
      create_future.Get<0>(),
      ElementsAre(
          net::device_bound_sessions::SessionError::ErrorType::kSuccess));
  EXPECT_THAT(create_future.Get<1>(), ElementsAre(IsInclude()));

  base::test::TestFuture<const std::vector<SessionKey>&> sessions_future;
  service().GetAllSessionsAsync(sessions_future.GetCallback());
  const std::vector<SessionKey>& sessions = sessions_future.Get();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].site, net::SchemefulSite(url));
  EXPECT_EQ(sessions[0].id.value(), session_id);

  base::test::TestFuture<const net::CookieAccessResultList&,
                         const net::CookieAccessResultList&>
      cookies_future;
  cookie_manager().GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                                 net::CookiePartitionKeyCollection(),
                                 cookies_future.GetCallback());
  const auto& cookies = cookies_future.Get<0>();
  ASSERT_EQ(cookies.size(), 1u);
  EXPECT_EQ(cookies[0].cookie.Name(), "test_cookie");
  EXPECT_EQ(cookies[0].cookie.Value(), "value");
}

TEST_F(DeviceBoundSessionManagerTest,
       CreateBoundSessions_InvalidSessionParams) {
  // `include_site` on a subdomain is forbidden
  GURL url("https://subdomain.example.com/path");
  std::string session_id = "session123";

  std::vector<SessionParams::Scope::Specification> specifications;
  specifications.emplace_back(
      SessionParams::Scope::Specification::Type::kInclude, "sub.example.com",
      "/path");
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.specifications = std::move(specifications);
  scope.origin = url::Origin::Create(url).Serialize();

  SessionParams params(
      session_id, url, "https://example.com/refresh", std::move(scope),
      {SessionParams::Credential{"test_cookie", "SameSite=Strict"}},
      unexportable_keys::UnexportableKeyId(), {"example.com"});

  net::CookieInclusionStatus status;
  auto cookie = net::CanonicalCookie::Create(
      url, "test_cookie=value", base::Time::Now(), std::nullopt,
      std::nullopt /* cookie_partition_key */, net::CookieSourceType::kHTTP,
      &status);
  ASSERT_TRUE(cookie);
  std::vector<net::CanonicalCookie> cookies_to_set;
  cookies_to_set.push_back(*cookie);

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  std::vector<SessionParams> params_list;
  params_list.push_back(std::move(params));

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  manager().CreateBoundSessions(std::move(params_list), GetWrappedKey(),
                                cookies_to_set, cookie_options,
                                create_future.GetCallback());
  EXPECT_THAT(create_future.Get<0>(),
              ElementsAre(net::device_bound_sessions::SessionError::ErrorType::
                              kInvalidScopeIncludeSite));
  EXPECT_THAT(create_future.Get<1>(), ElementsAre(IsInclude()));

  base::test::TestFuture<const net::CookieAccessResultList&,
                         const net::CookieAccessResultList&>
      cookies_future;
  cookie_manager().GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                                 net::CookiePartitionKeyCollection(),
                                 cookies_future.GetCallback());
  const auto& cookies = cookies_future.Get<0>();
  ASSERT_EQ(cookies.size(), 1u);
  EXPECT_EQ(cookies[0].cookie.Name(), "test_cookie");
  EXPECT_EQ(cookies[0].cookie.Value(), "value");
}

TEST_F(DeviceBoundSessionManagerTest, CreateBoundSessions_InvalidCookie) {
  GURL url("https://example.com/path");
  std::string session_id = "session123";

  std::vector<SessionParams::Scope::Specification> specifications;
  specifications.emplace_back(
      SessionParams::Scope::Specification::Type::kInclude, "sub.example.com",
      "/path");
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.specifications = std::move(specifications);
  scope.origin = url::Origin::Create(url).Serialize();

  SessionParams params(
      session_id, url, "https://example.com/refresh", std::move(scope),
      {SessionParams::Credential{"test_cookie", "SameSite=Strict"}},
      unexportable_keys::UnexportableKeyId(), {"example.com"});

  // This cookie is HttpOnly and our CookieOptions will forbid setting that.
  net::CookieInclusionStatus status;
  auto cookie = net::CanonicalCookie::CreateForTesting(
      url, "test_cookie=value; HttpOnly", base::Time::Now(), std::nullopt,
      std::nullopt /* cookie_partition_key */, net::CookieSourceType::kHTTP,
      &status);
  ASSERT_TRUE(cookie);
  std::vector<net::CanonicalCookie> cookies_to_set;
  cookies_to_set.push_back(*cookie);

  net::CookieOptions cookie_options;
  cookie_options.set_exclude_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  std::vector<SessionParams> params_list;
  params_list.push_back(std::move(params));

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  manager().CreateBoundSessions(std::move(params_list), GetWrappedKey(),
                                cookies_to_set, cookie_options,
                                create_future.GetCallback());
  EXPECT_THAT(
      create_future.Get<0>(),
      ElementsAre(
          net::device_bound_sessions::SessionError::ErrorType::kSuccess));
  EXPECT_THAT(create_future.Get<1>(), ElementsAre(Not(IsInclude())));
}

TEST_F(DeviceBoundSessionManagerTest, CreateBoundSessions_MultipleSessions) {
  GURL url("https://example.com/path");
  std::string session_id = "session123";

  std::vector<SessionParams> params_list;

  {
    std::vector<SessionParams::Scope::Specification> specifications;
    specifications.emplace_back(
        SessionParams::Scope::Specification::Type::kInclude, "sub.example.com",
        "/path");
    SessionParams::Scope scope;
    scope.include_site = true;
    scope.specifications = std::move(specifications);
    scope.origin = url::Origin::Create(url).Serialize();

    params_list.push_back(SessionParams(
        session_id, url, "https://example.com/refresh", std::move(scope),
        {SessionParams::Credential{"test_cookie", "SameSite=Strict"}},
        unexportable_keys::UnexportableKeyId(), {"example.com"}));
  }

  {
    SessionParams::Scope scope;
    scope.include_site = true;
    scope.origin = url::Origin::Create(url).Serialize();
    params_list.push_back(SessionParams(
        "session456", url, "https://example.com/refresh", std::move(scope),
        {SessionParams::Credential{"test_cookie", "SameSite=Strict"}},
        unexportable_keys::UnexportableKeyId(), {"example.com"}));
  }

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  base::test::TestFuture<
      const std::vector<net::device_bound_sessions::SessionError::ErrorType>&,
      std::vector<net::CookieInclusionStatus>>
      create_future;
  manager().CreateBoundSessions(std::move(params_list), GetWrappedKey(), {},
                                cookie_options, create_future.GetCallback());
  EXPECT_THAT(
      create_future.Get<0>(),
      ElementsAre(
          net::device_bound_sessions::SessionError::ErrorType::kSuccess,
          net::device_bound_sessions::SessionError::ErrorType::kSuccess));
  EXPECT_THAT(create_future.Get<1>(), IsEmpty());

  base::test::TestFuture<const std::vector<SessionKey>&> sessions_future;
  service().GetAllSessionsAsync(sessions_future.GetCallback());
  EXPECT_THAT(
      sessions_future.Get(),
      ElementsAre(
          SessionKey(net::SchemefulSite(url), Session::Id(session_id)),
          SessionKey(net::SchemefulSite(url), Session::Id("session456"))));
}

}  // namespace

}  // namespace network
