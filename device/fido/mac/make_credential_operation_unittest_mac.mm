// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/make_credential_operation.h"

#include <array>

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include "base/strings/string_number_conversions.h"

#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace fido {
namespace mac {
namespace {

using test::TestCallbackReceiver;

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
TEST(MakeCredentialOperationTest, DISABLED_TestRun)
API_AVAILABLE(macosx(10.12.2)) {
  base::test::TaskEnvironment task_environment;
  TestCallbackReceiver<CtapDeviceResponseCode,
                       base::Optional<AuthenticatorMakeCredentialResponse>>
      callback_receiver;
  auto request = MakeTestRequest();
  MakeCredentialOperation op(request, "test-profile", kKeychainAccessGroup,
                             callback_receiver.callback());

  op.Run();
  callback_receiver.WaitForCallback();
  auto result = callback_receiver.TakeResult();
  CtapDeviceResponseCode error = std::get<0>(result);
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess, error);
  auto opt_response = std::move(std::get<1>(result));
  ASSERT_TRUE(opt_response);
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
