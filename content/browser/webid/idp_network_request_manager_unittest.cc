// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <array>
#include <string>
#include <tuple>
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;
using AccountsResponse = content::IdpNetworkRequestManager::AccountsResponse;
using AccountsRequestCallback =
    content::IdpNetworkRequestManager::AccountsRequestCallback;

namespace content {

namespace {

const char kTestIdpUrl[] = "https://idp.test";
const char kTestRpUrl[] = "https://rp.test";
const char kTestAccountsEndpoint[] = "https://idp.test/accounts_endpoint";

class IdpNetworkRequestManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<IdpNetworkRequestManager>(
        GURL(kTestIdpUrl), url::Origin::Create(GURL(kTestRpUrl)),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  void TearDown() override { manager_.reset(); }

  std::tuple<AccountsResponse, AccountList>
  SendAccountsRequestAndWaitForResponse(const char* test_accounts) {
    GURL accounts_endpoint(kTestAccountsEndpoint);
    test_url_loader_factory().AddResponse(accounts_endpoint.spec(),
                                          test_accounts);

    base::RunLoop run_loop;
    AccountsResponse parsed_accounts_response;
    AccountList parsed_accounts;
    auto callback = base::BindLambdaForTesting(
        [&](AccountsResponse response, const AccountList& accounts) {
          parsed_accounts_response = response;
          parsed_accounts = accounts;
          run_loop.Quit();
        });
    manager().SendAccountsRequest(accounts_endpoint, std::move(callback));
    run_loop.Run();

    return {parsed_accounts_response, parsed_accounts};
  }

  IdpNetworkRequestManager& manager() { return *manager_; }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdpNetworkRequestManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
};

TEST_F(IdpNetworkRequestManagerTest, ParseAccountEmpty) {
  const auto* test_empty_account_json = R"({
  "accounts" : []
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_empty_account_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountSingle) {
  const auto* test_single_account_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    }
  ]
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_single_account_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMultiple) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    },
    {
      "sub" : "5678",
      "email": "sam@idp.test",
      "name": "Sam G. Test",
      "given_name": "Sam",
      "picture": "https://idp.test/profile/2"
    }
  ]
  })";
  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ(2UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].sub);
  EXPECT_EQ("5678", accounts[1].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountOptionalFields) {
  // given_name and picture fields are optional
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "sub" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
  EXPECT_EQ("1234", accounts[0].sub);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountRequiredFields) {
  {
    const auto* test_accounts_missing_sub_json = R"({"accounts" : [{
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_sub_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_email_json = R"({"accounts" : [{
      "sub" : "1234",
      "name": "Ken R. Example"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_email_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
  {
    const auto* test_accounts_missing_name_json = R"({"accounts" : [{
      "sub" : "1234",
      "email": "ken@idp.test"
    }]})";
    AccountsResponse accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_accounts_missing_name_json);

    EXPECT_EQ(AccountsResponse::kSuccess, accounts_response);
    EXPECT_TRUE(accounts.empty());
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountUnicode) {
  auto TestAccountWithKeyValue = [](const std::string& key,
                                    const std::string& value) {
    const auto* json = R"({
     "accounts" : [
        {
          "sub" : "1234",
          "email": "ken@idp.test",
          "%s": "%s"
        }
      ]
    })";
    return base::StringPrintf(json, key.c_str(), value.c_str());
  };

  std::array<std::string, 3> test_values{"ascii", "🦖", "مجید"};

  for (auto& test_value : test_values) {
    const auto& accounts_json = TestAccountWithKeyValue("name", test_value);

    AccountsResponse accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(accounts_json.c_str());

    EXPECT_EQ(1UL, accounts.size());
    EXPECT_EQ(test_value, accounts[0].name);
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountInvalid) {
  const auto* test_invalid_account_json = "{}";

  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(AccountsResponse::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMalformed) {
  const auto* test_invalid_account_json = "malformed_json";

  AccountsResponse accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(AccountsResponse::kInvalidResponseError, accounts_response);
  EXPECT_TRUE(accounts.empty());
}

}  // namespace

}  // namespace content
