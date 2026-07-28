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
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"

namespace content {
namespace webid {

using ::testing::ElementsAre;
using Field = IdentityRequestDialogDisclosureField;
using IdentityRequestAccountPtr = scoped_refptr<IdentityRequestAccount>;
using LoginState = IdentityRequestAccount::LoginState;

namespace {
IdentityRequestAccountPtr CreateEmptyAccount() {
  std::vector<std::string> empty;
  return base::MakeRefCounted<IdentityRequestAccount>(
      /*id=*/"",
      /*display_identifier=*/"", /*display_name=*/"", /*email=*/"",
      /*name=*/"", /*given_name=*/"", /*picture=*/GURL(), /*phone=*/"",
      /*username=*/"", /*potentially_approved_site_hashes=*/empty,
      /*login_hints=*/empty, /*domain_hints=*/empty,
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

TEST(FedCmMappersTest, GetDisclosureFields) {
  // When a superset of the supported fields is passed, we should mediate the
  // supported fields in enum order.
  std::vector<std::string> fields = {"name",   "email", "picture",
                                     "locale", "tel",   "username"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kUsername,
                          Field::kPhoneNumber, Field::kPicture));
}

TEST(FedCmMappersTest, GetDisclosureFieldsSubsetOfDefault) {
  // Subsets of the default fields should work.
  std::vector<std::string> fields = {"name", "locale"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName));
}

TEST(FedCmMappersTest, GetDisclosureFieldsDuplicates) {
  // Duplicate fields should be deduplicated.
  std::vector<std::string> fields = {"name", "email", "name", "picture",
                                     "email"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kPicture));
}

TEST(FedCmMappersTest, GetDisclosureFieldsOrdering) {
  // Passing fields in arbitrary/unordered sequence should produce output
  // strictly ordered by the enum value definition: kName, kEmail, kUsername,
  // kPhoneNumber, kPicture.
  std::vector<std::string> fields = {"picture", "tel", "username", "email",
                                     "name"};
  EXPECT_THAT(GetDisclosureFields(std::make_optional(fields)),
              ElementsAre(Field::kName, Field::kEmail, Field::kUsername,
                          Field::kPhoneNumber, Field::kPicture));
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
