// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/import/passkey_import_candidate.h"
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

using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr char kRpId[] = "example.com";
constexpr char kUserId[] = "user_id";
constexpr char kUserId2[] = "user_id2";

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
  candidate.user_id = std::vector<uint8_t>(user_id.begin(), user_id.end());
  candidate.private_key =
      crypto::keypair::PrivateKey::GenerateEcP256().ToPrivateKeyInfo();
  candidate.creation_time = 1234567890;
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
    passkey_importer_->StartImport(
        std::move(passkeys), std::vector<uint8_t>(32, 0), future.GetCallback());
    return future.Get();
  }

  int FinishImport(std::vector<int> selected_passkey_ids) {
    base::test::TestFuture<int> future;
    passkey_importer_->FinishImport(std::move(selected_passkey_ids),
                                    future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
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
}

}  // namespace
}  // namespace webauthn
