// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remote_support_host_ash.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromeos/browser_interop.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/host/policy_watcher.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using base::test::TestFuture;

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
      const ChromeOsEnterpriseParams& enterprise_params) {
    return StartSession(GetSupportSessionParams(), enterprise_params);
  }

  mojom::StartSupportSessionResponsePtr StartSession(
      const mojom::SupportSessionParams& params,
      const ChromeOsEnterpriseParams& enterprise_params) {
    TestFuture<mojom::StartSupportSessionResponsePtr> connect_result;

    support_host().StartSession(params.Clone(), enterprise_params,
                                connect_result.GetCallback());

    EXPECT_TRUE(connect_result.Wait());
    return connect_result.Take();
  }

 private:
  // content::BrowserTaskEnvironment environment_;
  base::test::SingleThreadTaskEnvironment environment_;

  scoped_refptr<FakeBrowserInterop> browser_interop_{
      base::MakeRefCounted<FakeBrowserInterop>()};

  scoped_refptr<FakeIt2MeHost> host_{base::MakeRefCounted<FakeIt2MeHost>()};
  RemoteSupportHostAsh support_host_{
      std::make_unique<FakeIt2MeHostFactory>(host_), browser_interop_,
      base::DoNothing()};
};

TEST_F(RemoteSupportHostAshTest, ShouldSendConnectMessageWhenStarting) {
  support_host().StartSession(GetSupportSessionParams().Clone(),
                              ChromeOsEnterpriseParams{}, base::DoNothing());

  EXPECT_TRUE(it2me_host().WaitForConnectCall());
}

TEST_F(RemoteSupportHostAshTest, ShouldInvokeConnectCallbackWhenStarted) {
  TestFuture<mojom::StartSupportSessionResponsePtr> connect_result;
  support_host().StartSession(GetSupportSessionParams().Clone(),
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

  ChromeOsEnterpriseParams params;
  params.suppress_notifications = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().suppress_notifications, value);
}
TEST_P(RemoteSupportHostAshTest,
       ShouldPassTerminateUponInputToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.terminate_upon_input = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().terminate_upon_input, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassCurtainLocalUserSessionToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.curtain_local_user_session = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().curtain_local_user_session, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassShowTroubleshootingToolsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.show_troubleshooting_tools = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().show_troubleshooting_tools, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowTroubleshootingToolsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.allow_troubleshooting_tools = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().allow_troubleshooting_tools,
            value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowReconnectionsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.allow_reconnections = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().allow_reconnections, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassAllowFileTransferToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.allow_file_transfer = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().allow_file_transfer, value);
}

TEST_P(RemoteSupportHostAshTest,
       ShouldPassSuppressUserDialogsToIt2MeHostWhenStarting) {
  const bool value = GetParam();

  ChromeOsEnterpriseParams params;
  params.suppress_user_dialogs = value;
  StartSession(params);

  EXPECT_EQ(it2me_host().enterprise_params().suppress_user_dialogs, value);
}

INSTANTIATE_TEST_SUITE_P(RemoteSupportHostAshTest,
                         RemoteSupportHostAshTest,
                         testing::Bool());

}  // namespace remoting
