// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/make_credential_operation.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::fido::mac {
namespace {

using base::test::TestFuture;

const std::string kRpId = "rp.example.com";
const std::vector<uint8_t> kUserId = {10, 11, 12, 13, 14, 15};
const char kKeychainAccessGroup[] =
    "EQHXZ8M8AV.com.google.chrome.webauthn.test";

CtapMakeCredentialRequest MakeTestRequest() {
  return CtapMakeCredentialRequest(
      test_data::kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(kUserId),
      PublicKeyCredentialParams(
          {{PublicKeyCredentialParams::
                CredentialInfo() /* defaults to ES-256 */}}));
}

// For demo purposes only. This test does a Touch ID user prompt. It will fail
// on incompatible hardware and crash if not code signed or lacking the
// keychain-access-group entitlement.
TEST(MakeCredentialOperationTest, DISABLED_TestRun) {
  base::test::TaskEnvironment task_environment;
  TestFuture<MakeCredentialStatus,
             std::optional<AuthenticatorMakeCredentialResponse>>
      future;
  auto request = MakeTestRequest();
  TouchIdCredentialStore credential_store(
      AuthenticatorConfig{"test-profile", kKeychainAccessGroup});
  MakeCredentialOperation op(request, &credential_store, future.GetCallback());

  op.Run();
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future.Get()));
  ASSERT_TRUE(std::get<1>(future.Get()));
}

}  // namespace
}  // namespace device::fido::mac
