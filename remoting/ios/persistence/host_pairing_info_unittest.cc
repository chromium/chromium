// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/persistence/host_pairing_info.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "remoting/ios/persistence/mock_keychain.h"

using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace remoting {

namespace {

const char kFakeUserId[] = "fake_user_id_1";

std::unique_ptr<base::Value> DecodeJson(const std::string& json) {
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      JSONStringValueDeserializer(json).Deserialize(&error_code,
                                                    &error_message);
  EXPECT_EQ(0, error_code);
  EXPECT_EQ("", error_message);
  return value;
}

}  // namespace

class HostPairingInfoTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  HostPairingInfo MockKeychainAndGetPairing(
      const std::string& mock_keychain_output,
      const std::string& host_id);
  void VerifyHostPairingInfo(const HostPairingInfo& pairing_info,
                             const std::string& expected_host_id,
                             const std::string& expected_pairing_id,
                             const std::string& expected_pairing_secret);
  MockKeychain mock_keychain_;
};

void HostPairingInfoTest::SetUp() {
  HostPairingInfo::SetKeychainForTesting(&mock_keychain_);
}

void HostPairingInfoTest::TearDown() {
  HostPairingInfo::SetKeychainForTesting(nullptr);
}

HostPairingInfo HostPairingInfoTest::MockKeychainAndGetPairing(
    const std::string& mock_keychain_output,
    const std::string& host_id) {
  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        kFakeUserId, mock_keychain_output);
  return HostPairingInfo::GetPairingInfo(kFakeUserId, host_id);
}

void HostPairingInfoTest::VerifyHostPairingInfo(
    const HostPairingInfo& pairing_info,
    const std::string& expected_host_id,
    const std::string& expected_pairing_id,
    const std::string& expected_pairing_secret) {
  EXPECT_EQ(kFakeUserId, pairing_info.user_id());
  EXPECT_EQ(expected_host_id, pairing_info.host_id());
  EXPECT_EQ(expected_pairing_id, pairing_info.pairing_id());
  EXPECT_EQ(expected_pairing_secret, pairing_info.pairing_secret());
}

#pragma mark - Tests

TEST_F(HostPairingInfoTest, RetrieveHostFromNewUser_returnBlankPairingInfo) {
  HostPairingInfo result = MockKeychainAndGetPairing("", "fake_host_1");
  VerifyHostPairingInfo(result, "fake_host_1", "", "");
}

TEST_F(HostPairingInfoTest, RetrieveNonexistentHost_returnBlankPairingInfo) {
  std::string keychain_result = R"({
    "fake_host_2": {"id": "id_2", "secret": "secret_2"}
  })";
  HostPairingInfo result =
      MockKeychainAndGetPairing(keychain_result, "fake_host_1");
  VerifyHostPairingInfo(result, "fake_host_1", "", "");
}

TEST_F(HostPairingInfoTest,
       RetrieveInfoFromUserWithMultipleHosts_returnTheRightPairingInfo) {
  std::string keychain_result = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"},
    "fake_host_2": {"id": "id_2", "secret": "secret_2"}
  })";
  HostPairingInfo result =
      MockKeychainAndGetPairing(keychain_result, "fake_host_2");
  VerifyHostPairingInfo(result, "fake_host_2", "id_2", "secret_2");
}

TEST_F(HostPairingInfoTest,
       SaveInfoForNewUser_writesFullPairingListToKeychain) {
  HostPairingInfo result = MockKeychainAndGetPairing("", "fake_host_1");
  result.set_pairing_id("id_1");
  result.set_pairing_secret("secret_1");

  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        kFakeUserId, "");

  std::string actual_json;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         kFakeUserId, &actual_json);
  result.Save();

  std::string expected_json = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"}
  })";
  EXPECT_EQ(*DecodeJson(expected_json), *DecodeJson(actual_json));
}

TEST_F(HostPairingInfoTest, SaveNewInfoForExistingUser_addsNewHostPairing) {
  std::string keychain_result = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"}
  })";

  HostPairingInfo result =
      MockKeychainAndGetPairing(keychain_result, "fake_host_2");
  result.set_pairing_id("id_2");
  result.set_pairing_secret("secret_2");

  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        kFakeUserId, keychain_result);

  std::string actual_json;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         kFakeUserId, &actual_json);
  result.Save();

  std::string expected_json = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"},
    "fake_host_2": {"id": "id_2", "secret": "secret_2"}
  })";
  EXPECT_EQ(*DecodeJson(expected_json), *DecodeJson(actual_json));
}

TEST_F(HostPairingInfoTest, SaveInfoForExistingHost_updatesExistingHostInfo) {
  std::string keychain_result = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"},
    "fake_host_2": {"id": "id_2", "secret": "secret_2"}
  })";

  HostPairingInfo result =
      MockKeychainAndGetPairing(keychain_result, "fake_host_2");
  result.set_pairing_id("id_3");
  result.set_pairing_secret("secret_3");

  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        kFakeUserId, keychain_result);

  std::string actual_json;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         kFakeUserId, &actual_json);
  result.Save();

  std::string expected_json = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"},
    "fake_host_2": {"id": "id_3", "secret": "secret_3"}
  })";
  EXPECT_EQ(*DecodeJson(expected_json), *DecodeJson(actual_json));
}

TEST_F(HostPairingInfoTest,
       LoadCorruptedDataThenSave_returnsBlankDataAndProperlySaved) {
  std::string mock_data = R"({
    "fake_host_1": "corrupted"
  })";
  HostPairingInfo result = MockKeychainAndGetPairing(mock_data, "fake_host_1");
  VerifyHostPairingInfo(result, "fake_host_1", "", "");
  result.set_pairing_id("id_1");
  result.set_pairing_secret("secret_1");

  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        kFakeUserId, "");

  std::string actual_json;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         kFakeUserId, &actual_json);
  result.Save();

  std::string expected_json = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"}
  })";
  EXPECT_EQ(*DecodeJson(expected_json), *DecodeJson(actual_json));
}

TEST_F(HostPairingInfoTest,
       LoadTwoPairingsFromTwoUsersThenSave_noInterference) {
  std::string mock_data_1 = R"({
    "fake_host_1": {"id": "id_1", "secret": "secret_1"}
  })";
  std::string mock_data_2 = R"({
    "fake_host_2": {"id": "id_2", "secret": "secret_2"}
  })";

  // Edit pairing of the first user's host.
  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        "user_id_1", mock_data_1);
  HostPairingInfo pairing_1 =
      HostPairingInfo::GetPairingInfo("user_id_1", "fake_host_1");
  pairing_1.set_pairing_id("id_3");
  pairing_1.set_pairing_secret("secret_3");

  // Add new pairing for the second user.
  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        "user_id_2", mock_data_2);
  HostPairingInfo pairing_2 =
      HostPairingInfo::GetPairingInfo("user_id_2", "fake_host_3");
  pairing_2.set_pairing_id("id_4");
  pairing_2.set_pairing_secret("secret_4");

  // Save first pairing.
  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        "user_id_1", mock_data_1);
  std::string expect_json_1 = R"({
    "fake_host_1": {"id": "id_3", "secret": "secret_3"}
  })";
  std::string actual_json_1;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         "user_id_1", &actual_json_1);
  pairing_1.Save();
  EXPECT_EQ(*DecodeJson(expect_json_1), *DecodeJson(actual_json_1));

  // Save Second pairing.
  mock_keychain_.ExpectGetDataAndReturn(Keychain::Key::PAIRING_INFO,
                                        "user_id_2", mock_data_2);
  std::string expect_json_2 = R"({
    "fake_host_2": {"id": "id_2", "secret": "secret_2"},
    "fake_host_3": {"id": "id_4", "secret": "secret_4"}
  })";
  std::string actual_json_2;
  mock_keychain_.ExpectAndCaptureSetData(Keychain::Key::PAIRING_INFO,
                                         "user_id_2", &actual_json_2);
  pairing_2.Save();
  EXPECT_EQ(*DecodeJson(expect_json_2), *DecodeJson(actual_json_2));
}

}  // namespace remoting
