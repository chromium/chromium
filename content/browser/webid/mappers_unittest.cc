// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/mappers.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using Field = content::IdentityRequestDialogDisclosureField;
using LoginState = content::IdentityRequestAccount::LoginState;
using ::testing::ElementsAre;

namespace content {
namespace webid {

namespace {
IdentityRequestAccountPtr CreateEmptyAccount() {
  std::vector<std::string> empty;
  return base::MakeRefCounted<IdentityRequestAccount>(
      /*id=*/"",
      /*display_identifier=*/"", /*display_name=*/"", /*email=*/"",
      /*name=*/"", /*given_name=*/"", /*picture=*/GURL(), /*phone=*/"",
      /*username=*/"", /*login_hints=*/empty, /*domain_hints=*/empty,
      /*labels=*/empty);
}
}  // namespace

TEST(FedCmMappersTest, GetDisclosureFieldsEmpty) {
  // An unknown field is being requested.
  std::vector<std::string> fields = {"address"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)), ElementsAre());
  // Nothing is requested.
  EXPECT_THAT(
      GetDisclosureFields(std::make_optional<std::vector<std::string>>({})),
      ElementsAre());
}

TEST(FedCmMappersTest, GetDisclosureFieldsWithoutAlternativeIdentifiers) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmAlternativeIdentifiers);
  // When no fields are passed, we use the default.
  EXPECT_THAT(GetDisclosureFields(std::nullopt),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
  // When the default fields are explicitly passed, we should mediate them.
  std::vector<std::string> fields = {"name", "email", "picture"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
  // When a superset of the supported fields is passed, we should mediate the
  // supported fields.
  fields = {"name", "email", "picture", "locale", "tel"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
}

TEST(FedCmMappersTest, GetDisclosureFieldsWithAlternativeIdentifiers) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAlternativeIdentifiers);
  // When a superset of the supported fields is passed, we should mediate the
  // supported fields.
  std::vector<std::string> fields = {"name", "email", "picture", "locale",
                                     "tel"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture,
                          Field::kPhoneNumber));
}

TEST(FedCmMappersTest, GetDisclosureFieldsWithAlternativeIdentifiersDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmAlternativeIdentifiers);
  // We should only support the new identifiers if the flag is enabled
  std::vector<std::string> fields = {"username", "tel"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)), ElementsAre());
}

TEST(FedCmMappersTest, GetDisclosureFieldsSubsetOfDefault) {
  // Subsets of the default fields should work.
  std::vector<std::string> fields = {"name", "locale"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName));
}

TEST(FedCmMappersTest, ComputeAccountFields) {
  std::vector<Field> fields = {Field::kName, Field::kPicture};
  IdentityRequestAccountPtr account = CreateEmptyAccount();
  std::vector<IdentityRequestAccountPtr> accounts{account};

  ComputeAccountFields(fields, accounts);
  EXPECT_EQ(0u, account->fields.size());

  account->name = "First Last";
  ComputeAccountFields(fields, accounts);
  EXPECT_THAT(account->fields, ElementsAre(Field::kName));

  account->browser_trusted_login_state = LoginState::kSignIn;
  ComputeAccountFields(fields, accounts);
  EXPECT_EQ(0u, account->fields.size());

  // IDP login state should override browser login state
  account->idp_claimed_login_state = LoginState::kSignUp;
  ComputeAccountFields(fields, accounts);
  EXPECT_THAT(account->fields, ElementsAre(Field::kName));
}

}  // namespace webid
}  // namespace content
