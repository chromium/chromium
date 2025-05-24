// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/http_status.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/proto/remote_support_service.h"
#include "remoting/proto/remoting/v1/remote_support_host_messages.pb.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

using RegisterSupportHostResponseCallback = base::OnceCallback<void(
    const HttpStatus&,
    std::unique_ptr<apis::v1::RegisterSupportHostResponse>)>;

constexpr char kSupportId[] = "123321456654";
constexpr base::TimeDelta kSupportIdLifetime = base::Minutes(5);
constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";
constexpr char kTestAuthorizedHelper[] = "helpful_dude@chromoting.com";
constexpr char kPublicKey[] = "fake_public_key";
constexpr char kHostVersion[] = "1.2.3.4";
constexpr char kHostOsName[] = "Windows 11";
constexpr char kHostOsVersion[] = "5.6.7.8";

void ValidateRegisterHost(const apis::v1::RegisterSupportHostRequest& request) {
  ASSERT_EQ(request.host_version(), kHostVersion);
  ASSERT_EQ(request.host_os_name(), kHostOsName);
  ASSERT_EQ(request.host_os_version(), kHostOsVersion);
  ASSERT_EQ(request.tachyon_id(), kFtlId);
  ASSERT_EQ(request.public_key(), kPublicKey);
}

void RespondOk(RegisterSupportHostResponseCallback callback) {
  auto response = std::make_unique<apis::v1::RegisterSupportHostResponse>();
  response->set_support_id(kSupportId);
  response->set_support_id_lifetime_seconds(kSupportIdLifetime.InSeconds());
  std::move(callback).Run(HttpStatus::OK(), std::move(response));
}

decltype(auto) DoValidateRegisterHostAndRespondOk() {
  return [=](std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
             RegisterSupportHostResponseCallback callback) {
    ValidateRegisterHost(*request);
    RespondOk(std::move(callback));
  };
}

decltype(auto) DoValidateEnterpriseOptionsAndRespondOk(
    const ChromeOsEnterpriseParams& params) {
  return [=](std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
             RegisterSupportHostResponseCallback callback) {
    ASSERT_TRUE(request->has_chrome_os_enterprise_options());
    auto& options = request->chrome_os_enterprise_options();
    ASSERT_EQ(options.allow_troubleshooting_tools(),
              params.allow_troubleshooting_tools);
    ASSERT_EQ(options.show_troubleshooting_tools(),
              params.show_troubleshooting_tools);
    ASSERT_EQ(options.allow_reconnections(), params.allow_reconnections);
    ASSERT_EQ(options.allow_file_transfer(), params.allow_file_transfer);
    ASSERT_EQ(options.connection_dialog_required(),
              params.connection_dialog_required);
    ASSERT_EQ(options.allow_remote_input(), params.allow_remote_input);
    ASSERT_EQ(options.allow_clipboard_sync(), params.allow_clipboard_sync);

    if (params.connection_auto_accept_timeout.is_zero()) {
      ASSERT_FALSE(options.has_connection_auto_accept_timeout());
    } else {
      ASSERT_EQ(options.connection_auto_accept_timeout().seconds(),
                params.connection_auto_accept_timeout.InSeconds());
    }
    ValidateRegisterHost(*request);
    RespondOk(std::move(callback));
  };
}

decltype(auto) DoValidateAuthorizedHelperAndRespondOk() {
  return [=](std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
             RegisterSupportHostResponseCallback callback) {
    ASSERT_TRUE(request->has_authorized_helper());
    ASSERT_EQ(request->authorized_helper(), kTestAuthorizedHelper);
    ValidateRegisterHost(*request);
    RespondOk(std::move(callback));
  };
}

}  // namespace

class RemotingRegisterSupportHostTest : public testing::Test {
 public:
  RemotingRegisterSupportHostTest() {
    register_host_request_ =
        std::make_unique<RemotingRegisterSupportHostRequest>(
            std::make_unique<FakeOAuthTokenGetter>(
                OAuthTokenGetter::Status::SUCCESS,
                OAuthTokenInfo("fake_access_token", "fake_email")),
            nullptr);

    auto register_host_client =
        std::make_unique<MockRegisterSupportHostClient>();
    register_host_client_ = register_host_client.get();
    register_host_request_->register_host_client_ =
        std::move(register_host_client);
    simple_host_.public_key = kPublicKey;
    simple_host_.version = kHostVersion;
    simple_host_.operating_system_info.name = kHostOsName;
    simple_host_.operating_system_info.version = kHostOsVersion;
    SignalingAddress ftl_address(kFtlId);
    ftl_address.GetFtlInfo(&simple_host_.tachyon_account_info.account_id,
                           &simple_host_.tachyon_account_info.registration_id);
  }

 protected:
  using RegisterHostCallback =
      RemotingRegisterSupportHostRequest::RegisterHostCallback;

  class MockRegisterSupportHostClient final
      : public RemotingRegisterSupportHostRequest::RegisterSupportHostClient {
   public:
    MOCK_METHOD2(RegisterSupportHost,
                 void(std::unique_ptr<apis::v1::RegisterSupportHostRequest>,
                      RegisterSupportHostResponseCallback));
    MOCK_METHOD0(CancelPendingRequests, void());
  };

  void RegisterHost(
      const internal::RemoteSupportHostStruct& host,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      RegisterHostCallback callback) {
    register_host_request_->RegisterHost(std::move(host), enterprise_params,
                                         std::move(callback));
  }

  void CancelPendingRequests() {
    register_host_request_->CancelPendingRequests();
  }

  internal::RemoteSupportHostStruct simple_host_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<RemotingRegisterSupportHostRequest> register_host_request_;
  raw_ptr<MockRegisterSupportHostClient> register_host_client_ = nullptr;
  std::string authorized_helper_;
};

TEST_F(RemotingRegisterSupportHostTest, RegisterFtl) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(DoValidateRegisterHostAndRespondOk());

  base::MockCallback<RegisterHostCallback> register_callback;
  EXPECT_CALL(register_callback,
              Run(HttpStatus::OK(), kSupportId, kSupportIdLifetime))
      .Times(1);

  RegisterHost(simple_host_, std::nullopt, register_callback.Get());
}

TEST_F(RemotingRegisterSupportHostTest, RegisterWithEnterpriseOptionsDisabled) {
  ChromeOsEnterpriseParams params;
  params.show_troubleshooting_tools = false;
  params.allow_troubleshooting_tools = false;
  params.allow_reconnections = false;
  params.allow_file_transfer = false;
  params.connection_dialog_required = false;
  params.allow_remote_input = false;
  params.allow_clipboard_sync = false;
  params.connection_auto_accept_timeout = base::Seconds(0);

  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(DoValidateEnterpriseOptionsAndRespondOk(params));

  base::MockCallback<RegisterHostCallback> register_callback;
  EXPECT_CALL(register_callback,
              Run(HttpStatus::OK(), kSupportId, kSupportIdLifetime))
      .Times(1);

  RegisterHost(simple_host_, std::move(params), register_callback.Get());
}

TEST_F(RemotingRegisterSupportHostTest, RegisterWithEnterpriseOptionsEnabled) {
  ChromeOsEnterpriseParams params;
  params.show_troubleshooting_tools = true;
  params.allow_troubleshooting_tools = true;
  params.allow_reconnections = true;
  params.allow_file_transfer = true;
  params.connection_dialog_required = true;
  params.allow_remote_input = true;
  params.allow_clipboard_sync = true;
  params.connection_auto_accept_timeout = base::Seconds(30);

  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(DoValidateEnterpriseOptionsAndRespondOk(params));

  base::MockCallback<RegisterHostCallback> register_callback;
  EXPECT_CALL(register_callback,
              Run(HttpStatus::OK(), kSupportId, kSupportIdLifetime))
      .Times(1);

  RegisterHost(simple_host_, std::move(params), register_callback.Get());
}

TEST_F(RemotingRegisterSupportHostTest, RegisterWithAuthorizedHelper) {
  internal::RemoteSupportHostStruct host_with_authorized_helper = simple_host_;
  host_with_authorized_helper.authorized_helper_email = kTestAuthorizedHelper;

  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(DoValidateAuthorizedHelperAndRespondOk());

  base::MockCallback<RegisterHostCallback> register_callback;
  EXPECT_CALL(register_callback,
              Run(HttpStatus::OK(), kSupportId, kSupportIdLifetime))
      .Times(1);

  RegisterHost(host_with_authorized_helper, std::nullopt,
               register_callback.Get());
}

TEST_F(RemotingRegisterSupportHostTest, FailedWithDeadlineExceeded) {
  HttpStatus deadline_exceeded{HttpStatus::Code::DEADLINE_EXCEEDED,
                               "deadline exceeded"};
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(
          [=](std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
              RegisterSupportHostResponseCallback callback) {
            ValidateRegisterHost(*request);
            std::move(callback).Run(deadline_exceeded, nullptr);
          });

  base::MockCallback<RegisterHostCallback> register_callback;
  EXPECT_CALL(register_callback, Run(deadline_exceeded, _, _)).Times(1);

  RegisterHost(simple_host_, std::nullopt, register_callback.Get());
}

}  // namespace remoting
