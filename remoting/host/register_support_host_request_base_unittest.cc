// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request_base.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/ssl/ssl_cert_request_info.h"
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

constexpr char kSupportId[] = "123321456654";
constexpr base::TimeDelta kSupportIdLifetime = base::Minutes(5);
constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";
const char kTestAuthorizedHelper[] = "helpful_dude@chromoting.com";

class TestRegisterSupportHostRequest : public RegisterSupportHostRequestBase {
 public:
  MOCK_METHOD(void,
              Initialize,
              (std::unique_ptr<net::ClientCertStore> client_cert_store),
              (override));
  MOCK_METHOD(void,
              RegisterHost,
              (const internal::RemoteSupportHostStruct& host,
               const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
               RegisterHostCallback callback),
              (override));
  MOCK_METHOD(void, CancelPendingRequests, (), (override));
};

class FakeClientCertStore : public net::ClientCertStore {
 public:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {}
};

}  // namespace

class RegisterSupportHostRequestBaseTest : public testing::Test {
 public:
  RegisterSupportHostRequestBaseTest() {
    register_host_request_ = std::make_unique<TestRegisterSupportHostRequest>();

    signal_strategy_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kFtlId));

    // Start in disconnected state.
    signal_strategy_->Disconnect();

    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
  }

  ~RegisterSupportHostRequestBaseTest() override {
    register_host_request_.reset();
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  using RegisterHostCallback =
      RegisterSupportHostRequestBase::RegisterHostCallback;

  static void ValidateRegisterHost(
      const internal::RemoteSupportHostStruct& host) {
    ASSERT_FALSE(host.version.empty());
    ASSERT_FALSE(host.operating_system_info.name.empty());
    ASSERT_FALSE(host.operating_system_info.version.empty());
    ASSERT_EQ(SignalingAddress::CreateFtlSignalingAddress(
                  host.tachyon_account_info.account_id,
                  host.tachyon_account_info.registration_id)
                  .id(),
              kFtlId);

    auto key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
    ASSERT_EQ(host.public_key, key_pair->GetPublicKey());
  }

  static void RespondOk(RegisterHostCallback callback) {
    std::move(callback).Run(HttpStatus::OK(), kSupportId, kSupportIdLifetime);
  }

  static decltype(auto) DoValidateRegisterHostAndRespondOk() {
    return [](const internal::RemoteSupportHostStruct& host,
              const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
              RegisterHostCallback callback) {
      ValidateRegisterHost(host);
      RespondOk(std::move(callback));
    };
  }

  static decltype(auto) DoValidateEnterpriseOptionsAndRespondOk(
      const ChromeOsEnterpriseParams& expected_enterprise_params) {
    return [=](const internal::RemoteSupportHostStruct& host,
               const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
               RegisterHostCallback callback) {
      ASSERT_TRUE(enterprise_params.has_value());
      ASSERT_EQ(*enterprise_params, expected_enterprise_params);
      ValidateRegisterHost(host);
      RespondOk(std::move(callback));
    };
  }

  static decltype(auto) DoValidateAuthorizedHelperAndRespondOk() {
    return [](const internal::RemoteSupportHostStruct& host,
              const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
              RegisterHostCallback callback) {
      ASSERT_FALSE(host.authorized_helper_email.empty());
      ASSERT_EQ(host.authorized_helper_email, kTestAuthorizedHelper);
      ValidateRegisterHost(host);
      RespondOk(std::move(callback));
    };
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestRegisterSupportHostRequest> register_host_request_;

  std::unique_ptr<SignalStrategy> signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string authorized_helper_;
};

TEST_F(RegisterSupportHostRequestBaseTest, CallInitializeWithClientCertStore) {
  auto client_cert_store = std::make_unique<FakeClientCertStore>();
  FakeClientCertStore* raw_client_cert_store = client_cert_store.get();
  EXPECT_CALL(*register_host_request_, Initialize(_))
      .WillOnce([=](std::unique_ptr<net::ClientCertStore> cert_store) {
        ASSERT_EQ(cert_store.get(), raw_client_cert_store);
      });

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::move(client_cert_store), key_pair_,
      authorized_helper_, std::nullopt, base::DoNothing());
}

TEST_F(RegisterSupportHostRequestBaseTest, RegisterFtl) {
  EXPECT_CALL(*register_host_request_, Initialize(_));
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(DoValidateRegisterHostAndRespondOk());
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::nullopt, register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RegisterSupportHostRequestBaseTest,
       RegisterWithEnterpriseOptionsDisabled) {
  ChromeOsEnterpriseParams params;
  params.show_troubleshooting_tools = false;
  params.allow_troubleshooting_tools = false;
  params.allow_reconnections = false;
  params.allow_file_transfer = false;
  params.connection_dialog_required = false;
  params.allow_remote_input = false;
  params.allow_clipboard_sync = false;
  params.connection_auto_accept_timeout = base::Seconds(0);

  EXPECT_CALL(*register_host_request_, Initialize(_));
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(DoValidateEnterpriseOptionsAndRespondOk(params));
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::move(params),
      register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RegisterSupportHostRequestBaseTest,
       RegisterWithEnterpriseOptionsEnabled) {
  ChromeOsEnterpriseParams params;
  params.show_troubleshooting_tools = true;
  params.allow_troubleshooting_tools = true;
  params.allow_reconnections = true;
  params.allow_file_transfer = true;
  params.connection_dialog_required = true;
  params.allow_remote_input = true;
  params.allow_clipboard_sync = true;
  params.connection_auto_accept_timeout = base::Seconds(30);

  EXPECT_CALL(*register_host_request_, Initialize(_));
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(DoValidateEnterpriseOptionsAndRespondOk(params));
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::move(params),
      register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RegisterSupportHostRequestBaseTest, RegisterWithAuthorizedHelper) {
  EXPECT_CALL(*register_host_request_, Initialize(_));
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(DoValidateAuthorizedHelperAndRespondOk());
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  authorized_helper_ = kTestAuthorizedHelper;

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::nullopt, register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RegisterSupportHostRequestBaseTest, FailedWithDeadlineExceeded) {
  EXPECT_CALL(*register_host_request_, Initialize(_));
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(
          [](const internal::RemoteSupportHostStruct& host,
             const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
             RegisterHostCallback callback) {
            ValidateRegisterHost(host);
            std::move(callback).Run(
                HttpStatus(HttpStatus::Code::DEADLINE_EXCEEDED,
                           "deadline exceeded"),
                {}, {});
          });
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_TIMEOUT))
      .Times(1);

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::nullopt, register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RegisterSupportHostRequestBaseTest,
       SignalingDisconnectedBeforeRegistrationSucceeds) {
  EXPECT_CALL(*register_host_request_, Initialize(_));
  RegisterHostCallback register_support_host_callback;
  EXPECT_CALL(*register_host_request_, RegisterHost(_, _, _))
      .WillOnce(
          [&](const internal::RemoteSupportHostStruct& host,
              const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
              RegisterHostCallback callback) {
            ValidateRegisterHost(host);
            register_support_host_callback = std::move(callback);
          });
  EXPECT_CALL(*register_host_request_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_ERROR))
      .Times(1);

  register_host_request_->StartRequest(
      signal_strategy_.get(), std::make_unique<FakeClientCertStore>(),
      key_pair_, authorized_helper_, std::nullopt, register_callback.Get());
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  RespondOk(std::move(register_support_host_callback));
}

}  // namespace remoting
