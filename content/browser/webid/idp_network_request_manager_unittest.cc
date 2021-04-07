// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <array>
#include <string>
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;

namespace content {

namespace {

base::Value CreateTestAccount(const std::string& sub) {
  base::Value::DictStorage storage;
  storage.emplace("sub", sub);
  storage.emplace("email", "email@idp.test");
  storage.emplace("name", "Ken R. Example");
  storage.emplace("given_name", "Ken");
  storage.emplace("picture", "https://idp.test/profile");

  return base::Value(storage);
}

TEST(AccountsParseTest, EmptyAccounts) {
  base::ListValue empty_list;
  AccountList parsed_accounts;
  EXPECT_TRUE(
      IdpNetworkRequestManager::ParseAccounts(&empty_list, parsed_accounts));
  EXPECT_TRUE(parsed_accounts.empty());
}

TEST(AccountsParseTest, SingleAccount) {
  base::Value::ListStorage accounts;
  accounts.emplace_back(CreateTestAccount("1234"));
  base::Value single_account_value(accounts);
  AccountList parsed_accounts;
  EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&single_account_value,
                                                      parsed_accounts));
  EXPECT_EQ(1UL, parsed_accounts.size());
  EXPECT_EQ("1234", parsed_accounts[0].sub);
}

TEST(AccountsParseTest, MultipleAccounts) {
  base::Value::ListStorage accounts;
  accounts.emplace_back(CreateTestAccount("1234"));
  accounts.emplace_back(CreateTestAccount("5678"));
  base::Value single_account_value(accounts);
  AccountList parsed_accounts;
  EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&single_account_value,
                                                      parsed_accounts));
  EXPECT_EQ(2UL, parsed_accounts.size());
  EXPECT_EQ("1234", parsed_accounts[0].sub);
  EXPECT_EQ("5678", parsed_accounts[1].sub);
}

TEST(AccountsParseTest, OptionalFields) {
  auto account = CreateTestAccount("1234");
  account.RemoveKey("given_name");
  account.RemoveKey("family_name");
  account.RemoveKey("picture");
  // given_name and picture are optional
  base::Value::ListStorage accounts;
  accounts.emplace_back(std::move(account));
  base::Value single_account_value(accounts);

  AccountList parsed_accounts;
  EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&single_account_value,
                                                      parsed_accounts));
  EXPECT_EQ(1UL, parsed_accounts.size());
  EXPECT_EQ("1234", parsed_accounts[0].sub);
}

TEST(AccountsParseTest, RequiredFields) {
  auto TestAccountWithMissingField = [](const std::string& removed_key) {
    auto account = CreateTestAccount("1234");
    account.RemoveKey(removed_key);
    base::Value::ListStorage accounts;
    accounts.emplace_back(std::move(account));
    return base::Value(accounts);
  };

  {
    auto account_value = TestAccountWithMissingField("sub");
    AccountList parsed_accounts;
    EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&account_value,
                                                        parsed_accounts));
    EXPECT_TRUE(parsed_accounts.empty());
  }
  {
    auto account_value = TestAccountWithMissingField("email");
    AccountList parsed_accounts;
    EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&account_value,
                                                        parsed_accounts));
    EXPECT_TRUE(parsed_accounts.empty());
  }
  {
    auto account_value = TestAccountWithMissingField("name");
    AccountList parsed_accounts;
    EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&account_value,
                                                        parsed_accounts));
    EXPECT_TRUE(parsed_accounts.empty());
  }
}

TEST(AccountsParseTest, Unicode) {
  auto TestAccountWithKeyValue = [](const std::string& key,
                                    const std::string& value) {
    auto account = CreateTestAccount("1234");
    account.SetStringKey(key, value);
    base::Value::ListStorage accounts;
    accounts.emplace_back(std::move(account));
    return base::Value(accounts);
  };

  std::array<std::string, 3> test_values{"ascii", "🦖", "مجید"};

  for (auto& test_value : test_values) {
    const auto& account_value = TestAccountWithKeyValue("name", test_value);
    AccountList parsed_accounts;
    EXPECT_TRUE(IdpNetworkRequestManager::ParseAccounts(&account_value,
                                                        parsed_accounts));
    EXPECT_EQ(1UL, parsed_accounts.size());
    EXPECT_EQ(test_value, parsed_accounts[0].name);
  }
}

TEST(AccountsParseTest, InvalidAccounts) {
  const base::DictionaryValue dictionary_value;
  AccountList parsed_accounts;
  EXPECT_FALSE(IdpNetworkRequestManager::ParseAccounts(&dictionary_value,
                                                       parsed_accounts));
  EXPECT_TRUE(parsed_accounts.empty());
}

}  // namespace

}  // namespace content
