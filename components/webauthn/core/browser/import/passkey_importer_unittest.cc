// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
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

sync_pb::WebauthnCredentialSpecifics CreatePasskey(const std::string& rp_id,
                                                   const std::string& user_id) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id(rp_id);
  passkey.set_user_id(user_id);
  passkey.set_private_key({1, 2, 3, 4});
  passkey.set_user_name("username");
  passkey.set_user_display_name("display_name");
  return passkey;
}

class PasskeyImporterTest : public testing::Test {
 public:
  PasskeyImporterTest()
      : passkey_model_(std::make_unique<TestPasskeyModel>()),
        passkey_importer_(
            std::make_unique<PasskeyImporter>(*passkey_model_.get())) {}

  ImportProcessingResult StartImport(
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
    base::test::TestFuture<const ImportProcessingResult&> future;
    passkey_importer_->StartImport(std::move(passkeys), future.GetCallback());
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
};

TEST_F(PasskeyImporterTest, ProcessesValidPasskeys) {
  ImportProcessingResult result = StartImport({CreatePasskey(kRpId, kUserId)});

  EXPECT_EQ(result.valid_passkeys_amount, 1);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts, IsEmpty());
}

TEST_F(PasskeyImporterTest, ProcessesInvalidPasskeys) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey(kRpId, kUserId);
  passkey.clear_private_key();
  ImportProcessingResult result = StartImport({passkey});

  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, UnorderedElementsAre(ImportedInfoIs(
                                 kRpId, "username",
                                 ImportedPasskeyStatus::kPrivateKeyMissing)));
  EXPECT_THAT(result.conflicts, IsEmpty());
}

TEST_F(PasskeyImporterTest, ProcessesConflictingPasskeys) {
  passkey_model_->AddNewPasskeyForTesting(CreatePasskey(kRpId, kUserId));

  ImportProcessingResult result = StartImport({CreatePasskey(kRpId, kUserId)});

  EXPECT_EQ(result.valid_passkeys_amount, 0);
  EXPECT_THAT(result.errors, IsEmpty());
  EXPECT_THAT(result.conflicts,
              UnorderedElementsAre(ImportedInfoIs(kRpId, "username",
                                                  ImportedPasskeyStatus::kOk)));
}

TEST_F(PasskeyImporterTest, ImportsValidPasskeys) {
  std::ignore = StartImport(
      {CreatePasskey(kRpId, kUserId), CreatePasskey(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 2);
  EXPECT_THAT(passkey_model_->GetAllPasskeys(), SizeIs(2));
}

TEST_F(PasskeyImporterTest, ImportsIncomingConflictingPasskey) {
  sync_pb::WebauthnCredentialSpecifics stored_passkey =
      CreatePasskey(kRpId, kUserId);
  passkey_model_->AddNewPasskeyForTesting(stored_passkey);

  std::ignore = StartImport(
      {CreatePasskey(kRpId, kUserId), CreatePasskey(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{0});
  EXPECT_EQ(passkeys_imported, 2);
  EXPECT_THAT(passkey_model_->GetAllPasskeys(), SizeIs(3));
}

TEST_F(PasskeyImporterTest, IgnoresNotSelectedConflictingPasskey) {
  sync_pb::WebauthnCredentialSpecifics stored_passkey =
      CreatePasskey(kRpId, kUserId);
  passkey_model_->AddNewPasskeyForTesting(stored_passkey);

  std::ignore = StartImport(
      {CreatePasskey(kRpId, kUserId), CreatePasskey(kRpId, kUserId2)});
  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 1);
  EXPECT_THAT(passkey_model_->GetAllPasskeys(), SizeIs(2));
}

TEST_F(PasskeyImporterTest, DoesNotImportInvalidPasskeys) {
  sync_pb::WebauthnCredentialSpecifics invalid_passkey =
      CreatePasskey(kRpId, kUserId);
  invalid_passkey.clear_private_key();
  std::ignore = StartImport({invalid_passkey});

  int passkeys_imported = FinishImport(/*selected_passkey_ids=*/{});
  EXPECT_EQ(passkeys_imported, 0);
  EXPECT_THAT(passkey_model_->GetAllPasskeys(), IsEmpty());
}

}  // namespace
}  // namespace webauthn
