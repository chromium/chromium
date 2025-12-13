// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/imported_passkey_checker.h"

#include "base/rand_util.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

sync_pb::WebauthnCredentialSpecifics CreateValidPasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id("example.com");
  passkey.set_sync_id(
      base::RandBytesAsString(passkey_model_utils::kSyncIdLength));
  passkey.set_credential_id(
      base::RandBytesAsString(webauthn::kCredentialIdMinLength));
  passkey.set_user_id(
      base::RandBytesAsString(passkey_model_utils::kUserIdMaxLength));
  passkey.set_private_key({1, 2, 3, 4});
  passkey.set_user_name("username");
  passkey.set_user_display_name("display_name");
  return passkey;
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForValidPasskey) {
  EXPECT_EQ(CheckImportedPasskey(CreateValidPasskey()),
            ImportedPasskeyStatus::kOk);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooShortCredentialId) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.set_credential_id(
      base::RandBytesAsString(webauthn::kCredentialIdMinLength - 1));

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kCredentialIdTooShort);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooLongCredentialId) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.set_credential_id(
      base::RandBytesAsString(webauthn::kCredentialIdMaxLength + 1));

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kCredentialIdTooLong);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooLongUserId) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.set_user_id(
      base::RandBytesAsString(passkey_model_utils::kUserIdMaxLength + 1));

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kUserIdTooLong);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForMissingPrivateKey) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.clear_private_key();

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kPrivateKeyMissing);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForMissingRpId) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreateValidPasskey();
  passkey.clear_rp_id();

  EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kRpIdMissing);
}

}  // namespace
}  // namespace webauthn
