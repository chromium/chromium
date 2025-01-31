// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_manager.h"

#include "base/test/task_environment.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

using net::device_bound_sessions::RegistrationFetcher;
using net::device_bound_sessions::RegistrationFetcherParam;
using net::device_bound_sessions::Session;
using net::device_bound_sessions::SessionAccess;
using net::device_bound_sessions::SessionKey;
using net::device_bound_sessions::SessionParams;
using net::device_bound_sessions::SessionService;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

// TODO(drubery): Consolidate the test fetcher here with the test
// fetchers in session_service_impl_unittest.cc
std::optional<RegistrationFetcher::RegistrationCompleteParams> TestFetcher() {
  std::vector<SessionParams::Credential> cookie_credentials;
  cookie_credentials.push_back(
      SessionParams::Credential{"test_cookie", "secure"});
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.origin = "example.com";
  SessionParams session_params("SessionId", "https://example.com/refresh",
                               std::move(scope), std::move(cookie_credentials));
  unexportable_keys::UnexportableKeyId key_id;
  return std::make_optional<RegistrationFetcher::RegistrationCompleteParams>(
      std::move(session_params), std::move(key_id),
      GURL("https://example.com/"));
}

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
        service_(SessionService::Create(context_.get())),
        manager_(DeviceBoundSessionManager::Create(service_.get())) {}

  DeviceBoundSessionManager& manager() { return *manager_; }

  SessionService& service() { return *service_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<SessionService> service_;
  std::unique_ptr<DeviceBoundSessionManager> manager_;
};

TEST_F(DeviceBoundSessionManagerTest, ObserverNotifiesChangeOnlyOnSite) {
  RegistrationFetcher::SetFetcherForTesting(&TestFetcher);

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

  RegistrationFetcher::SetFetcherForTesting(nullptr);
}

}  // namespace

}  // namespace network
