// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/icloud_keychain.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/mac/fake_icloud_keychain_sys.h"
#include "device/fido/mac/icloud_keychain_sys.h"
#include "device/fido/public_key_credential_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::fido::icloud_keychain {

namespace {

constexpr char kMetricName[] = "WebAuthentication.MacOS.PasskeyPermission";

static const uint8_t kAttestationObjectBytes[] = {
    0xa3, 0x63, 0x66, 0x6d, 0x74, 0x66, 0x70, 0x61, 0x63, 0x6b, 0x65, 0x64,
    0x67, 0x61, 0x74, 0x74, 0x53, 0x74, 0x6d, 0x74, 0xa3, 0x63, 0x61, 0x6c,
    0x67, 0x26, 0x63, 0x73, 0x69, 0x67, 0x58, 0x46, 0x30, 0x44, 0x02, 0x20,
    0x05, 0xaa, 0x7b, 0xcb, 0x4f, 0x15, 0xc8, 0x3d, 0x3a, 0x0b, 0x57, 0x12,
    0xa8, 0xab, 0x8d, 0x60, 0x16, 0x9b, 0xfb, 0x91, 0x91, 0xfd, 0x1d, 0xe6,
    0x30, 0xab, 0xae, 0xe3, 0x71, 0xd6, 0xfb, 0x33, 0x02, 0x20, 0x30, 0xba,
    0x47, 0x7b, 0x38, 0x06, 0x89, 0xbc, 0x46, 0x1c, 0xa1, 0x60, 0x7e, 0x99,
    0x88, 0x85, 0x7f, 0x24, 0xf9, 0x82, 0xb7, 0xb5, 0x03, 0x8f, 0x92, 0x16,
    0x86, 0xd6, 0x10, 0x50, 0x9c, 0xc8, 0x63, 0x78, 0x35, 0x63, 0x81, 0x59,
    0x02, 0xdc, 0x30, 0x82, 0x02, 0xd8, 0x30, 0x82, 0x01, 0xc0, 0xa0, 0x03,
    0x02, 0x01, 0x02, 0x02, 0x09, 0x00, 0xff, 0x87, 0x6c, 0x2d, 0xaf, 0x73,
    0x79, 0xc8, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x2e, 0x31, 0x2c, 0x30, 0x2a, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x13, 0x23, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f,
    0x20, 0x55, 0x32, 0x46, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41,
    0x20, 0x53, 0x65, 0x72, 0x69, 0x61, 0x6c, 0x20, 0x34, 0x35, 0x37, 0x32,
    0x30, 0x30, 0x36, 0x33, 0x31, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
    0x38, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f,
    0x32, 0x30, 0x35, 0x30, 0x30, 0x39, 0x30, 0x34, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x5a, 0x30, 0x6e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
    0x04, 0x06, 0x13, 0x02, 0x53, 0x45, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03,
    0x55, 0x04, 0x0a, 0x0c, 0x09, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f, 0x20,
    0x41, 0x42, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c,
    0x19, 0x41, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74, 0x69, 0x63, 0x61, 0x74,
    0x6f, 0x72, 0x20, 0x41, 0x74, 0x74, 0x65, 0x73, 0x74, 0x61, 0x74, 0x69,
    0x6f, 0x6e, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x1e, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f, 0x20, 0x55, 0x32, 0x46, 0x20,
    0x45, 0x45, 0x20, 0x53, 0x65, 0x72, 0x69, 0x61, 0x6c, 0x20, 0x37, 0x36,
    0x32, 0x30, 0x38, 0x37, 0x34, 0x32, 0x33, 0x30, 0x59, 0x30, 0x13, 0x06,
    0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x25, 0xf1,
    0x23, 0xa0, 0x48, 0x28, 0x3f, 0xc5, 0x79, 0x6c, 0xcf, 0x88, 0x7d, 0x99,
    0x48, 0x9f, 0xd9, 0x35, 0xc2, 0x41, 0x98, 0xc4, 0xb5, 0xd8, 0xd5, 0xb2,
    0xc2, 0xbf, 0xd7, 0xdd, 0x5d, 0x15, 0xaf, 0xe4, 0x5b, 0x70, 0x70, 0x77,
    0x65, 0x67, 0xd5, 0xb5, 0xb0, 0xb2, 0x3e, 0x04, 0x56, 0x0b, 0x5b, 0xea,
    0x77, 0xb4, 0x83, 0xb1, 0xf6, 0x49, 0x1e, 0x53, 0xa3, 0xf2, 0xbe, 0xe6,
    0xa3, 0x9a, 0xa3, 0x81, 0x81, 0x30, 0x7f, 0x30, 0x13, 0x06, 0x0a, 0x2b,
    0x06, 0x01, 0x04, 0x01, 0x82, 0xc4, 0x0a, 0x0d, 0x01, 0x04, 0x05, 0x04,
    0x03, 0x05, 0x05, 0x06, 0x30, 0x22, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04,
    0x01, 0x82, 0xc4, 0x0a, 0x02, 0x04, 0x15, 0x31, 0x2e, 0x33, 0x2e, 0x36,
    0x2e, 0x31, 0x2e, 0x34, 0x2e, 0x31, 0x2e, 0x34, 0x31, 0x34, 0x38, 0x32,
    0x2e, 0x31, 0x2e, 0x39, 0x30, 0x13, 0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04,
    0x01, 0x82, 0xe5, 0x1c, 0x02, 0x01, 0x01, 0x04, 0x04, 0x03, 0x02, 0x05,
    0x20, 0x30, 0x21, 0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xe5,
    0x1c, 0x01, 0x01, 0x04, 0x04, 0x12, 0x04, 0x10, 0xd8, 0x52, 0x2d, 0x9f,
    0x57, 0x5b, 0x48, 0x66, 0x88, 0xa9, 0xba, 0x99, 0xfa, 0x02, 0xf3, 0x5b,
    0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x02,
    0x30, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x52, 0xb0,
    0x69, 0x49, 0xdb, 0xaa, 0xd1, 0xa6, 0x4c, 0x1b, 0xa9, 0xeb, 0xc1, 0x98,
    0xb3, 0x17, 0xec, 0x31, 0xf9, 0xa3, 0x73, 0x63, 0xba, 0x51, 0x61, 0xb3,
    0x42, 0xe3, 0xa4, 0x9c, 0xad, 0x50, 0x4f, 0x34, 0xe7, 0x42, 0x8b, 0xb8,
    0x96, 0xe9, 0xcf, 0xd2, 0x8d, 0x03, 0xad, 0x10, 0xce, 0x32, 0x5a, 0x06,
    0x83, 0x8e, 0x9b, 0x6c, 0x4e, 0xcb, 0x17, 0xad, 0x40, 0xd0, 0x90, 0xa1,
    0x6c, 0x9e, 0x7c, 0x34, 0x49, 0x83, 0x32, 0xff, 0x85, 0x3b, 0x62, 0x74,
    0x7e, 0x8f, 0xcd, 0xf0, 0x0d, 0xae, 0x62, 0x75, 0x6e, 0x57, 0xbd, 0x40,
    0xb1, 0x6d, 0x67, 0x79, 0x07, 0xa8, 0x35, 0xc0, 0x43, 0x5a, 0x2e, 0xbc,
    0xe9, 0xb0, 0xb9, 0x06, 0x9c, 0xa1, 0x22, 0xbf, 0x9d, 0x96, 0x4a, 0x73,
    0x20, 0x6a, 0xf7, 0x4f, 0xf3, 0xc0, 0x01, 0x44, 0xeb, 0xff, 0x3d, 0xe7,
    0xc7, 0x75, 0x8d, 0x31, 0x47, 0xc8, 0xc2, 0xf9, 0xfe, 0x87, 0xc1, 0x2f,
    0x2a, 0x96, 0x75, 0xa2, 0x04, 0x6b, 0x01, 0x07, 0x63, 0x61, 0xa9, 0x97,
    0x21, 0x87, 0x1f, 0xa7, 0x8f, 0xb0, 0xde, 0x29, 0x45, 0xb5, 0x79, 0xf9,
    0x16, 0x6c, 0x48, 0xad, 0x2f, 0xd5, 0x0c, 0x3c, 0xe5, 0x6c, 0x82, 0x21,
    0xa7, 0x50, 0x83, 0xf6, 0x56, 0x11, 0x93, 0x94, 0x36, 0x8f, 0xf1, 0x7d,
    0x2c, 0x92, 0x0c, 0x63, 0xa0, 0x9f, 0x01, 0xed, 0x25, 0x01, 0x14, 0x6b,
    0x7d, 0xf1, 0xab, 0x39, 0x70, 0xa2, 0xa3, 0x29, 0x38, 0xfa, 0x9a, 0x51,
    0x7a, 0xf4, 0x71, 0x08, 0x5e, 0x16, 0x0b, 0x3c, 0xa7, 0x97, 0x64, 0x23,
    0x17, 0x46, 0xba, 0x6a, 0xbb, 0xa6, 0x8e, 0x0d, 0x13, 0xce, 0x25, 0x97,
    0x96, 0xbc, 0xd2, 0xa0, 0x3a, 0xd8, 0x3c, 0x74, 0xe1, 0x53, 0x31, 0x32,
    0x8e, 0xab, 0x43, 0x8e, 0x6a, 0x41, 0x97, 0xcb, 0x12, 0xec, 0x6f, 0xd1,
    0xe3, 0x88, 0x68, 0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61, 0x58,
    0xc4, 0x26, 0xbd, 0x72, 0x78, 0xbe, 0x46, 0x37, 0x61, 0xf1, 0xfa, 0xa1,
    0xb1, 0x0a, 0xb4, 0xc4, 0xf8, 0x26, 0x70, 0x26, 0x9c, 0x41, 0x0c, 0x72,
    0x6a, 0x1f, 0xd6, 0xe0, 0x58, 0x55, 0xe1, 0x9b, 0x46, 0x45, 0x00, 0x00,
    0x00, 0x03, 0xd8, 0x52, 0x2d, 0x9f, 0x57, 0x5b, 0x48, 0x66, 0x88, 0xa9,
    0xba, 0x99, 0xfa, 0x02, 0xf3, 0x5b, 0x00, 0x40, 0x91, 0x88, 0xee, 0xf6,
    0xe9, 0x75, 0xef, 0x4e, 0x8b, 0x5b, 0x91, 0x34, 0xbf, 0x59, 0x89, 0x37,
    0xe7, 0x91, 0x60, 0x21, 0xeb, 0x61, 0x5d, 0x23, 0x83, 0xe4, 0x33, 0xe9,
    0xbc, 0x59, 0xb4, 0x7e, 0xf0, 0xae, 0xfb, 0x4d, 0xad, 0xb5, 0xde, 0x9e,
    0x0c, 0x41, 0x00, 0x5b, 0xdc, 0xc0, 0x14, 0xb2, 0x18, 0x16, 0xf1, 0xfb,
    0x8d, 0xe7, 0x67, 0x69, 0x71, 0xb3, 0x4e, 0xd3, 0x27, 0xfe, 0x7a, 0x4c,
    0xa5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0xa2, 0x68,
    0x2f, 0x95, 0x1e, 0xdf, 0x6c, 0xba, 0xe7, 0x20, 0xc9, 0x74, 0xe2, 0x3a,
    0xb5, 0xeb, 0x1a, 0x0d, 0xdf, 0xf9, 0x1a, 0xf0, 0x80, 0x41, 0x40, 0x28,
    0x8f, 0xaf, 0x34, 0x58, 0xe4, 0xc5, 0x22, 0x58, 0x20, 0x32, 0x91, 0xcc,
    0x36, 0xcb, 0xa9, 0xe7, 0xf6, 0x4b, 0xaf, 0xf9, 0xbc, 0x84, 0x1d, 0x1a,
    0x66, 0xc8, 0x01, 0x1c, 0x05, 0x42, 0x31, 0x3a, 0x26, 0x3a, 0x5d, 0x2a,
    0x12, 0xd6, 0x6d, 0x26, 0xf4,
};

static const uint8_t kCredentialID[] = {
    0x91, 0x88, 0xEE, 0xF6, 0xE9, 0x75, 0xEF, 0x4E, 0x8B, 0x5B, 0x91,
    0x34, 0xBF, 0x59, 0x89, 0x37, 0xE7, 0x91, 0x60, 0x21, 0xEB, 0x61,
    0x5D, 0x23, 0x83, 0xE4, 0x33, 0xE9, 0xBC, 0x59, 0xB4, 0x7E, 0xF0,
    0xAE, 0xFB, 0x4D, 0xAD, 0xB5, 0xDE, 0x9E, 0x0C, 0x41, 0x00, 0x5B,
    0xDC, 0xC0, 0x14, 0xB2, 0x18, 0x16, 0xF1, 0xFB, 0x8D, 0xE7, 0x67,
    0x69, 0x71, 0xB3, 0x4E, 0xD3, 0x27, 0xFE, 0x7A, 0x4C,
};

class iCloudKeychainTest : public testing::Test, FidoDiscoveryBase::Observer {
 public:
  void SetUp() override {
    if (@available(macOS 13.5, *)) {
      fake_ = base::MakeRefCounted<FakeSystemInterface>();
      SetSystemInterfaceForTesting(fake_);

      discovery_ = NewDiscovery(kFakeNSWindowForTesting);
      discovery_->set_observer(this);
      discovery_->Start();
      task_environment_.RunUntilIdle();
      CHECK(authenticator_);
    }
  }

  void TearDown() override {
    if (@available(macOS 13.5, *)) {
      SetSystemInterfaceForTesting(nullptr);
    }
  }

  // FidoDiscoveryBase::Observer:
  void DiscoveryStarted(
      FidoDiscoveryBase* discovery,
      bool success,
      std::vector<FidoAuthenticator*> authenticators) override {
    CHECK(success);
    CHECK_EQ(authenticators.size(), 1u);
    CHECK(!authenticator_);
    authenticator_ = authenticators[0];
  }

  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override {
    NOTREACHED();
  }

  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override {
    NOTREACHED();
  }

 protected:
  API_AVAILABLE(macos(13.3))
  scoped_refptr<FakeSystemInterface> fake_;
  std::unique_ptr<FidoDiscoveryBase> discovery_;
  raw_ptr<FidoAuthenticator> authenticator_ = nullptr;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(iCloudKeychainTest, RequestAuthorization) {
  if (@available(macOS 13.5, *)) {
    PublicKeyCredentialParams public_key_params(
        {PublicKeyCredentialParams::CredentialInfo()});
    CtapMakeCredentialRequest make_credential_request(
        "{}", {{1, 2, 3, 4}, "rp.id"}, {{4, 3, 2, 1}, "name", "displayName"},
        std::move(public_key_params));
    MakeCredentialOptions make_credential_options;

    CtapGetAssertionRequest get_assertion_request("rp.id", "{}");
    CtapGetAssertionOptions get_assertion_options;

    for (const auto auth_state :
         {SystemInterface::kAuthNotAuthorized, SystemInterface::kAuthDenied,
          SystemInterface::kAuthAuthorized}) {
      for (const bool is_make_credential : {true, false}) {
        SCOPED_TRACE(auth_state);
        SCOPED_TRACE(is_make_credential);

        fake_->set_auth_state(auth_state);
        if (auth_state == SystemInterface::kAuthNotAuthorized) {
          fake_->set_next_auth_state(SystemInterface::kAuthAuthorized);
        }

        if (is_make_credential) {
          base::test::TestFuture<
              MakeCredentialStatus,
              std::optional<AuthenticatorMakeCredentialResponse>>
              future;
          authenticator_->MakeCredential(make_credential_request,
                                         make_credential_options,
                                         future.GetCallback());
          EXPECT_TRUE(future.Wait());
        } else {
          base::test::TestFuture<GetAssertionStatus,
                                 std::vector<AuthenticatorGetAssertionResponse>>
              future;
          authenticator_->GetAssertion(get_assertion_request,
                                       get_assertion_options,
                                       future.GetCallback());
          EXPECT_TRUE(future.Wait());
        }

        // If the auth state was SystemInterface::kAuthNotAuthorized then
        // authorisation should have been requested.
        EXPECT_EQ(fake_->GetAuthState(),
                  auth_state == SystemInterface::kAuthNotAuthorized
                      ? SystemInterface::kAuthAuthorized
                      : auth_state);
      }
    }
  }
}

TEST_F(iCloudKeychainTest, MakeCredential) {
  if (@available(macOS 13.5, *)) {
    PublicKeyCredentialParams public_key_params(
        {PublicKeyCredentialParams::CredentialInfo()});
    CtapMakeCredentialRequest request("{}", {{1, 2, 3, 4}, "rp.id"},
                                      {{4, 3, 2, 1}, "name", "displayName"},
                                      std::move(public_key_params));
    MakeCredentialOptions options;

    auto make_credential = [this, &request, &options]()
        -> std::tuple<MakeCredentialStatus,
                      std::optional<AuthenticatorMakeCredentialResponse>> {
      base::test::TestFuture<MakeCredentialStatus,
                             std::optional<AuthenticatorMakeCredentialResponse>>
          future;
      authenticator_->MakeCredential(request, options, future.GetCallback());
      EXPECT_TRUE(future.Wait());
      return future.Take();
    };

    {
      // Without `SetMakeCredentialResult` being called, an error is returned.
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result), MakeCredentialStatus::kUserConsentDenied);
      EXPECT_FALSE(std::get<1>(result).has_value());
    }

    {
      fake_->SetMakeCredentialError(8 /* exclude list match */);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result),
                MakeCredentialStatus::kUserConsentButCredentialExcluded);
      EXPECT_FALSE(std::get<1>(result).has_value());
    }

    {
      // This is a little odd because we call `Cancel` before `MakeCredential`
      // rather than during it, but our fake doesn't support blocking
      // operations.
      fake_->SetMakeCredentialError(1001 /* generic error */);
      EXPECT_EQ(fake_->cancel_count(), 0u);
      authenticator_->Cancel();
      EXPECT_EQ(fake_->cancel_count(), 1u);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result),
                MakeCredentialStatus::kAuthenticatorResponseInvalid);
      EXPECT_FALSE(std::get<1>(result).has_value());
    }

    {
      static const uint8_t kWrongCredentialID[] = {1, 2, 3, 4};
      fake_->SetMakeCredentialResult(kAttestationObjectBytes,
                                     kWrongCredentialID);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result),
                MakeCredentialStatus::kAuthenticatorResponseInvalid);
      ASSERT_FALSE(std::get<1>(result).has_value());
    }

    {
      static const uint8_t kInvalidCBOR[] = {1, 2, 3, 4};
      fake_->SetMakeCredentialResult(kInvalidCBOR, kCredentialID);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result),
                MakeCredentialStatus::kAuthenticatorResponseInvalid);
      ASSERT_FALSE(std::get<1>(result).has_value());
    }

    {
      // kInvalidAttestationStatement is an empty CBOR map, which is valid CBOR
      // but invalid at a higher level.
      static const uint8_t kInvalidAttestationStatement[] = {0xa0};
      fake_->SetMakeCredentialResult(kInvalidAttestationStatement,
                                     kCredentialID);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result),
                MakeCredentialStatus::kAuthenticatorResponseInvalid);
      ASSERT_FALSE(std::get<1>(result).has_value());
    }

    {
      fake_->SetMakeCredentialResult(kAttestationObjectBytes, kCredentialID);
      auto result = make_credential();
      EXPECT_EQ(std::get<0>(result), MakeCredentialStatus::kSuccess);
      ASSERT_TRUE(std::get<1>(result).has_value());
      const AuthenticatorMakeCredentialResponse response =
          std::move(*std::get<1>(result));

      const std::vector<uint8_t> returned_credential_id =
          response.attestation_object.authenticator_data().GetCredentialId();
      EXPECT_TRUE(base::ranges::equal(returned_credential_id, kCredentialID));
      EXPECT_FALSE(response.enterprise_attestation_returned);
      EXPECT_TRUE(response.is_resident_key.value_or(false));
      EXPECT_FALSE(response.enterprise_attestation_returned);
      EXPECT_EQ(response.transports->size(), 2u);
      EXPECT_TRUE(
          base::Contains(*response.transports, FidoTransportProtocol::kHybrid));
      EXPECT_TRUE(base::Contains(*response.transports,
                                 FidoTransportProtocol::kInternal));
      EXPECT_EQ(response.transport_used, FidoTransportProtocol::kInternal);
    }

    {
      std::unique_ptr<base::StatisticsRecorder> stats_recorder =
          base::StatisticsRecorder::CreateTemporaryForTesting();

      fake_->set_auth_state(FakeSystemInterface::kAuthNotAuthorized);
      fake_->set_next_auth_state(FakeSystemInterface::kAuthDenied);
      fake_->SetMakeCredentialResult(kAttestationObjectBytes, kCredentialID);
      make_credential();

      base::HistogramBase* histogram =
          base::StatisticsRecorder::FindHistogram(kMetricName);
      std::unique_ptr<base::HistogramSamples> samples(
          histogram->SnapshotSamples());
      EXPECT_EQ(samples->GetCount(0), 1);
      EXPECT_EQ(samples->GetCount(1), 0);
      EXPECT_EQ(samples->GetCount(2), 1);
    }

    {
      std::unique_ptr<base::StatisticsRecorder> stats_recorder =
          base::StatisticsRecorder::CreateTemporaryForTesting();

      fake_->set_auth_state(FakeSystemInterface::kAuthNotAuthorized);
      fake_->set_next_auth_state(FakeSystemInterface::kAuthAuthorized);
      fake_->SetMakeCredentialResult(kAttestationObjectBytes, kCredentialID);
      make_credential();

      base::HistogramBase* histogram =
          base::StatisticsRecorder::FindHistogram(kMetricName);
      std::unique_ptr<base::HistogramSamples> samples(
          histogram->SnapshotSamples());
      EXPECT_EQ(samples->GetCount(0), 1);
      EXPECT_EQ(samples->GetCount(1), 1);
      EXPECT_EQ(samples->GetCount(2), 0);
    }
  }
}

TEST_F(iCloudKeychainTest, GetAssertion) {
  static const uint8_t kAuthenticatorData[] = {
      0x26, 0xbd, 0x72, 0x78, 0xbe, 0x46, 0x37, 0x61, 0xf1, 0xfa,
      0xa1, 0xb1, 0x0a, 0xb4, 0xc4, 0xf8, 0x26, 0x70, 0x26, 0x9c,
      0x41, 0x0c, 0x72, 0x6a, 0x1f, 0xd6, 0xe0, 0x58, 0x55, 0xe1,
      0x9b, 0x46, 0x01, 0x00, 0x00, 0x0f, 0xdd,
  };
  static const uint8_t kSignature[] = {1, 2, 3, 4};
  static const uint8_t kUserID[] = {5, 6, 7, 8};

  if (@available(macOS 13.5, *)) {
    CtapGetAssertionRequest request("rp.id", "{}");
    CtapGetAssertionOptions options;

    auto get_assertion = [this, &request, &options]()
        -> std::tuple<GetAssertionStatus,
                      std::vector<AuthenticatorGetAssertionResponse>> {
      base::test::TestFuture<GetAssertionStatus,
                             std::vector<AuthenticatorGetAssertionResponse>>
          future;
      authenticator_->GetAssertion(request, options, future.GetCallback());
      EXPECT_TRUE(future.Wait());
      return future.Take();
    };

    {
      // Without `SetGetAssertionResult` being called, an error is returned.
      auto result = get_assertion();
      EXPECT_EQ(std::get<0>(result), GetAssertionStatus::kUserConsentDenied);
      EXPECT_TRUE(std::get<1>(result).empty());
    }

    {
      // This is a little odd because we call `Cancel` before `GetAssertion`
      // rather than during it, but our fake doesn't support blocking
      // operations.
      fake_->SetMakeCredentialError(1001 /* generic error */);
      EXPECT_EQ(fake_->cancel_count(), 0u);
      authenticator_->Cancel();
      EXPECT_EQ(fake_->cancel_count(), 1u);
      auto result = get_assertion();
      EXPECT_EQ(std::get<0>(result),
                GetAssertionStatus::kAuthenticatorResponseInvalid);
      EXPECT_TRUE(std::get<1>(result).empty());
    }

    {
      static const uint8_t kInvalidAuthenticatorData[] = {1, 2, 3, 4};
      fake_->SetGetAssertionResult(kInvalidAuthenticatorData, kSignature,
                                   kUserID, kCredentialID);
      auto result = get_assertion();
      EXPECT_EQ(std::get<0>(result),
                GetAssertionStatus::kAuthenticatorResponseInvalid);
      EXPECT_TRUE(std::get<1>(result).empty());
    }

    {
      fake_->SetGetAssertionResult(kAuthenticatorData, kSignature, kUserID,
                                   kCredentialID);
      auto result = get_assertion();
      EXPECT_EQ(std::get<0>(result), GetAssertionStatus::kSuccess);
      EXPECT_EQ(std::get<1>(result).size(), 1u);

      AuthenticatorGetAssertionResponse response =
          std::move(std::get<1>(result)[0]);
      EXPECT_TRUE(base::ranges::equal(response.signature, kSignature));
      EXPECT_TRUE(base::ranges::equal(response.user_entity->id, kUserID));
      EXPECT_TRUE(base::ranges::equal(response.credential->id, kCredentialID));
      EXPECT_TRUE(response.user_selected);
      EXPECT_EQ(response.transport_used, FidoTransportProtocol::kInternal);
    }

    {
      fake_->SetGetAssertionError(1001,
                                  "... No credentials available for login ...");
      auto result = get_assertion();
      EXPECT_EQ(std::get<0>(result),
                GetAssertionStatus::kICloudKeychainNoCredentials);
    }

    {
      std::unique_ptr<base::StatisticsRecorder> stats_recorder =
          base::StatisticsRecorder::CreateTemporaryForTesting();

      fake_->set_auth_state(FakeSystemInterface::kAuthNotAuthorized);
      fake_->set_next_auth_state(FakeSystemInterface::kAuthDenied);
      fake_->SetGetAssertionResult(kAuthenticatorData, kSignature, kUserID,
                                   kCredentialID);
      get_assertion();

      base::HistogramBase* histogram =
          base::StatisticsRecorder::FindHistogram(kMetricName);
      std::unique_ptr<base::HistogramSamples> samples(
          histogram->SnapshotSamples());
      EXPECT_EQ(samples->GetCount(3), 1);
      EXPECT_EQ(samples->GetCount(4), 0);
      EXPECT_EQ(samples->GetCount(5), 1);
    }

    {
      std::unique_ptr<base::StatisticsRecorder> stats_recorder =
          base::StatisticsRecorder::CreateTemporaryForTesting();

      fake_->set_auth_state(FakeSystemInterface::kAuthNotAuthorized);
      fake_->set_next_auth_state(FakeSystemInterface::kAuthAuthorized);
      fake_->SetGetAssertionResult(kAuthenticatorData, kSignature, kUserID,
                                   kCredentialID);
      get_assertion();

      base::HistogramBase* histogram =
          base::StatisticsRecorder::FindHistogram(kMetricName);
      std::unique_ptr<base::HistogramSamples> samples(
          histogram->SnapshotSamples());
      EXPECT_EQ(samples->GetCount(3), 1);
      EXPECT_EQ(samples->GetCount(4), 1);
      EXPECT_EQ(samples->GetCount(5), 0);
    }
  }
}

// Gardener 2024-06-18: Disabled due to asan failures (crbug.com/347287026).
TEST_F(iCloudKeychainTest, DISABLED_FetchCredentialMetadata) {
  if (@available(macOS 13.5, *)) {
    const std::vector<DiscoverableCredentialMetadata> creds = {
        {AuthenticatorType::kICloudKeychain,
         "example.com",
         {1, 2, 3, 4},
         {{4, 3, 2, 1}, "name", std::nullopt}}};
    fake_->SetCredentials(creds);
    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>,
                           FidoRequestHandlerBase::RecognizedCredential>
        future;
    CtapGetAssertionRequest request("example.com", "{}");
    CtapGetAssertionOptions options;

    CHECK(authenticator_);
    authenticator_->GetPlatformCredentialInfoForRequest(request, options,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto result = future.Take();
    std::vector<DiscoverableCredentialMetadata> creds_out =
        std::move(std::get<0>(result));

    ASSERT_EQ(creds_out.size(), 1u);
    EXPECT_EQ(creds[0], creds_out[0]);
  }
}

// Gardener 2024-06-18: Disabled due to asan failures (crbug.com/347287026).
TEST_F(iCloudKeychainTest, DISABLED_FetchCredentialMetadataWithAllowlist) {
  if (@available(macOS 13.5, *)) {
    const std::vector<DiscoverableCredentialMetadata> creds = {
        {AuthenticatorType::kICloudKeychain,
         "example.com",
         {1, 2, 3, 4},
         {{4, 3, 2, 1}, "name", std::nullopt}},
        {AuthenticatorType::kICloudKeychain,
         "example.com",
         {1, 2, 3, 5},
         {{4, 3, 2, 2}, "name", std::nullopt}},
    };
    fake_->SetCredentials(creds);
    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>,
                           FidoRequestHandlerBase::RecognizedCredential>
        future;
    CtapGetAssertionRequest request("example.com", "{}");
    request.allow_list = {{CredentialType::kPublicKey, {1, 2, 3, 4}}};
    CtapGetAssertionOptions options;

    CHECK(authenticator_);
    authenticator_->GetPlatformCredentialInfoForRequest(request, options,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto result = future.Take();
    std::vector<DiscoverableCredentialMetadata> creds_out =
        std::move(std::get<0>(result));

    // The second credential should have been filtered out by the allow list.
    ASSERT_EQ(creds_out.size(), 1u);
    EXPECT_EQ(creds[0], creds_out[0]);
  }
}

TEST_F(iCloudKeychainTest, FetchCredentialMetadataNoPermission) {
  if (@available(macOS 13.5, *)) {
    fake_->set_auth_state(FakeSystemInterface::kAuthNotAuthorized);

    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>,
                           FidoRequestHandlerBase::RecognizedCredential>
        future;
    CtapGetAssertionRequest request("example.com", "{}");
    CtapGetAssertionOptions options;

    CHECK(authenticator_);
    authenticator_->GetPlatformCredentialInfoForRequest(request, options,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto result = future.Take();
    EXPECT_EQ(std::get<1>(result),
              FidoRequestHandlerBase::RecognizedCredential::kUnknown);
  }
}

}  // namespace

}  // namespace device::fido::icloud_keychain
