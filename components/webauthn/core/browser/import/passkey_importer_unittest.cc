// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/import/passkey_import_candidate.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "crypto/keypair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

MATCHER_P3(ImportedInfoIs, rp_id, user_name, status, "") {
  return arg.rp_id == rp_id && arg.user_name == user_name &&
         arg.status == status;
}

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr char kRpId[] = "example.com";
constexpr char kUserId[] = "user_id";
constexpr char kUserId2[] = "user_id2";

std::vector<uint8_t> ToByteVector(std::string_view str) {
  return base::ToVector(base::as_byte_span(str));
}

std::vector<uint8_t> TestTrustedVaultKey() {
  return std::vector<uint8_t>(32, 0);
}

sync_pb::WebauthnCredentialSpecifics CreateSpecifics(
    const std::string& rp_id,
    const std::string& user_id) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id(rp_id);
  passkey.set_user_id(user_id);
  passkey.set_encrypted("dummy_encrypted");
  passkey.set_user_name("username");
  passkey.set_user_display_name("display_name");
  return passkey;
}

PasskeyImportCandidate CreateCandidate(const std::string& rp_id,
                                       const std::string& user_id) {
  PasskeyImportCandidate candidate;
  candidate.rp_id = rp_id;
  candidate.user_name = "username";
  candidate.user_display_name = "display_name";
  candidate.credential_id = std::vector<uint8_t>(16, 'a');
  candidate.user_id = ToByteVector(user_id);
  candidate.private_key =
      crypto::keypair::PrivateKey::GenerateEcP256().ToPrivateKeyInfo();
  return candidate;
}

class PasskeyImporterTest : public testing::Test {
 public:
  PasskeyImporterTest()
      : passkey_model_(std::make_unique<TestPasskeyModel>()),
        passkey_importer_(
            std::make_unique<PasskeyImporter>(*passkey_model_.get())) {}

  ImportProcessingResult StartImport(
      std::vector<PasskeyImportCandidate> passkeys) {
    base::test::TestFuture<const ImportProcessingResult&> future;
    passkey_importer_->StartImport(std::move(passkeys), TestTrustedVaultKey(),
                                   future.GetCallback());
    return future.Get();
  }

  int FinishImport(std::vector<int> selected_passkey_ids) {
    base::test::TestFuture<int> future;
    passkey_importer_->FinishImport(std::move(selected_passkey_ids),
                                    future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestPasskeyModel> passkey_model_;
  std::unique_ptr<PasskeyImporter> passkey_importer_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PasskeyImporterTest, ProcessesValidPasskeys) {
  ImportProcessingResult result =
      StartImport({CreateCandidate(kRpId, kUserId)});

  EXPECT_EQ(result.valid_passkeys_amount, 1);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());
}

TEST_F(PasskeyImporterTest, ProcessesInvalidPasskeys) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.private_key = {};
  ImportProcessingResult result = StartImport({candidate});

  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, UnorderedElementsAre(ImportedInfoIs(
                                 kRpId, "username",
                                 ImportedPasskeyStatus::kPrivateKeyMissing)));
  EXPECT_THAT(result.conflicts, IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyImportStatus",
      ImportedPasskeyStatus::kPrivateKeyMissing, 1);
}

TEST_F(PasskeyImporterTest, ProcessesDuplicatePasskey) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  // Add an already existing passkey to the model with the same credential_id.
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_rp_id(kRpId);
  specifics.set_credential_id(std::string(candidate.credential_id.begin(),
                                          candidate.credential_id.end()));
  passkey_model_->AddNewPasskeyForTesting(specifics);

  std::ignore = StartImport({candidate});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});

  // Duplicate passkey should be reported as imported, but not actually added
  // to the model.
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(1));
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyDuplicatesCount", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 1, 1);
}

TEST_F(PasskeyImporterTest, ProcessesConflictingPasskeys) {
  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, kUserId));

  ImportProcessingResult result =
      StartImport({CreateCandidate(kRpId, kUserId)});

  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts,
              UnorderedElementsAre(ImportedInfoIs(kRpId, "username",
                                                  ImportedPasskeyStatus::kOk)));
}

TEST_F(PasskeyImporterTest, ImportsValidPasskeys) {
  std::ignore = StartImport(
      {CreateCandidate(kRpId, kUserId), CreateCandidate(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 2);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(2));
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 2, 1);
}

TEST_F(PasskeyImporterTest, SetsCreationTimeToCurrentTimeOnImport) {
  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, kUserId));

  base::Time time_now = base::Time::Now();
  PasskeyImportCandidate valid_candidate = CreateCandidate(kRpId, kUserId2);
  valid_candidate.exporter_creation_time = time_now - base::Days(10);
  valid_candidate.credential_id = std::vector<uint8_t>(16, 'v');

  PasskeyImportCandidate conflicting_candidate =
      CreateCandidate(kRpId, kUserId);
  conflicting_candidate.exporter_creation_time = time_now - base::Days(10);
  conflicting_candidate.credential_id = std::vector<uint8_t>(16, 'c');

  std::ignore = StartImport({valid_candidate, conflicting_candidate});
  std::ignore = FinishImport(/*selected_passkey_ids=*/{0});

  std::optional<sync_pb::WebauthnCredentialSpecifics> valid_passkey =
      passkey_model_->GetPasskey(kRpId, std::string(16, 'v'),
                                 PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_TRUE(valid_passkey.has_value());
  EXPECT_EQ(valid_passkey->creation_time(),
            time_now.InMillisecondsSinceUnixEpoch());

  std::optional<sync_pb::WebauthnCredentialSpecifics> conflicting_passkey =
      passkey_model_->GetPasskey(kRpId, std::string(16, 'c'),
                                 PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_TRUE(conflicting_passkey.has_value());
  EXPECT_EQ(conflicting_passkey->creation_time(),
            time_now.InMillisecondsSinceUnixEpoch());
}

TEST_F(PasskeyImporterTest, ImportsIncomingConflictingPasskey) {
  sync_pb::WebauthnCredentialSpecifics stored_passkey =
      CreateSpecifics(kRpId, kUserId);
  passkey_model_->AddNewPasskeyForTesting(stored_passkey);

  std::ignore = StartImport(
      {CreateCandidate(kRpId, kUserId), CreateCandidate(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{0});
  EXPECT_EQ(passkeys_imported, 2);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(3));
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsCount", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsResolvedCount", 1,
      1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 2, 1);
}

TEST_F(PasskeyImporterTest, IgnoresNotSelectedConflictingPasskey) {
  sync_pb::WebauthnCredentialSpecifics stored_passkey =
      CreateSpecifics(kRpId, kUserId);
  passkey_model_->AddNewPasskeyForTesting(stored_passkey);

  std::ignore = StartImport(
      {CreateCandidate(kRpId, kUserId), CreateCandidate(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(2));
}

TEST_F(PasskeyImporterTest, DoesNotImportInvalidPasskeys) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.private_key = {};
  std::ignore = StartImport({candidate});

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 0);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 0, 1);
}

TEST_F(PasskeyImporterTest, ImportsPasskeyWithHmacSecret) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.hmac_secret =
      std::vector<uint8_t>(passkey_model_utils::kHmacSecretSize, 'a');
  candidate.hmac_secret_algorithm = "sha256";
  std::ignore = StartImport({candidate});

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  auto passkeys = passkey_model_->GetPasskeys(
      PasskeyModel::AnyRp(), PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  EXPECT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      TestTrustedVaultKey(), /*device_authorization_key=*/{}, passkeys[0],
      &decrypted));
  EXPECT_EQ(decrypted.hmac_secret(),
            std::string(passkey_model_utils::kHmacSecretSize, 'a'));
}

TEST_F(PasskeyImporterTest, ImportsPasskeyWithLargeBlob) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.large_blob = ToByteVector("large_blob");
  candidate.large_blob_uncompressed_size = 100;
  std::ignore = StartImport({candidate});

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  auto passkeys = passkey_model_->GetPasskeys(
      PasskeyModel::AnyRp(), PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  EXPECT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      TestTrustedVaultKey(), /*device_authorization_key=*/{}, passkeys[0],
      &decrypted));
  EXPECT_EQ(decrypted.large_blob(), "large_blob");
  EXPECT_EQ(decrypted.large_blob_uncompressed_size(), 100u);
}

// Test that importing a passkey with an empty `large_blob` and zero
// `large_blob_uncompressed_size` preserves both fields in the encrypted data.
TEST_F(PasskeyImporterTest, ImportsPasskeyWithEmptyLargeBlob) {
  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.large_blob = std::vector<uint8_t>();
  candidate.large_blob_uncompressed_size = 0;
  std::ignore = StartImport({candidate});

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  auto passkeys = passkey_model_->GetPasskeys(
      PasskeyModel::AnyRp(), PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  EXPECT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      TestTrustedVaultKey(), /*device_authorization_key=*/{}, passkeys[0],
      &decrypted));
  EXPECT_TRUE(decrypted.has_large_blob());
  EXPECT_EQ(decrypted.large_blob(), "");
  EXPECT_TRUE(decrypted.has_large_blob_uncompressed_size());
  EXPECT_EQ(decrypted.large_blob_uncompressed_size(), 0u);
}

// Test that importing a passkey whose credential ID matches a shadowed passkey
// is treated as a duplicate without modifying the shadow chain.
TEST_F(PasskeyImporterTest, ProcessesDuplicateMatchingShadowedPasskey) {
  sync_pb::WebauthnCredentialSpecifics shadowed =
      CreateSpecifics(kRpId, kUserId);
  shadowed.set_credential_id("shadowed_cred_id");
  passkey_model_->AddNewPasskeyForTesting(shadowed);

  sync_pb::WebauthnCredentialSpecifics active = CreateSpecifics(kRpId, kUserId);
  active.set_credential_id("active_cred_id");
  active.add_newly_shadowed_credential_ids("shadowed_cred_id");
  passkey_model_->AddNewPasskeyForTesting(active);

  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.credential_id = ToByteVector("shadowed_cred_id");

  ImportProcessingResult result = StartImport({candidate});
  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(2));
  std::vector<sync_pb::WebauthnCredentialSpecifics> unshadowed_passkeys =
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_THAT(unshadowed_passkeys, SizeIs(1));
  EXPECT_EQ(unshadowed_passkeys[0].credential_id(), "active_cred_id");
  EXPECT_THAT(unshadowed_passkeys[0].newly_shadowed_credential_ids(),
              ElementsAre("shadowed_cred_id"));
  std::optional<sync_pb::WebauthnCredentialSpecifics> stored_shadowed =
      passkey_model_->GetPasskey(kRpId, "shadowed_cred_id",
                                 PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_TRUE(stored_shadowed.has_value());
  EXPECT_THAT(stored_shadowed->newly_shadowed_credential_ids(), IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyDuplicatesCount", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 1, 1);
}

// Test that importing a passkey for a user ID that only has a shadowed passkey
// in the model does not trigger a conflict and shadows the old credential.
TEST_F(PasskeyImporterTest, DoesNotConflictWithShadowedPasskey) {
  sync_pb::WebauthnCredentialSpecifics shadowed =
      CreateSpecifics(kRpId, kUserId);
  shadowed.set_credential_id("old_cred_id");
  passkey_model_->AddNewPasskeyForTesting(shadowed);

  // Create an active credential with a different user ID (`kUserId2`) that
  // shadows `old_cred_id` (`kUserId`). This ensures `kUserId` has a shadowed
  // credential in the model, but no active credential for that user ID.
  sync_pb::WebauthnCredentialSpecifics active =
      CreateSpecifics(kRpId, kUserId2);
  active.set_credential_id("active_cred_id");
  active.add_newly_shadowed_credential_ids("old_cred_id");
  passkey_model_->AddNewPasskeyForTesting(active);

  PasskeyImportCandidate candidate = CreateCandidate(kRpId, kUserId);
  candidate.credential_id = std::vector<uint8_t>(16, 'x');

  ImportProcessingResult result = StartImport({candidate});
  EXPECT_EQ(result.valid_passkeys_amount, 1);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(3));
  std::optional<sync_pb::WebauthnCredentialSpecifics> imported_passkey =
      passkey_model_->GetPasskey(kRpId, std::string(16, 'x'),
                                 PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_TRUE(imported_passkey.has_value());
  EXPECT_THAT(imported_passkey->newly_shadowed_credential_ids(),
              ElementsAre("old_cred_id"));
}

// Test that only the user-selected subset of conflicting passkeys is imported
// into the model.
TEST_F(PasskeyImporterTest, ImportsSubsetOfMultipleConflictingPasskeys) {
  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, "user_1"));
  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, "user_2"));
  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, "user_3"));

  PasskeyImportCandidate candidate1 = CreateCandidate(kRpId, "user_1");
  candidate1.user_name = "user_1";
  candidate1.credential_id = std::vector<uint8_t>(16, '1');
  PasskeyImportCandidate candidate2 = CreateCandidate(kRpId, "user_2");
  candidate2.user_name = "user_2";
  candidate2.credential_id = std::vector<uint8_t>(16, '2');
  PasskeyImportCandidate candidate3 = CreateCandidate(kRpId, "user_3");
  candidate3.user_name = "user_3";
  candidate3.credential_id = std::vector<uint8_t>(16, '3');

  ImportProcessingResult result =
      StartImport({candidate1, candidate2, candidate3});
  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(
      result.conflicts,
      ElementsAre(ImportedInfoIs(kRpId, "user_1", ImportedPasskeyStatus::kOk),
                  ImportedInfoIs(kRpId, "user_2", ImportedPasskeyStatus::kOk),
                  ImportedInfoIs(kRpId, "user_3", ImportedPasskeyStatus::kOk)));

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{0, 2});
  EXPECT_EQ(passkeys_imported, 2);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(5));
  EXPECT_TRUE(passkey_model_
                  ->GetPasskey(kRpId, std::string(16, '1'),
                               PasskeyModel::ShadowedCredentials::kInclude)
                  .has_value());
  EXPECT_FALSE(passkey_model_
                   ->GetPasskey(kRpId, std::string(16, '2'),
                                PasskeyModel::ShadowedCredentials::kInclude)
                   .has_value());
  EXPECT_TRUE(passkey_model_
                  ->GetPasskey(kRpId, std::string(16, '3'),
                               PasskeyModel::ShadowedCredentials::kInclude)
                  .has_value());
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsCount", 3, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsResolvedCount", 2,
      1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 2, 1);
}

// Test that a single import batch containing valid, duplicate, conflicting, and
// invalid passkeys is categorized and imported accurately.
TEST_F(PasskeyImporterTest, ProcessesMixedBatch) {
  sync_pb::WebauthnCredentialSpecifics existing_duplicate =
      CreateSpecifics(kRpId, "duplicate_user");
  existing_duplicate.set_credential_id("existing_duplicate_cred_id");
  passkey_model_->AddNewPasskeyForTesting(existing_duplicate);

  sync_pb::WebauthnCredentialSpecifics existing_conflict =
      CreateSpecifics(kRpId, "conflict_user");
  existing_conflict.set_credential_id("existing_conflict_cred_id");
  passkey_model_->AddNewPasskeyForTesting(existing_conflict);

  PasskeyImportCandidate valid_candidate = CreateCandidate(kRpId, "valid_user");
  valid_candidate.user_name = "valid_user";
  valid_candidate.credential_id = std::vector<uint8_t>(16, 'v');

  PasskeyImportCandidate duplicate_candidate =
      CreateCandidate(kRpId, "duplicate_user");
  duplicate_candidate.user_name = "duplicate_user";
  duplicate_candidate.credential_id =
      ToByteVector("existing_duplicate_cred_id");

  PasskeyImportCandidate conflict_candidate =
      CreateCandidate(kRpId, "conflict_user");
  conflict_candidate.user_name = "conflict_user";
  conflict_candidate.credential_id = std::vector<uint8_t>(16, 'c');

  PasskeyImportCandidate invalid_candidate =
      CreateCandidate(kRpId, "invalid_user");
  invalid_candidate.user_name = "invalid_user";
  invalid_candidate.credential_id = std::vector<uint8_t>(16, 'i');
  invalid_candidate.private_key = {};

  ImportProcessingResult result =
      StartImport({valid_candidate, duplicate_candidate, conflict_candidate,
                   invalid_candidate});

  EXPECT_EQ(result.valid_passkeys_amount, 1);
  EXPECT_THAT(result.conflicts,
              UnorderedElementsAre(ImportedInfoIs(kRpId, "conflict_user",
                                                  ImportedPasskeyStatus::kOk)));
  EXPECT_THAT(result.errors, UnorderedElementsAre(ImportedInfoIs(
                                 kRpId, "invalid_user",
                                 ImportedPasskeyStatus::kPrivateKeyMissing)));

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{0});
  EXPECT_EQ(passkeys_imported, 3);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(4));
  EXPECT_TRUE(passkey_model_
                  ->GetPasskey(kRpId, std::string(16, 'v'),
                               PasskeyModel::ShadowedCredentials::kInclude)
                  .has_value());
  EXPECT_TRUE(passkey_model_
                  ->GetPasskey(kRpId, std::string(16, 'c'),
                               PasskeyModel::ShadowedCredentials::kInclude)
                  .has_value());
  EXPECT_FALSE(passkey_model_
                   ->GetPasskey(kRpId, std::string(16, 'i'),
                                PasskeyModel::ShadowedCredentials::kInclude)
                   .has_value());

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 3, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyDuplicatesCount", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsCount", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsResolvedCount", 1,
      1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyImportStatus",
      ImportedPasskeyStatus::kPrivateKeyMissing, 1);
}

// Test that an incoming passkey with the same user ID as an existing passkey on
// a different RP is imported without conflict.
TEST_F(PasskeyImporterTest, DoesNotConflictWithSameUserIdOnDifferentRp) {
  passkey_model_->AddNewPasskeyForTesting(
      CreateSpecifics("site-a.com", kUserId));

  PasskeyImportCandidate candidate = CreateCandidate("site-b.com", kUserId);
  ImportProcessingResult result = StartImport({candidate});

  EXPECT_EQ(result.valid_passkeys_amount, 1);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      SizeIs(2));
}

// Test that all unencrypted and encrypted fields from an import candidate are
// preserved in the stored `WebauthnCredentialSpecifics`.
TEST_F(PasskeyImporterTest, PreservesAllCandidateFieldsInStoredSpecifics) {
  PasskeyImportCandidate candidate;
  candidate.rp_id = "test-domain.com";
  candidate.user_name = "test_user_name";
  candidate.user_display_name = "Test User Display Name";
  candidate.credential_id = ToByteVector("cred_1234567890a");
  candidate.user_id = ToByteVector("user_id_123");
  auto key_pair = crypto::keypair::PrivateKey::GenerateEcP256();
  candidate.private_key = key_pair.ToPrivateKeyInfo();

  std::ignore = StartImport({candidate});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);

  auto passkeys = passkey_model_->GetPasskeys(
      PasskeyModel::AnyRp(), PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  const sync_pb::WebauthnCredentialSpecifics& stored = passkeys[0];

  EXPECT_EQ(stored.sync_id().size(), passkey_model_utils::kSyncIdLength);
  EXPECT_EQ(base::as_byte_span(stored.credential_id()),
            candidate.credential_id);
  EXPECT_EQ(base::as_byte_span(stored.user_id()), candidate.user_id);
  EXPECT_EQ(stored.rp_id(), candidate.rp_id);
  EXPECT_EQ(stored.user_name(), candidate.user_name);
  EXPECT_EQ(stored.user_display_name(), candidate.user_display_name);

  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  EXPECT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      TestTrustedVaultKey(), /*device_authorization_key=*/{}, stored,
      &decrypted));
  EXPECT_EQ(base::as_byte_span(decrypted.private_key()), candidate.private_key);
}

// Test that the candidate's `exporter_creation_time` is preserved in the
// conflict and error entries of `ImportProcessingResult`.
TEST_F(PasskeyImporterTest, PreservesExporterCreationTimeInProcessingResult) {
  base::Time creation_time =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);

  passkey_model_->AddNewPasskeyForTesting(CreateSpecifics(kRpId, kUserId));
  PasskeyImportCandidate conflict_candidate = CreateCandidate(kRpId, kUserId);
  conflict_candidate.exporter_creation_time = creation_time;

  PasskeyImportCandidate invalid_candidate = CreateCandidate(kRpId, kUserId2);
  invalid_candidate.private_key = {};
  invalid_candidate.exporter_creation_time = creation_time;

  ImportProcessingResult result =
      StartImport({conflict_candidate, invalid_candidate});

  ASSERT_THAT(result.conflicts, SizeIs(1));
  EXPECT_EQ(result.conflicts[0].exporter_creation_time, creation_time);

  ASSERT_THAT(result.errors, SizeIs(1));
  EXPECT_EQ(result.errors[0].exporter_creation_time, creation_time);
}

// Test that importing an empty list of candidates completes cleanly and records
// zero counts in all histograms.
TEST_F(PasskeyImporterTest, HandlesEmptyImport) {
  ImportProcessingResult result = StartImport({});
  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 0);
  EXPECT_THAT(
      passkey_model_->GetPasskeys(PasskeyModel::AnyRp(),
                                  PasskeyModel::ShadowedCredentials::kInclude),
      IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyDuplicatesCount", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsCount", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.CredentialExchange.PasskeyConflictsResolvedCount", 0,
      1);
}

}  // namespace
}  // namespace webauthn
