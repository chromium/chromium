// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/signal_api_utils.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webauthn {
namespace {

const char kRpId[] = "example.com";
const std::vector<uint8_t> kCredentialId = {1, 2, 3};
const std::vector<uint8_t> kUserId = {4, 5, 6};
const char kName[] = "username";
const char kDisplayName[] = "User Name";

sync_pb::WebauthnCredentialSpecifics CreatePasskey() {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_rp_id(kRpId);
  specifics.set_credential_id(kCredentialId.data(), kCredentialId.size());
  specifics.set_user_id(kUserId.data(), kUserId.size());
  specifics.set_user_name(kName);
  specifics.set_user_display_name(kDisplayName);
  return specifics;
}

class SignalApiUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    PasskeyChangeQuotaTracker::GetInstance()->ResetForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SignalApiUtilsTest, SignalUnknownCredential_Success) {
  TestPasskeyModel passkey_model;
  passkey_model.AddNewPasskeyForTesting(CreatePasskey());
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_TRUE(UpdatePasskeyModelForSignalUnknownCredential(
      origin, kRpId, kCredentialId, passkey_model));

  std::optional<sync_pb::WebauthnCredentialSpecifics> passkey =
      passkey_model.GetPasskey(
          kRpId, std::string(kCredentialId.begin(), kCredentialId.end()),
          PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_TRUE(passkey.has_value());
  EXPECT_TRUE(passkey->hidden());
}

TEST_F(SignalApiUtilsTest, SignalUnknownCredential_NotFound) {
  TestPasskeyModel passkey_model;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_FALSE(UpdatePasskeyModelForSignalUnknownCredential(
      origin, kRpId, kCredentialId, passkey_model));
}

TEST_F(SignalApiUtilsTest, SignalAllAcceptedCredentials_Hide) {
  TestPasskeyModel passkey_model;
  passkey_model.AddNewPasskeyForTesting(CreatePasskey());
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_EQ(UpdatePasskeyModelForSignalAllAcceptedCredentials(
                origin, kRpId, kUserId, {}, passkey_model),
            SignalAllAcceptedCredentialsResult::kPasskeyHidden);

  std::optional<sync_pb::WebauthnCredentialSpecifics> passkey =
      passkey_model.GetPasskey(
          kRpId, std::string(kCredentialId.begin(), kCredentialId.end()),
          PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_TRUE(passkey.has_value());
  EXPECT_TRUE(passkey->hidden());
}

TEST_F(SignalApiUtilsTest, SignalAllAcceptedCredentials_Restore) {
  TestPasskeyModel passkey_model;
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey.set_hidden(true);
  passkey_model.AddNewPasskeyForTesting(passkey);
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_EQ(UpdatePasskeyModelForSignalAllAcceptedCredentials(
                origin, kRpId, kUserId, {kCredentialId}, passkey_model),
            SignalAllAcceptedCredentialsResult::kPasskeyRestored);

  std::optional<sync_pb::WebauthnCredentialSpecifics> updated =
      passkey_model.GetPasskey(
          kRpId, std::string(kCredentialId.begin(), kCredentialId.end()),
          PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_TRUE(updated.has_value());
  EXPECT_FALSE(updated->hidden());
}

TEST_F(SignalApiUtilsTest, SignalCurrentUserDetails_Update) {
  TestPasskeyModel passkey_model;
  passkey_model.AddNewPasskeyForTesting(CreatePasskey());
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_TRUE(UpdatePasskeyModelForSignalCurrentUserDetails(
      origin, kRpId, kUserId, "new_name", "New Display Name", passkey_model));

  std::optional<sync_pb::WebauthnCredentialSpecifics> updated =
      passkey_model.GetPasskey(
          kRpId, std::string(kCredentialId.begin(), kCredentialId.end()),
          PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(updated->user_name(), "new_name");
  EXPECT_EQ(updated->user_display_name(), "New Display Name");
}

TEST_F(SignalApiUtilsTest, SignalCurrentUserDetails_NoUpdateNeeded) {
  TestPasskeyModel passkey_model;
  passkey_model.AddNewPasskeyForTesting(CreatePasskey());
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_FALSE(UpdatePasskeyModelForSignalCurrentUserDetails(
      origin, kRpId, kUserId, kName, kDisplayName, passkey_model));
}

}  // namespace
}  // namespace webauthn
