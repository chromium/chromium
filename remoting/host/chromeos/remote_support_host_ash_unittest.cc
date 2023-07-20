// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remote_support_host_ash.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromeos/browser_interop.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/session_storage.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/host/policy_watcher.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using base::test::TestFuture;
using remoting::features::kEnableCrdAdminRemoteAccessV2;

// Matcher that checks if the result of a `StartSupportSession` request
// indicates we failed to start the session
auto IsError() {
  return testing::Pointee(testing::Property(
      &mojom::StartSupportSessionResponse::is_support_session_error,
      testing::Eq(true)));
}

// Matcher that checks if the result of a `StartSupportSession` request
// indicates we could start the session.
auto IsSuccessfull() {
  return testing::Pointee(testing::Property(
      &mojom::StartSupportSessionResponse::is_support_session_error,
      testing::Eq(false)));
}

class FakeIt2MeHost : public It2MeHost {
 public:
  FakeIt2MeHost() = default;
  FakeIt2MeHost(const FakeIt2MeHost&) = delete;
  FakeIt2MeHost& operator=(const FakeIt2MeHost&) = delete;

  // `It2MeHost` implementation:
  void Connect(std::unique_ptr<ChromotingHostContext> context,
               base::Value::Dict policies,
               std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
               base::WeakPtr<It2MeHost::Observer> observer,
               CreateDeferredConnectContext create_context,
               const std::string& user_name,
               const protocol::IceConfig& ice_config) override {
    observer_ = observer;
    user_name_ = user_name;
    connect_waiter_.SetValue();
  }
  void Disconnect() override {}
  void set_chrome_os_enterprise_params(
      ChromeOsEnterpriseParams value) override {
    enterprise_params_ = value;
  }

  bool WaitForConnectCall() { return connect_waiter_.Wait(); }

  std::string user_name() const { return user_name_; }
  ChromeOsEnterpriseParams enterprise_params() const {
    return enterprise_params_;
  }

  It2MeHost::Observer& observer() {
    CHECK(observer_) << "`Connect()` has not been invoked";
    CHECK(observer_.MaybeValid());
    return *observer_;
  }

 private:
  ~FakeIt2MeHost() override = default;

  base::WeakPtr<It2MeHost::Observer> observer_;
  std::string user_name_;
  ChromeOsEnterpriseParams enterprise_params_;
  TestFuture<void> connect_waiter_;
};

class FakeIt2MeHostFactory : public It2MeHostFactory {
 public:
  explicit FakeIt2MeHostFactory(scoped_refptr<FakeIt2MeHost> host)
      : host_(host) {}

  ~FakeIt2MeHostFactory() override = default;

  std::unique_ptr<It2MeHostFactory> Clone() const override {
    return std::make_unique<FakeIt2MeHostFactory>(host_);
  }

  scoped_refptr<It2MeHost> CreateIt2MeHost() override { return host_; }

 private:
  scoped_refptr<FakeIt2MeHost> host_;
};

// Implementation of `BrowserInterop` that returns testing instances of
// the requested resources, and which will wait in its resources until
// everything is properly cleaned up.
class FakeBrowserInterop : public BrowserInterop {
 public:
  FakeBrowserInterop() = default;
  FakeBrowserInterop(const FakeBrowserInterop&) = delete;
  FakeBrowserInterop& operator=(const FakeBrowserInterop&) = delete;

  // `BrowserInterop` implementation:
  std::unique_ptr<ChromotingHostContext> CreateChromotingHostContext()
      override {
    return ChromotingHostContext::CreateForTesting(
        auto_thread_task_runner_, url_loader_factory_.GetSafeWeakWrapper());
  }

  std::unique_ptr<PolicyWatcher> CreatePolicyWatcher() override {
    return PolicyWatcher::CreateFromPolicyLoaderForTesting(
        std::make_unique<policy::FakeAsyncPolicyLoader>(
            base::SingleThreadTaskRunner::GetCurrentDefault()));
  }

 private:
  ~FakeBrowserInterop() override {
    // We must wait until all `ChromotingHostContext` instances we created
    // are destroyed (and until the `AutoThreadTaskRunner` instances they
    // create are destroyed as well).
    // We know this is the case when they stop referencing
    // `auto_thread_task_runner_`, so we drop our reference to it and wait until
    // the refcount reaches zero.
    auto_thread_task_runner_.reset();
    EXPECT_TRUE(on_auto_thread_task_runner_deleted_.Wait());
  }

  base::test::TestFuture<void> on_auto_thread_task_runner_deleted_;

  scoped_refptr<AutoThreadTaskRunner> auto_thread_task_runner_ =
      base::MakeRefCounted<AutoThreadTaskRunner>(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          on_auto_thread_task_runner_deleted_.GetCallback());

  network::TestURLLoaderFactory url_loader_factory_;
};

class InMemorySessionStorage : public SessionStorage {
 public:
  InMemorySessionStorage() = default;
  InMemorySessionStorage(const InMemorySessionStorage&) = delete;
  InMemorySessionStorage& operator=(const InMemorySessionStorage&) = delete;
  ~InMemorySessionStorage() override = default;

  // `SessionStorage` implementation:
  void StoreSession(const base::Value::Dict& information,
                    base::OnceClosure on_done) override {
    session_ = information.Clone();
    std::move(on_done).Run();
  }
  void DeleteSession(base::OnceClosure on_done) override {
    session_.reset();
    std::move(on_done).Run();
  }
  void RetrieveSession(
      base::OnceCallback<void(absl::optional<base::Value::Dict>)> on_done)
      override {
    if (session_.has_value()) {
      std::move(on_done).Run(session_.value().Clone());
    } else {
      std::move(on_done).Run(absl::nullopt);
    }
  }
  void HasSession(base::OnceCallback<void(bool)> on_done) const override {
    std::move(on_done).Run(session_.has_value());
  }

 private:
  absl::optional<base::Value::Dict> session_;
};

bool HasSession(SessionStorage& storage) {
  base::test::TestFuture<bool> waiter_;
  storage.HasSession(waiter_.GetCallback());
  return waiter_.Get();
}

}  // namespace

class RemoteSupportHostAshTest : public testing::TestWithParam<bool> {
 public:
  RemoteSupportHostAshTest() = default;
  RemoteSupportHostAshTest(const RemoteSupportHostAshTest&) = delete;
  RemoteSupportHostAshTest& operator=(const RemoteSupportHostAshTest&) = delete;
  ~RemoteSupportHostAshTest() override = default;

  FakeIt2MeHost& it2me_host() { return *host_.get(); }
  RemoteSupportHostAsh& support_host() { return support_host_; }

  mojom::SupportSessionParams GetSupportSessionParams() {
    mojom::SupportSessionParams params;
    params.user_name = "<the-user>";
    return params;
  }

  mojom::StartSupportSessionResponsePtr StartSession(
      absl::optional<ChromeOsEnterpriseParams> enterprise_params) {
    return StartSession(GetSupportSessionParams(), enterprise_params);
  }

  mojom::StartSupportSessionResponsePtr StartSession(
      const mojom::SupportSessionParams& params,
      absl::optional<ChromeOsEnterpriseParams> enterprise_params) {
    TestFuture<mojom::StartSupportSessionResponsePtr> connect_result;
    support_host().StartSession(params, enterprise_params,
                                connect_result.GetCallback());

    EXPECT_TRUE(connect_result.Wait());
    return connect_result.Take();
  }

  mojom::StartSupportSessionResponsePtr ReconnectToSession(
      SessionId id = kEnterpriseSessionId) {
    TestFuture<mojom::StartSupportSessionResponsePtr> connect_result;
    support_host().ReconnectToSession(id, connect_result.GetCallback());
    return connect_result.Take();
  }

  void SignalClientIsConnected() {
    it2me_host().observer().OnStateChanged(It2MeHostState::kConnected,
                                           protocol::ErrorCode::OK);
  }

  InMemorySessionStorage& session_storage() { return session_storage_; }

  bool StoreReconnectableSessionInformation(
      mojom::SupportSessionParams params,
      ChromeOsEnterpriseParams enterprise_params = {.allow_reconnections =
                                                        true},
      std::string remote_user_email = "remote-user@email.com") {
    // Only reconnectable sessions can be stored as reconnectable sessions.
    CHECK(enterprise_params.allow_reconnections);

    // We do not want our test to make any assumptions about how the
    // reconnectable session information is stored, so we store the
    // reconnectable session information by creating a second
    // `RemoteSupportHostAsh` and using it to start a reconnectable session.
    scoped_refptr<FakeIt2MeHost> it2me_host{
        base::MakeRefCounted<FakeIt2MeHost>()};
    RemoteSupportHostAsh support_host{
        std::make_unique<FakeIt2MeHostFactory>(it2me_host), browser_interop_,
        session_storage_, base::DoNothing()};

    support_host.StartSession(params, enterprise_params, base::DoNothing());
    EXPECT_TRUE(it2me_host->WaitForConnectCall());
    it2me_host->observer().OnClientAuthenticated(remote_user_email);
    it2me_host->observer().OnStateChanged(It2MeHostState::kConnected,
                                          protocol::ErrorCode::OK);

    return HasSession(session_storage());
  }

  void EnableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndDisableFeature(feature);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  base::test::ScopedFeatureList feature_;

  scoped_refptr<FakeBrowserInterop> browser_interop_{
      base::MakeRefCounted<FakeBrowserInterop>()};

  InMemorySessionStorage session_storage_;
  scoped_refptr<FakeIt2MeHost> host_{base::MakeRefCounted<FakeIt2MeHost>()};
  RemoteSupportHostAsh support_host_{
      std::make_unique<FakeIt2MeHostFactory>(host_), browser_interop_,
      session_storage_, base::DoNothing()};
};

TEST_F(RemoteSupportHostAshTest, ShouldSendConnectMessageWhenStarting) {
  support_host().StartSession(GetSupportSessionParams(),
                              ChromeOsEnterpriseParams{}, base::DoNothing());

  EXPECT_TRUE(it2me_host().WaitForConnectCall());
}

TEST_F(RemoteSupportHostAshTest, ShouldInvokeConnectCallbackWhenStarted) {
  TestFuture<mojom::StartSupportSessionResponsePtr> connect_result;
  support_host().StartSession(GetSupportSessionParams(),
                              ChromeOsEnterpriseParams{},
                              connect_result.GetCallback());

  ASSERT_TRUE(connect_result.Wait());
  EXPECT_FALSE(connect_result.Get()->is_support_session_error());
}

TEST_F(RemoteSupportHostAshTest, ShouldPassUserNameToIt2MeHostWhenStarting) {
  mojom::SupportSessionParams params = GetSupportSessionParams();
  params.user_name = "<the-user-name>";

  StartSession(params, ChromeOsEnterpriseParams{});

  EXPECT_EQ(it2me_host().user_name(), params.user_name);
}

TEST_F(RemoteSupportHostAshTest, ShouldPassOAuthTokenToIt2MeHostWhenStarting) {
  mojom::SupportSessionParams params = GetSupportSessionParams();
  params.oauth_access_token = "<the-oauth-token>";

  StartSession(params, ChromeOsEnterpriseParams{});

  EXPECT_EQ(it2me_host().user_name(), params.user_name);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassSuppressNotificationsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.suppress_notifications = value});

  EXPECT_EQ(it2me_host().enterprise_params().suppress_notifications, value);
}
TEST_P(RemoteSupportHostAshTest,
       ShouldPassTerminateUponInputToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.terminate_upon_input = value});

  EXPECT_EQ(it2me_host().enterprise_params().terminate_upon_input, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassCurtainLocalUserSessionToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.curtain_local_user_session = value});

  EXPECT_EQ(it2me_host().enterprise_params().curtain_local_user_session, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassShowTroubleshootingToolsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.show_troubleshooting_tools = value});

  EXPECT_EQ(it2me_host().enterprise_params().show_troubleshooting_tools, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowTroubleshootingToolsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.allow_troubleshooting_tools = value});

  EXPECT_EQ(it2me_host().enterprise_params().allow_troubleshooting_tools,
            value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowReconnectionsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.allow_reconnections = value});

  EXPECT_EQ(it2me_host().enterprise_params().allow_reconnections, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowFileTransferToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.allow_file_transfer = value});

  EXPECT_EQ(it2me_host().enterprise_params().allow_file_transfer, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassSuppressUserDialogsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  StartSession(ChromeOsEnterpriseParams{.suppress_user_dialogs = value});

  EXPECT_EQ(it2me_host().enterprise_params().suppress_user_dialogs, value);
}

TEST_F(RemoteSupportHostAshTest,
       ShouldNotStoreSessionInfoBeforeClientConnects) {
  StartSession(ChromeOsEnterpriseParams{.allow_reconnections = true});

  ASSERT_FALSE(HasSession(session_storage()));
}

TEST_F(RemoteSupportHostAshTest,
       ShouldStoreSessionInfoWhenClientConnectsToReconnectableSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  StartSession(ChromeOsEnterpriseParams{.allow_reconnections = true});
  SignalClientIsConnected();

  ASSERT_TRUE(HasSession(session_storage()));
}

TEST_F(RemoteSupportHostAshTest, ShouldNotStoreSessionInfoIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccessV2);

  StartSession(ChromeOsEnterpriseParams{.allow_reconnections = true});
  SignalClientIsConnected();

  ASSERT_FALSE(HasSession(session_storage()));
}

TEST_F(RemoteSupportHostAshTest,
       ShouldNotStoreSessionInfoIfSessionIsNotReconnectable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  StartSession(ChromeOsEnterpriseParams{.allow_reconnections = false});
  SignalClientIsConnected();

  ASSERT_FALSE(HasSession(session_storage()));
}

TEST_F(RemoteSupportHostAshTest,
       ShouldNotStoreSessionInfoIfEnterpriseParamsAreUnset) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  StartSession(absl::nullopt);
  SignalClientIsConnected();

  ASSERT_FALSE(HasSession(session_storage()));
}

TEST_F(RemoteSupportHostAshTest,
       ShouldAllowReconnectingToStoredReconnectableSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  ASSERT_TRUE(StoreReconnectableSessionInformation(GetSupportSessionParams()));

  EXPECT_THAT(ReconnectToSession(kEnterpriseSessionId), IsSuccessfull());
}

TEST_F(RemoteSupportHostAshTest,
       ShouldNotAllowReconnectingIfFeatureIsDisabled) {
  // We start by enabling the feature so we can store the reconnectable session
  // information...
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  ASSERT_TRUE(StoreReconnectableSessionInformation(GetSupportSessionParams()));

  // ... so we can test that the reconnect code itself also checks the feature
  // flag.
  DisableFeature(kEnableCrdAdminRemoteAccessV2);
  EXPECT_THAT(ReconnectToSession(kEnterpriseSessionId), IsError());
}

TEST_F(RemoteSupportHostAshTest,
       ShouldFailReconnectingIfThereIsNoStoredReconnectableSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  // Not setting up any reconnectable session.
  ASSERT_FALSE(HasSession(session_storage()));

  EXPECT_THAT(ReconnectToSession(kEnterpriseSessionId), IsError());
}

TEST_F(RemoteSupportHostAshTest, ShouldFailReconnectingIfTheSessionIdIsWrong) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  ASSERT_TRUE(StoreReconnectableSessionInformation(GetSupportSessionParams()));

  EXPECT_THAT(ReconnectToSession(SessionId{666}), IsError());
}

TEST_F(RemoteSupportHostAshTest, ShouldPassUserNameWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  auto params = GetSupportSessionParams();
  params.user_name = "the-user";
  ASSERT_TRUE(StoreReconnectableSessionInformation(params));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().user_name(), "the-user");
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassSuppressUserDialogsFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.suppress_user_dialogs = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().suppress_user_dialogs, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassSuppressNotificationsFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.suppress_notifications = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().suppress_notifications, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassTerminateUponInputFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.terminate_upon_input = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().terminate_upon_input, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassCurtainLocalUserSessionFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.curtain_local_user_session = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().curtain_local_user_session, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassShowTroubleshootingToolsFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.show_troubleshooting_tools = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().show_troubleshooting_tools, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowTroubleshootingToolsFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.allow_troubleshooting_tools = value, .allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().allow_troubleshooting_tools,
            value);
}

TEST_F(RemoteSupportHostAshTest,
       ShouldPassAllowReconnectionsFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  // We can't test 'false' since there is no way to reconnect to a session
  // with `allow_reconnections` set to false.
  const bool value = true;

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(), {.allow_reconnections = true}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().allow_reconnections, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowFileTransferFieldWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const bool value = GetParam();

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(),
      {.allow_reconnections = true, .allow_file_transfer = value}));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().enterprise_params().allow_file_transfer, value);
}

TEST_F(RemoteSupportHostAshTest,
       ShouldUseRemoteUserAsAuthorizedHelperWhenReconnectingToSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  ASSERT_TRUE(StoreReconnectableSessionInformation(
      GetSupportSessionParams(), {.allow_reconnections = true},
      "the-remote-user@domain.com"));

  ReconnectToSession(kEnterpriseSessionId);

  EXPECT_EQ(it2me_host().authorized_helper(), "the-remote-user@domain.com");
}

INSTANTIATE_TEST_SUITE_P(RemoteSupportHostAshTest,
                         RemoteSupportHostAshTest,
                         testing::Bool());

}  // namespace remoting
