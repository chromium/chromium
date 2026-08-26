// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/imported_passkey_checker.h"

#include "base/rand_util.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/import/passkey_import_candidate.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "crypto/keypair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

PasskeyImportCandidate CreateValidPasskey() {
  return PasskeyImportCandidate{
      .rp_id = "example.com",
      .user_name = "username",
      .user_display_name = "display_name",
      .credential_id =
          base::RandBytesAsVector(passkey_model_utils::kCredentialIdMinLength),
      .user_id = base::RandBytesAsVector(passkey_model_utils::kUserIdMaxLength),
      .private_key =
          crypto::keypair::PrivateKey::GenerateEcP256().ToPrivateKeyInfo(),
  };
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForValidPasskey) {
  EXPECT_EQ(CheckImportedPasskey(CreateValidPasskey()),
            ImportedPasskeyStatus::kOk);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooShortCredentialId) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.credential_id =
      base::RandBytesAsVector(passkey_model_utils::kCredentialIdMinLength - 1);

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kCredentialIdTooShort);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooLongCredentialId) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.credential_id =
      base::RandBytesAsVector(passkey_model_utils::kCredentialIdMaxLength + 1);

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kCredentialIdTooLong);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForTooLongUserId) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.user_id =
      base::RandBytesAsVector(passkey_model_utils::kUserIdMaxLength + 1);

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kUserIdTooLong);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForMissingPrivateKey) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.private_key.clear();

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kPrivateKeyMissing);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForMissingRpId) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.rp_id.clear();

  EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kRpIdMissing);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForInvalidPrivateKey) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.private_key = {'n', 'o', 't', '_', 'a', '_', 'k', 'e', 'y'};

  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kPrivateKeyInvalid);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForUnsupportedAlgorithm) {
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.private_key =
        crypto::keypair::PrivateKey::GenerateRsa2048().ToPrivateKeyInfo();
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kPrivateKeyUnsupportedAlgorithm);
  }
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.private_key =
        crypto::keypair::PrivateKey::GenerateEcP384().ToPrivateKeyInfo();
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kPrivateKeyUnsupportedAlgorithm);
  }
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.private_key =
        crypto::keypair::PrivateKey::GenerateEd25519().ToPrivateKeyInfo();
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kPrivateKeyUnsupportedAlgorithm);
  }
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForInvalidHmacSecretSize) {
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize - 1);
    passkey.hmac_secret_algorithm = "sha256";
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kHmacSecretInvalidSize);
  }
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize + 1);
    passkey.hmac_secret_algorithm = "sha256";
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kHmacSecretInvalidSize);
  }
  {
    // `kHmacSecretSize`-byte HMAC secret with sha256 is valid.
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize);
    passkey.hmac_secret_algorithm = "sha256";
    EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kOk);
  }
}

TEST(ImportedPasskeyCheckerTest,
     ReturnsStatusForUnsupportedHmacSecretAlgorithm) {
  {
    // HMAC secret without algorithm is unsupported.
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize);
    passkey.hmac_secret_algorithm = std::nullopt;
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kHmacSecretUnsupportedAlgorithm);
  }
  {
    // HMAC secret with non-sha256 algorithm is unsupported.
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize);
    passkey.hmac_secret_algorithm = "sha512";
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kHmacSecretUnsupportedAlgorithm);
  }
  {
    PasskeyImportCandidate passkey = CreateValidPasskey();
    passkey.hmac_secret =
        base::RandBytesAsVector(passkey_model_utils::kHmacSecretSize);
    passkey.hmac_secret_algorithm = "unsupported";
    EXPECT_EQ(CheckImportedPasskey(passkey),
              ImportedPasskeyStatus::kHmacSecretUnsupportedAlgorithm);
  }
}

TEST(ImportedPasskeyCheckerTest,
     ReturnsStatusForLargeBlobWithoutUncompressedSize) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob = {1, 2, 3};
  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kLargeBlobInvalid);
}

TEST(ImportedPasskeyCheckerTest,
     ReturnsStatusForUncompressedSizeWithoutLargeBlob) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob_uncompressed_size = 10;
  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kLargeBlobInvalid);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForLargeBlobTooLarge) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob = base::RandBytesAsVector(
      passkey_model_utils::kLargeBlobMaxCompressedSize + 1);
  passkey.large_blob_uncompressed_size = 3000;
  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kLargeBlobTooLarge);
}

TEST(ImportedPasskeyCheckerTest,
     ReturnsStatusForValidLargeBlobWithUncompressedSizeZero) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob = {1, 2, 3};
  passkey.large_blob_uncompressed_size = 0;
  EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kOk);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForValidEmptyLargeBlob) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob = std::vector<uint8_t>();
  passkey.large_blob_uncompressed_size = 0;
  EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kOk);
}

TEST(ImportedPasskeyCheckerTest,
     ReturnsStatusForLargeBlobUncompressedSizeTooLarge) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob = {1, 2, 3};
  passkey.large_blob_uncompressed_size =
      passkey_model_utils::kLargeBlobMaxUncompressedSize + 1;
  EXPECT_EQ(CheckImportedPasskey(passkey),
            ImportedPasskeyStatus::kLargeBlobUncompressedSizeTooLarge);
}

TEST(ImportedPasskeyCheckerTest, ReturnsStatusForValidLargeBlob) {
  PasskeyImportCandidate passkey = CreateValidPasskey();
  passkey.large_blob =
      base::RandBytesAsVector(passkey_model_utils::kLargeBlobMaxCompressedSize);
  passkey.large_blob_uncompressed_size = 3000;
  EXPECT_EQ(CheckImportedPasskey(passkey), ImportedPasskeyStatus::kOk);
}

}  // namespace
}  // namespace webauthn
