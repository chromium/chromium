// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_string_value_serializer.h"
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
  constexpr char kJson[] = R"({
      "authenticatorData": "EZQijaj9ve79JhvXtllc_XClDXDGQHvPAT3pbU77F94BAAAAAA",
      "signature": "dGVzdCBzaWduYXR1cmU"
      })";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  absl::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(*value);
  ASSERT_TRUE(response);

  EXPECT_EQ(response->signature, kSignature);
  EXPECT_EQ(response->authenticator_data.SerializeToByteArray(),
            std::vector<uint8_t>(
                kTestAuthenticatorData,
                kTestAuthenticatorData + sizeof(kTestAuthenticatorData)));
}

// Convert JSON to an AuthenticatorGetAssertionResponse successfully with all
// required and optional fields.
TEST(ValueResponseConversionTest,
     AuthenticatorGetAssertionResponseFromValueSuccessAllFields) {
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  static const std::vector<uint8_t> kUserId = ToByteVector("userid1");
  constexpr char kJson[] = R"({
      "authenticatorData": "EZQijaj9ve79JhvXtllc_XClDXDGQHvPAT3pbU77F94BAAAAAA",
      "signature": "dGVzdCBzaWduYXR1cmU",
      "userHandle": "dXNlcmlkMQ"
      })";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  absl::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(*value);
  ASSERT_TRUE(response);

  EXPECT_EQ(response->signature, kSignature);
  EXPECT_EQ(response->authenticator_data.SerializeToByteArray(),
            std::vector<uint8_t>(
                kTestAuthenticatorData,
                kTestAuthenticatorData + sizeof(kTestAuthenticatorData)));
  EXPECT_TRUE(response->user_entity);
  EXPECT_EQ(response->user_entity->id, kUserId);
}

// Test converting assertion response JSON with missing required fields.
TEST(ValueResponseConversionTest,
     AuthenticatorGetAssertionResponseFromValueFailure) {
  constexpr char kJsonNoAuthenticatorData[] = R"({
      "signature": "dGVzdCBzaWduYXR1cmU"
      })";
  constexpr char kJsonSignature[] = R"({
      "authenticatorData": "EZQijaj9ve79JhvXtllc_XClDXDGQHvPAT3pbU77F94BAAAAAA"
      })";

  {
    JSONStringValueDeserializer deserializer(kJsonNoAuthenticatorData);
    std::string deserialize_error;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
    ASSERT_TRUE(value) << deserialize_error;

    absl::optional<AuthenticatorGetAssertionResponse> response =
        AuthenticatorGetAssertionResponseFromValue(*value);
    ASSERT_FALSE(response)
        << "Parsing incorrectly succeeded with no authenticatorData.";
  }

  {
    JSONStringValueDeserializer deserializer(kJsonSignature);
    std::string deserialize_error;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
    ASSERT_TRUE(value) << deserialize_error;

    absl::optional<AuthenticatorGetAssertionResponse> response =
        AuthenticatorGetAssertionResponseFromValue(*value);
    ASSERT_FALSE(response)
        << "Parsing incorrectly succeeded with no signature.";
  }
}

// Convert JSON to an AuthenticatorMakeCredentialResponse successfully.
TEST(ValueResponseConversionTest,
     AuthenticatorMakeCredentialResponseFromValueSuccess) {
  constexpr char kJson[] = R"({
      "authenticatorData": "EZQijaj9ve79JhvXtllc_XClDXDGQHvPAT3pbU77F94BAAAAAA"
      })";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  absl::optional<AuthenticatorMakeCredentialResponse> response =
      AuthenticatorMakeCredentialResponseFromValue(*value);
  ASSERT_TRUE(response);

  EXPECT_EQ(
      response->attestation_object.authenticator_data().SerializeToByteArray(),
      std::vector<uint8_t>(
          kTestAuthenticatorData,
          kTestAuthenticatorData + sizeof(kTestAuthenticatorData)));
}

// Test converting registration response JSON with missing required fields.
TEST(ValueResponseConversionTest,
     AuthenticatorMakeCredentialResponseFromValueFailure) {
  constexpr char kJsonNoAuthenticatorData[] = R"({
      "clientDataJSON": "test clientDataJSON"
    })";

  JSONStringValueDeserializer deserializer(kJsonNoAuthenticatorData);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  absl::optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponseFromValue(*value);
  ASSERT_FALSE(response)
      << "Parsing incorrectly succeeded with no authenticatorData.";
}

}  // namespace
}  // namespace device
