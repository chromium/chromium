// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/value_response_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

// clang-format off
constexpr uint8_t kTestAuthenticatorData[] = {
    // sha256 hash of rp id.
    0x11, 0x94, 0x22, 0x8D, 0xA8, 0xFD, 0xBD, 0xEE, 0xFD, 0x26, 0x1B, 0xD7,
    0xB6, 0x59, 0x5C, 0xFD, 0x70, 0xA5, 0x0D, 0x70, 0xC6, 0x40, 0x7B, 0xCF,
    0x01, 0x3D, 0xE9, 0x6D, 0x4E, 0xFB, 0x17, 0xDE,
    // flags
    0x01,
    // counter
    0x00, 0x00, 0x00, 0x00};
// clang-format on

std::vector<uint8_t> ToByteVector(const std::string& in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

// Convert JSON to an AuthenticatorGetAssertionResponse successfully with only
// required fields.
TEST(ValueResponseConversionTest,
     AuthenticatorGetAssertionResponseFromValueSuccess) {
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  const std::vector<uint8_t> authenticator_data(
      kTestAuthenticatorData,
      kTestAuthenticatorData + sizeof(kTestAuthenticatorData));

  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().Set("authenticatorData", base::Value(authenticator_data));
  dict.GetDict().Set("signature", base::Value(kSignature));

  std::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(dict);
  ASSERT_TRUE(response);

  EXPECT_EQ(response->signature, kSignature);
  EXPECT_EQ(response->authenticator_data.SerializeToByteArray(),
            authenticator_data);
}

// Convert JSON to an AuthenticatorGetAssertionResponse successfully with all
// required and optional fields.
TEST(ValueResponseConversionTest,
     AuthenticatorGetAssertionResponseFromValueSuccessAllFields) {
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  static const std::vector<uint8_t> kUserId = ToByteVector("userid1");
  const std::vector<uint8_t> authenticator_data(
      kTestAuthenticatorData,
      kTestAuthenticatorData + sizeof(kTestAuthenticatorData));

  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().Set("authenticatorData", base::Value(authenticator_data));
  dict.GetDict().Set("signature", base::Value(kSignature));
  dict.GetDict().Set("userHandle", base::Value(kUserId));

  std::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(dict);
  ASSERT_TRUE(response);

  EXPECT_EQ(response->signature, kSignature);
  EXPECT_EQ(response->authenticator_data.SerializeToByteArray(),
            authenticator_data);
  EXPECT_TRUE(response->user_entity);
  EXPECT_EQ(response->user_entity->id, kUserId);
}

// Test converting assertion response JSON with missing required fields.
TEST(ValueResponseConversionTest,
     AuthenticatorGetAssertionResponseFromValueFailure) {
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  const std::vector<uint8_t> authenticator_data(
      kTestAuthenticatorData,
      kTestAuthenticatorData + sizeof(kTestAuthenticatorData));
  {
    base::Value dict(base::Value::Type::DICT);
    dict.GetDict().Set("signature", base::Value(kSignature));

    std::optional<AuthenticatorGetAssertionResponse> response =
        AuthenticatorGetAssertionResponseFromValue(dict);
    ASSERT_FALSE(response)
        << "Parsing incorrectly succeeded with no authenticatorData.";
  }

  {
    base::Value dict(base::Value::Type::DICT);
    dict.GetDict().Set("authenticatorData", base::Value(authenticator_data));

    std::optional<AuthenticatorGetAssertionResponse> response =
        AuthenticatorGetAssertionResponseFromValue(dict);
    ASSERT_FALSE(response)
        << "Parsing incorrectly succeeded with no signature.";
  }
}

}  // namespace
}  // namespace device
