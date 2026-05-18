// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/virtual_wallet.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/digital_identity_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using DigitalCredential = DigitalIdentityProvider::DigitalCredential;

DigitalCredential GenerateTestCredential() {
  constexpr char kCredentialJson[] = R"({
    "credential_id": "test-cred-123",
    "claims": {
      "age_over_18": true
    }
  })";
  return DigitalCredential("openid4vp", base::test::ParseJson(kCredentialJson));
}

DigitalCredential GenerateSecondTestCredential() {
  constexpr char kCredentialJson[] = R"({
    "credential_id": "test-cred-456",
    "claims": {
      "document_type": "passport",
      "issuing_country": "US"
    }
  })";
  return DigitalCredential("mdoc", base::test::ParseJson(kCredentialJson));
}

class VirtualWalletTest : public testing::Test {
 public:
  VirtualWalletTest() = default;
  ~VirtualWalletTest() override = default;

  VirtualWalletTest(const VirtualWalletTest&) = delete;
  VirtualWalletTest& operator=(const VirtualWalletTest&) = delete;

 protected:
  void SetUp() override { wallet_ = std::make_unique<VirtualWallet>(); }

  VirtualWallet* wallet() { return wallet_.get(); }

 private:
  std::unique_ptr<VirtualWallet> wallet_;
};

// GetCredential() is non-consuming: successive calls each return a fresh
// independent copy of the stored credential.
TEST_F(VirtualWalletTest, GetCredentialIsNonConsuming) {
  wallet()->SetCredential(GenerateTestCredential());

  std::optional<DigitalCredential> first = wallet()->GetCredential();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->protocol, "openid4vp");

  std::optional<DigitalCredential> second = wallet()->GetCredential();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->protocol, "openid4vp");
  const std::string* cred_id =
      second->data.GetDict().FindString("credential_id");
  ASSERT_TRUE(cred_id);
  EXPECT_EQ(*cred_id, "test-cred-123");
}

// SetCredential() overwrites a previously stored credential
// (last write wins).
TEST_F(VirtualWalletTest, Overwrite) {
  wallet()->SetCredential(GenerateTestCredential());
  wallet()->SetCredential(GenerateSecondTestCredential());

  std::optional<DigitalCredential> retrieved = wallet()->GetCredential();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->protocol, "mdoc");
  const base::DictValue& data_dict = retrieved->data.GetDict();

  const std::string* credential_id = data_dict.FindString("credential_id");
  ASSERT_TRUE(credential_id);
  EXPECT_EQ(*credential_id, "test-cred-456");

  const base::DictValue* claims = data_dict.FindDict("claims");
  ASSERT_TRUE(claims);
  const std::string* document_type = claims->FindString("document_type");
  ASSERT_TRUE(document_type);
  EXPECT_EQ(*document_type, "passport");
  const std::string* issuing_country = claims->FindString("issuing_country");
  ASSERT_TRUE(issuing_country);
  EXPECT_EQ(*issuing_country, "US");
  EXPECT_FALSE(claims->FindBool("age_over_18").has_value());
}

// Clear() wipes both the stored credential and the behavior.
TEST_F(VirtualWalletTest, ClearWipesCredentialAndBehavior) {
  wallet()->SetCredential(GenerateTestCredential());
  wallet()->set_behavior(VirtualWallet::Behavior::kRespond);

  wallet()->Clear();

  EXPECT_FALSE(wallet()->behavior().has_value());
  EXPECT_FALSE(wallet()->GetCredential().has_value());
}

}  // namespace
}  // namespace content
