// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter_base.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_config.h"
#include "remoting/host/setup/daemon_controller.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Response for spoofing Gaia calls.
constexpr char kAccessToken[] = "LEGIT_ACCESS_TOKEN";
constexpr char kRefreshToken[] = "LEGIT_REFRESH_TOKEN";
constexpr char kGetTokensResponse[] = R"({
            "refresh_token": "LEGIT_REFRESH_TOKEN",
            "access_token": "LEGIT_ACCESS_TOKEN",
            "expires_in": 3600,
            "token_type": "Bearer"
         })";
constexpr char kGetUserEmailResponseForUser[] = R"({"email": "user@test.com"})";
constexpr char kGetUserEmailResponseForServiceAccount[] =
    R"({"email": "robot@chromoting.com"})";

// Known values for testing config file generation.
constexpr char kTestUserEmail[] = "user@test.com";
constexpr char kTestRobotEmail[] = "robot@chromoting.com";
constexpr char kTestRobotAuthCode[] = "robot_auth_code";
constexpr char kTestDirectoryId[] = "test_directory_id";
constexpr char kTestMachineName[] = "test_machine_name";

constexpr char kTestConfigValuePath[] = "test_config_value";
constexpr char kTestConfigValue[] = "so_much_value";

class TestDaemonControllerDelegate : public DaemonController::Delegate {
 public:
  TestDaemonControllerDelegate();

  TestDaemonControllerDelegate(const TestDaemonControllerDelegate&) = delete;
  TestDaemonControllerDelegate& operator=(const TestDaemonControllerDelegate&) =
      delete;

  ~TestDaemonControllerDelegate() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::optional<base::Value::Dict> GetConfig() override;
  void CheckPermission(bool it2me,
                       DaemonController::BoolCallback callback) override;
  void SetConfigAndStart(base::Value::Dict config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(base::Value::Dict config,
                    DaemonController::CompletionCallback done) override;
  void Stop(DaemonController::CompletionCallback done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;

  // Methods used for controlling behavior of the fake instance.
  void set_result_for_config_and_start(DaemonController::AsyncResult result) {
    result_for_config_and_start_ = result;
  }
  void set_initial_config(base::Value::Dict config) {
    config_ = std::move(config);
  }

  bool stop_called() { return stop_called_; }

 private:
  bool stop_called_ = false;
  DaemonController::State state_ = DaemonController::STATE_STOPPED;
  DaemonController::AsyncResult result_for_config_and_start_ =
      DaemonController::RESULT_OK;
  base::Value::Dict config_;
};

TestDaemonControllerDelegate::TestDaemonControllerDelegate() = default;

TestDaemonControllerDelegate::~TestDaemonControllerDelegate() = default;

DaemonController::State TestDaemonControllerDelegate::GetState() {
  return state_;
}

std::optional<base::Value::Dict> TestDaemonControllerDelegate::GetConfig() {
  return config_.Clone();
}

void TestDaemonControllerDelegate::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  ADD_FAILURE() << "Unexpected call to CheckPermission()";
}

void TestDaemonControllerDelegate::SetConfigAndStart(
    base::Value::Dict config,
    bool consent,
    DaemonController::CompletionCallback done) {
  config_ = std::move(config);
  state_ = DaemonController::STATE_STARTED;
  std::move(done).Run(result_for_config_and_start_);
}

void TestDaemonControllerDelegate::UpdateConfig(
    base::Value::Dict config,
    DaemonController::CompletionCallback done) {
  ADD_FAILURE() << "Unexpected call to UpdateConfig()";
}

void TestDaemonControllerDelegate::Stop(
    DaemonController::CompletionCallback done) {
  stop_called_ = true;
  state_ = DaemonController::STATE_STOPPED;
  std::move(done).Run(DaemonController::RESULT_OK);
}

DaemonController::UsageStatsConsent
TestDaemonControllerDelegate::GetUsageStatsConsent() {
  ADD_FAILURE() << "Unexpected call to GetUsageStatsConsent()";
  return DaemonController::UsageStatsConsent();
}

class TestHostStarter : public HostStarterBase {
 public:
  TestHostStarter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<DaemonController> daemon_controller,
      base::OnceClosure configure_gaia_for_service_account);

  TestHostStarter(const TestHostStarter&) = delete;
  TestHostStarter& operator=(const TestHostStarter&) = delete;

  ~TestHostStarter() override;

  // HostStarterBase implementation.
  void RegisterNewHost(const std::string& public_key,
                       std::optional<std::string> access_token) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_removed) override;
  void ApplyConfigValues(base::Value::Dict& config) override;
  void ReportError(const std::string& error_message,
                   base::OnceClosure on_done) override;

  std::string& user_access_token() { return user_access_token_; }
  std::string& error_message() { return error_message_; }

  void clear_directory_id() { directory_id_.clear(); }

  void clear_owner_account_email() { owner_account_email_.clear(); }

  void clear_service_account_email() { service_account_email_.clear(); }

  void clear_robot_authorization_code() { robot_authorization_code_.clear(); }

 private:
  std::string user_access_token_;
  std::string error_message_;

  std::string directory_id_{kTestDirectoryId};
  std::string owner_account_email_{kTestUserEmail};
  std::string service_account_email_{kTestRobotEmail};
  std::string robot_authorization_code_{kTestRobotAuthCode};

  base::OnceClosure configure_gaia_for_service_account_;
};

TestHostStarter::TestHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<DaemonController> daemon_controller,
    base::OnceClosure configure_gaia_for_service_account)
    : HostStarterBase(url_loader_factory),
      configure_gaia_for_service_account_(
          std::move(configure_gaia_for_service_account)) {
  SetDaemonControllerForTest(daemon_controller);
}

TestHostStarter::~TestHostStarter() = default;

void TestHostStarter::RegisterNewHost(const std::string& public_key,
                                      std::optional<std::string> access_token) {
  // Set up the TestUrlLoaderFactory so it will provide service account
  // responses rather than user account responses.
  std::move(configure_gaia_for_service_account_).Run();

  user_access_token_ = access_token.value_or(std::string());
  OnNewHostRegistered(directory_id_, owner_account_email_,
                      service_account_email_, robot_authorization_code_);
}

void TestHostStarter::RemoveOldHostFromDirectory(base::OnceClosure on_removed) {
  std::move(on_removed).Run();
}

void TestHostStarter::ApplyConfigValues(base::Value::Dict& config) {
  config.Set(kTestConfigValuePath, kTestConfigValue);
}

void TestHostStarter::ReportError(const std::string& error_message,
                                  base::OnceClosure on_done) {
  error_message_ = error_message;
  LOG(ERROR) << error_message_;
  std::move(on_done).Run();
}

}  // namespace

class HostStarterBaseTest : public testing::Test {
 public:
  HostStarterBaseTest();
  ~HostStarterBaseTest() override;

  void SetUp() override;
  void TearDown() override;

  HostStarter::CompletionCallback GetCompletionCallback() {
    return base::BindOnce(&HostStarterBaseTest::CompletionHandler,
                          base::Unretained(this));
  }

 protected:
  void RunUntilQuit();
  void CompletionHandler(HostStarter::Result result);

  void ConfigureGaiaResponseForUser();
  void ConfigureGaiaResponseForServiceAccount();

  TestHostStarter& test_host_starter() { return *test_host_starter_; }
  std::optional<HostStarter::Result>& start_result() { return start_result_; }
  TestDaemonControllerDelegate& test_daemon_controller_delegate() {
    return *test_daemon_controller_delegate_;
  }

 private:
  base::RepeatingClosure quit_closure_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Reference to the DaemonControllerDelegate used for testing, which is owned
  // by |test_host_starter_|.
  raw_ptr<TestDaemonControllerDelegate> test_daemon_controller_delegate_;
  scoped_refptr<DaemonController> daemon_controller_;

  std::optional<HostStarter::Result> start_result_;
  std::unique_ptr<TestHostStarter> test_host_starter_;
};

HostStarterBaseTest::HostStarterBaseTest() = default;

HostStarterBaseTest::~HostStarterBaseTest() = default;

void HostStarterBaseTest::SetUp() {
  shared_url_loader_factory_ = test_url_loader_factory_.GetSafeWeakWrapper();
  test_daemon_controller_delegate_ = new TestDaemonControllerDelegate();
  daemon_controller_ = new DaemonController(
      base::WrapUnique(test_daemon_controller_delegate_.get()));
  test_host_starter_ = std::make_unique<TestHostStarter>(
      shared_url_loader_factory_, daemon_controller_,
      base::BindOnce(
          &HostStarterBaseTest::ConfigureGaiaResponseForServiceAccount,
          base::Unretained(this)));
  quit_closure_ = task_environment_.QuitClosure();
}

void HostStarterBaseTest::TearDown() {
  // Clear the raw pointer so it doesn't appear to be hanging.
  test_daemon_controller_delegate_ = nullptr;
  // Clear the test instance and allow the threads to shut down so ASAN doesn't
  // detect it as leaked.
  test_host_starter_.reset();

  task_environment_.GetMainThreadTaskRunner()->ReleaseSoon(
      FROM_HERE, std::move(daemon_controller_));

  task_environment_.RunUntilIdle();
}

void HostStarterBaseTest::RunUntilQuit() {
  task_environment_.RunUntilQuit();
}

void HostStarterBaseTest::CompletionHandler(HostStarter::Result result) {
  start_result_ = result;
  quit_closure_.Run();
}

void HostStarterBaseTest::ConfigureGaiaResponseForUser() {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(), kGetTokensResponse);
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
      kGetUserEmailResponseForUser);
}

void HostStarterBaseTest::ConfigureGaiaResponseForServiceAccount() {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(), kGetTokensResponse);
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
      kGetUserEmailResponseForServiceAccount);
}

TEST_F(HostStarterBaseTest, StartHostUsingOAuth) {
  HostStarter::Params params;
  params.pin = "123456";
  params.name = kTestMachineName;
  params.auth_code = "auth_me_dude";
  params.redirect_url = "/redirect";

  ConfigureGaiaResponseForUser();

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify the user access token was provided to the subclass and no errors
  // were reported.
  EXPECT_EQ(test_host_starter().user_access_token(), kAccessToken);
  EXPECT_EQ(test_host_starter().error_message(), std::string());

  // Verify the configuration dict has the expected fields populated.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
  value = config->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestRobotEmail);
  value = config->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshToken);
  value = config->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestDirectoryId);
  value = config->FindString(kHostNameConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestMachineName);
  // Verify subclass value was applied.
  value = config->FindString(kTestConfigValuePath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestConfigValue);

  // Verify Stop() was not called.
  EXPECT_FALSE(test_daemon_controller_delegate().stop_called());
}

TEST_F(HostStarterBaseTest, CorpCodePath) {
  HostStarter::Params params;
  params.username = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify no user access token was provided and no errors were reported.
  EXPECT_EQ(test_host_starter().user_access_token(), std::string());
  EXPECT_EQ(test_host_starter().error_message(), std::string());

  // Verify the configuration dict has the expected fields populated.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
  value = config->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestRobotEmail);
  value = config->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshToken);
  value = config->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestDirectoryId);
  // We use the value from GetHostname() if no name is provided.
  EXPECT_TRUE(config->FindString(kHostNameConfigPath));
  // Verify subclass value was applied.
  value = config->FindString(kTestConfigValuePath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestConfigValue);

  // Verify Stop() was not called.
  EXPECT_FALSE(test_daemon_controller_delegate().stop_called());
}

TEST_F(HostStarterBaseTest, CloudCodePath) {
  HostStarter::Params params;
  params.owner_email = kTestUserEmail;
  params.api_key = "API_KEY_FOR_TESTING";

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify no user access token was provided and no errors were reported.
  EXPECT_EQ(test_host_starter().user_access_token(), std::string());
  EXPECT_EQ(test_host_starter().error_message(), std::string());

  // Verify the configuration dict has the expected fields populated.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
  value = config->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestRobotEmail);
  value = config->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshToken);
  value = config->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestDirectoryId);
  // We use the value from GetHostname() if no name is provided.
  EXPECT_TRUE(config->FindString(kHostNameConfigPath));
  // Verify subclass value was applied.
  value = config->FindString(kTestConfigValuePath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestConfigValue);

  // Verify Stop() was not called.
  EXPECT_FALSE(test_daemon_controller_delegate().stop_called());
}

TEST_F(HostStarterBaseTest, LegacyCloudCodePath) {
  HostStarter::Params params;
  params.owner_email = kTestUserEmail;
  params.pin = "123456";

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify no user access token was provided and no errors were reported.
  EXPECT_EQ(test_host_starter().user_access_token(), std::string());
  EXPECT_EQ(test_host_starter().error_message(), std::string());

  // Verify the configuration dict has the expected fields populated.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
  value = config->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestRobotEmail);
  value = config->FindString(kOAuthRefreshTokenConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kRefreshToken);
  value = config->FindString(kHostIdConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestDirectoryId);
  // We use the value from GetHostname() if no name is provided.
  EXPECT_TRUE(config->FindString(kHostNameConfigPath));
  // Verify subclass value was applied.
  value = config->FindString(kTestConfigValuePath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestConfigValue);

  // Verify Stop() was not called.
  EXPECT_FALSE(test_daemon_controller_delegate().stop_called());
}

TEST_F(HostStarterBaseTest, ExistingHostIsStopped) {
  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  base::Value::Dict initial_config;
  initial_config.Set(kHostIdConfigPath, "old_crusty_host_id");
  test_daemon_controller_delegate().set_initial_config(
      std::move(initial_config));

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify no errors were reported.
  EXPECT_TRUE(test_host_starter().error_message().empty());

  // Verify Stop() was called.
  EXPECT_TRUE(test_daemon_controller_delegate().stop_called());
}

TEST_F(HostStarterBaseTest, OAuthFlowWithMismatchedOwnerEmail) {
  HostStarter::Params params;
  params.owner_email = "not-the-person-who-generated-the-auth-code@fraud.com";
  params.auth_code = "auth_me_dude";
  params.redirect_url = "/redirect";

  ConfigureGaiaResponseForUser();

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation failed for the expected reason.
  EXPECT_EQ(start_result(), HostStarter::PERMISSION_DENIED);

  // Verify no user access token was provided and an error was reported.
  EXPECT_EQ(test_host_starter().user_access_token(), std::string());
  EXPECT_FALSE(test_host_starter().error_message().empty());
}

TEST_F(HostStarterBaseTest, CorpFlowWithMismatchedOwnerEmailValue) {
  HostStarter::Params params;
  params.owner_email = "PLACEHOLDER_VALUE";

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify owner from the service was written to the config rather than the
  // placeholder value.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
}

TEST_F(HostStarterBaseTest, RegisterNewHostCallbackDoesNotProvideId) {
  test_host_starter().clear_directory_id();

  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation failed for the expected reason.
  EXPECT_EQ(start_result(), HostStarter::REGISTRATION_ERROR);

  // Verify an error was reported.
  EXPECT_NE(test_host_starter().error_message(), std::string());
}

TEST_F(HostStarterBaseTest, RegisterNewHostCallbackProvideMismatchedId) {
  HostStarter::Params params;
  params.id = "my-guid";
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation failed for the expected reason.
  EXPECT_EQ(start_result(), HostStarter::REGISTRATION_ERROR);

  // Verify an error was reported.
  EXPECT_NE(test_host_starter().error_message(), std::string());
}

TEST_F(HostStarterBaseTest, RegisterNewHostCallbackDoesNotProvideAuthCode) {
  test_host_starter().clear_robot_authorization_code();

  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation failed for the expected reason.
  EXPECT_EQ(start_result(), HostStarter::REGISTRATION_ERROR);

  // Verify an error was reported.
  EXPECT_NE(test_host_starter().error_message(), std::string());
}

TEST_F(HostStarterBaseTest, RegisterNewHostCallbackDoesNotProvideOwnerEmail) {
  test_host_starter().clear_owner_account_email();

  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify the owner email to be written to the config.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kHostOwnerConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestUserEmail);
}

TEST_F(HostStarterBaseTest,
       RegisterNewHostCallbackDoesNotProvideServiceAccount) {
  test_host_starter().clear_service_account_email();

  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation completed successfully.
  EXPECT_EQ(start_result(), HostStarter::START_COMPLETE);

  // Verify the expected service account was written to the config.
  auto config = test_daemon_controller_delegate().GetConfig();
  ASSERT_TRUE(config.has_value());
  const std::string* value = config->FindString(kServiceAccountConfigPath);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, kTestRobotEmail);
}

TEST_F(HostStarterBaseTest, NewHostFailsToStart) {
  test_daemon_controller_delegate().set_result_for_config_and_start(
      DaemonController::RESULT_FAILED);

  HostStarter::Params params;
  params.owner_email = kTestUserEmail;

  test_host_starter().StartHost(std::move(params), GetCompletionCallback());
  RunUntilQuit();

  // Make sure the operation failed for the expected reason.
  EXPECT_EQ(start_result(), HostStarter::START_ERROR);

  // Verify an error was reported.
  EXPECT_NE(test_host_starter().error_message(), std::string());
}

}  // namespace remoting
