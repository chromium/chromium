// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_accounts_parser.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::webid {

class IdpAccountsParserTest : public testing::Test {};

TEST_F(IdpAccountsParserTest, ParseValidAccounts) {
  const char json[] = R"({
    "accounts": [
      {
        "id": "123",
        "email": "ken@example.com",
        "name": "Ken",
        "given_name": "Ken",
        "picture": "https://example.com/ken.png"
      }
    ]
  })";

  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ("123", (*result)[0]->id);
  EXPECT_EQ("ken@example.com", (*result)[0]->email);
  EXPECT_EQ("Ken", (*result)[0]->name);
  EXPECT_EQ("Ken", (*result)[0]->given_name);
  EXPECT_EQ(GURL("https://example.com/ken.png"), (*result)[0]->picture);
}

TEST_F(IdpAccountsParserTest, ParseMissingAccountsKey) {
  const char json[] = R"({"other": "data"})";
  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      IdpNetworkRequestManager::AccountsResponseInvalidReason::kNoAccountsKey,
      result.error());
}

TEST_F(IdpAccountsParserTest, ParseEmptyAccountsList) {
  const char json[] = R"({"accounts": []})";
  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(IdpNetworkRequestManager::AccountsResponseInvalidReason::
                kAccountListIsEmpty,
            result.error());
}

TEST_F(IdpAccountsParserTest, ParseDuplicateAccountIds) {
  const char json[] = R"({
    "accounts": [
      {"id": "123", "email": "a@example.com", "name": "A"},
      {"id": "123", "email": "b@example.com", "name": "B"}
    ]
  })";
  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(IdpNetworkRequestManager::AccountsResponseInvalidReason::
                kAccountsShareSameId,
            result.error());
}

TEST_F(IdpAccountsParserTest, ParseAccountMissingRequiredFields) {
  const char json[] = R"({
    "accounts": [
      {"email": "ken@example.com", "name": "Ken"}
    ]
  })";
  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(IdpNetworkRequestManager::AccountsResponseInvalidReason::
                kAccountMissesRequiredField,
            result.error());
}

TEST_F(IdpAccountsParserTest, ParseAccountNotADict) {
  const char json[] = R"({
    "accounts": ["not_a_dict"]
  })";
  auto dict = base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());

  auto result = IdpAccountsParser::ParseAccounts(*dict);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(IdpNetworkRequestManager::AccountsResponseInvalidReason::
                kAccountIsNotDict,
            result.error());
}

}  // namespace content::webid
