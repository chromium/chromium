// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using AccountList = content::IdpNetworkRequestManager::AccountList;
using IdpClientMetadata = content::IdpNetworkRequestManager::ClientMetadata;
using Endpoints = content::IdpNetworkRequestManager::Endpoints;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using AccountsRequestCallback =
    content::IdpNetworkRequestManager::AccountsRequestCallback;
using LoginState = content::IdentityRequestAccount::LoginState;
using AccountsResponseInvalidReason =
    content::IdpNetworkRequestManager::AccountsResponseInvalidReason;

namespace content {

namespace {

// Values for testing. Real minimum and ideal sizes are different.
const int kTestIdpBrandIconMinimumSize = 16;
const int kTestIdpBrandIconIdealSize = 32;

const char kTestIdpUrl[] = "https://idp.test";
const char kTestRpUrl[] = "https://rp.test";
const char kTestWellKnownUrl[] = "https://idp.test/.well-known/web-identity";
const char kTestConfigUrl[] = "https://idp.test/fedcm.json";
const char kTestAccountsEndpoint[] = "https://idp.test/accounts_endpoint";
const char kTestTokenEndpoint[] = "https://idp.test/token_endpoint";
const char kTestClientMetadataEndpoint[] =
    "https://idp.test/client_metadata_endpoint";

const char kSingleAccountEndpointValidJson[] = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    }
  ]
  })";

// Replaces the first line with the passed-in JSON key in `input` with
// `new_line`.
std::string ReplaceFirstLineWithKeyFromJson(const std::string& key,
                                            const std::string& new_line,
                                            const std::string& input,
                                            bool replace_all) {
  std::string pattern = R"(^[ ]*")" + key + R"(".*$)";
  std::string result = input;
  RE2::Options options;
  options.set_posix_syntax(true);
  options.set_one_line(false);
  RE2 re2(pattern, options);
  if (replace_all)
    RE2::GlobalReplace(&result, re2, new_line);
  else
    RE2::Replace(&result, re2, new_line);
  return result;
}

// Removes all lines with the passed-in JSON key in `input`.
std::string RemoveAllLinesWithKeyFromJson(const std::string& key,
                                          const std::string& input) {
  return ReplaceFirstLineWithKeyFromJson(key, "", input, /*replace_all=*/true);
}

url::Origin GetOriginHeader(const network::ResourceRequest& request) {
  std::string origin;
  request.headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin);
  return url::Origin::Create(GURL(origin));
}

class IdpNetworkRequestManagerTest : public ::testing::Test {
 public:
  std::unique_ptr<IdpNetworkRequestManager> CreateTestManager() {
    return std::make_unique<IdpNetworkRequestManager>(
        url::Origin::Create(GURL(kTestRpUrl)),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New());
  }

  void AddResponse(const GURL& url,
                   net::HttpStatusCode http_status,
                   const std::string mime_type,
                   const std::string& content) {
    auto head = network::mojom::URLResponseHead::New();
    std::string raw_header = "HTTP/1.1 " + base::NumberToString(http_status) +
                             " " + net::GetHttpReasonPhrase(http_status) +
                             "\n"
                             "Content-type: " +
                             mime_type + "\n\n";
    head->headers = net::HttpResponseHeaders::TryToCreate(raw_header);
    test_url_loader_factory().AddResponse(url, std::move(head), content,
                                          network::URLLoaderCompletionStatus());
  }

  std::tuple<FetchStatus, std::set<GURL>>
  SendWellKnownRequestAndWaitForResponse(
      const char* test_data,
      net::HttpStatusCode http_status = net::HTTP_OK,
      const std::string& mime_type = "application/json") {
    GURL well_known_url(kTestWellKnownUrl);
    AddResponse(well_known_url, http_status, mime_type, test_data);

    base::RunLoop run_loop;
    FetchStatus parsed_fetch_status;
    std::set<GURL> parsed_urls;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus fetch_status, const std::set<GURL>& urls) {
          parsed_fetch_status = fetch_status;
          parsed_urls = urls;
          run_loop.Quit();
        });

    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->FetchWellKnown(GURL(kTestIdpUrl), std::move(callback));
    run_loop.Run();

    return {parsed_fetch_status, parsed_urls};
  }

  std::tuple<FetchStatus, IdentityProviderMetadata>
  SendConfigRequestAndWaitForResponse(
      const char* test_data,
      net::HttpStatusCode http_status = net::HTTP_OK,
      const std::string& mime_type = "application/json") {
    GURL config_url(kTestConfigUrl);
    AddResponse(config_url, http_status, mime_type, test_data);

    base::RunLoop run_loop;
    FetchStatus parsed_fetch_status;
    IdentityProviderMetadata parsed_idp_metadata;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus fetch_status, Endpoints endpoints,
            IdentityProviderMetadata idp_metadata) {
          parsed_fetch_status = fetch_status;
          parsed_idp_metadata = std::move(idp_metadata);
          run_loop.Quit();
        });

    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->FetchConfig(GURL(kTestConfigUrl), kTestIdpBrandIconIdealSize,
                         kTestIdpBrandIconMinimumSize, std::move(callback));
    run_loop.Run();

    return {parsed_fetch_status, parsed_idp_metadata};
  }

  std::tuple<FetchStatus, AccountList> SendAccountsRequestAndWaitForResponse(
      const std::string& test_accounts,
      const char* client_id = "",
      net::HttpStatusCode response_code = net::HTTP_OK,
      const std::string& mime_type = "application/json") {
    GURL accounts_endpoint(kTestAccountsEndpoint);
    AddResponse(accounts_endpoint, response_code, mime_type, test_accounts);

    base::RunLoop run_loop;
    FetchStatus parsed_accounts_response;
    AccountList parsed_accounts;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus response, AccountList accounts) {
          parsed_accounts_response = response;
          parsed_accounts = accounts;
          run_loop.Quit();
        });

    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->SendAccountsRequest(accounts_endpoint, client_id,
                                 std::move(callback));
    run_loop.Run();

    return {parsed_accounts_response, parsed_accounts};
  }

  std::tuple<FetchStatus, std::string> SendTokenRequestAndWaitForResponse(
      const char* account,
      const char* request,
      net::HttpStatusCode http_status = net::HTTP_OK,
      const std::string& mime_type = "application/json") {
    const char response[] = R"({"token": "token"})";
    GURL token_endpoint(kTestTokenEndpoint);
    AddResponse(token_endpoint, http_status, mime_type, response);

    FetchStatus fetch_status;
    std::string token;
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus status, const std::string& token_response) {
          fetch_status = status;
          token = token_response;
          run_loop.Quit();
        });

    auto on_continue =
        base::BindLambdaForTesting([&](FetchStatus status, const GURL& url) {});

    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->SendTokenRequest(token_endpoint, account, request,
                              std::move(callback), std::move(on_continue));
    run_loop.Run();
    return {fetch_status, token};
  }

  IdpClientMetadata SendClientMetadataRequestAndWaitForResponse(
      const char* client_id,
      const std::string& response = R"({})") {
    GURL client_id_endpoint(kTestClientMetadataEndpoint);
    AddResponse(GURL(client_id_endpoint.spec() + "?client_id=" + client_id),
                net::HTTP_OK, "application/json", response);

    IdpClientMetadata data;
    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&](FetchStatus status, IdpClientMetadata metadata) {
          data = metadata;
          run_loop.Quit();
        });

    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->FetchClientMetadata(client_id_endpoint, client_id,
                                 std::move(callback));
    run_loop.Run();
    return data;
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester_;
};

TEST_F(IdpNetworkRequestManagerTest, ParseAccountEmpty) {
  const auto* test_empty_account_json = R"({
  "accounts" : []
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_empty_account_json);

  EXPECT_EQ(ParseStatus::kEmptyListError, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts.empty());

  histogram_tester()->ExpectUniqueSample(
      "Blink.FedCm.Status.AccountsResponseInvalidReason",
      AccountsResponseInvalidReason::kAccountListIsEmpty, 1);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountSingle) {
  const auto* test_single_account_json = kSingleAccountEndpointValidJson;

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_single_account_json);

  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMultiple) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "given_name": "Ken",
      "picture": "https://idp.test/profile/1"
    },
    {
      "id" : "5678",
      "email": "sam@idp.test",
      "name": "Sam G. Test",
      "given_name": "Sam",
      "picture": "https://idp.test/profile/2"
    }
  ]
  })";
  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_EQ(2UL, accounts.size());
  EXPECT_EQ("1234", accounts[0].id);
  EXPECT_EQ("5678", accounts[1].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountOptionalFields) {
  // given_name and picture fields are optional
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_EQ("1234", accounts[0].id);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountRequiredFields) {
  {
    base::HistogramTester histogram_tester;
    std::string test_account_missing_account_id_json =
        RemoveAllLinesWithKeyFromJson("id", kSingleAccountEndpointValidJson);
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(
            test_account_missing_account_id_json);

    EXPECT_EQ(ParseStatus::kInvalidResponseError,
              accounts_response.parse_status);
    EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
    EXPECT_TRUE(accounts.empty());
    histogram_tester.ExpectUniqueSample(
        "Blink.FedCm.Status.AccountsResponseInvalidReason",
        AccountsResponseInvalidReason::kAccountMissesRequiredField, 1);
  }
  {
    base::HistogramTester histogram_tester;
    std::string test_account_missing_email_json =
        RemoveAllLinesWithKeyFromJson("email", kSingleAccountEndpointValidJson);
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_account_missing_email_json);

    EXPECT_EQ(ParseStatus::kInvalidResponseError,
              accounts_response.parse_status);
    EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
    EXPECT_TRUE(accounts.empty());
    histogram_tester.ExpectUniqueSample(
        "Blink.FedCm.Status.AccountsResponseInvalidReason",
        AccountsResponseInvalidReason::kAccountMissesRequiredField, 1);
  }
  {
    base::HistogramTester histogram_tester;
    std::string test_account_missing_name_json =
        RemoveAllLinesWithKeyFromJson("name", kSingleAccountEndpointValidJson);
    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(test_account_missing_name_json);

    EXPECT_EQ(ParseStatus::kInvalidResponseError,
              accounts_response.parse_status);
    EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
    EXPECT_TRUE(accounts.empty());
    histogram_tester.ExpectUniqueSample(
        "Blink.FedCm.Status.AccountsResponseInvalidReason",
        AccountsResponseInvalidReason::kAccountMissesRequiredField, 1);
  }
}

// Test that parsing accounts fails if two accounts have the same account id.
TEST_F(IdpNetworkRequestManagerTest, ParseAccountDuplicateIds) {
  const auto* accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    },
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(accounts_json);

  EXPECT_EQ(ParseStatus::kInvalidResponseError, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts.empty());
  histogram_tester()->ExpectUniqueSample(
      "Blink.FedCm.Status.AccountsResponseInvalidReason",
      AccountsResponseInvalidReason::kAccountsShareSameId, 1);

  // Test that JSON is valid with exception of duplicate id.
  std::string accounts_json_different_account_ids =
      ReplaceFirstLineWithKeyFromJson("id", R"("id": "5678",)", accounts_json,
                                      /*replace_all=*/false);

  std::tie(accounts_response, accounts) = SendAccountsRequestAndWaitForResponse(
      accounts_json_different_account_ids);
  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountPictureUrl) {
  const auto* test_accounts_json = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "picture": "https://idp.test/profile/1234"
    },
    {
      "id" : "567",
      "email": "sam@idp.test",
      "name": "Sam R. Example",
      "picture": "invalid_url"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts[0].picture.is_valid());
  EXPECT_EQ(GURL("https://idp.test/profile/1234"), accounts[0].picture);
  EXPECT_FALSE(accounts[1].picture.is_valid());
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountUnicode) {
  auto TestAccountWithKeyValue = [](const std::string& key,
                                    const std::string& value) {
    const auto* json = R"({
     "accounts" : [
        {
          "id" : "1234",
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

    FetchStatus accounts_response;
    AccountList accounts;
    std::tie(accounts_response, accounts) =
        SendAccountsRequestAndWaitForResponse(accounts_json.c_str());

    EXPECT_EQ(1UL, accounts.size());
    EXPECT_EQ(test_value, accounts[0].name);
  }
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountInvalid) {
  const auto* test_invalid_account_json = "{}";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(ParseStatus::kInvalidResponseError, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts.empty());
  histogram_tester()->ExpectUniqueSample(
      "Blink.FedCm.Status.AccountsResponseInvalidReason",
      AccountsResponseInvalidReason::kNoAccountsKey, 1);
}

TEST_F(IdpNetworkRequestManagerTest, ParseAccountMalformed) {
  const auto* test_invalid_account_json = "malformed_json";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_invalid_account_json);

  EXPECT_EQ(ParseStatus::kInvalidResponseError, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts.empty());
  histogram_tester()->ExpectUniqueSample(
      "Blink.FedCm.Status.AccountsResponseInvalidReason",
      AccountsResponseInvalidReason::kResponseIsNotJsonOrDict, 1);
}

TEST_F(IdpNetworkRequestManagerTest, ComputeWellKnownUrl) {
  EXPECT_EQ("https://localhost:8000/.well-known/web-identity",
            IdpNetworkRequestManager::ComputeWellKnownUrl(
                GURL("https://localhost:8000/test/"))
                ->spec());

  EXPECT_EQ("https://google.com/.well-known/web-identity",
            IdpNetworkRequestManager::ComputeWellKnownUrl(
                GURL("https://www.google.com:8000/test/"))
                ->spec());

  EXPECT_EQ(absl::nullopt, IdpNetworkRequestManager::ComputeWellKnownUrl(
                               GURL("https://192.101.0.1/test/")));
}

// Test that IdpNetworkRequestManager::FetchWellKnown() fails when the
// identity provider domain is empty.
TEST_F(IdpNetworkRequestManagerTest, FetchWellKnownIllegalDomainFails) {
  GURL illegal_idp_url("https://192.101.0.1/test/");

  network::TestURLLoaderFactory test_url_loader_factory;
  auto network_manager = std::make_unique<IdpNetworkRequestManager>(
      url::Origin::Create(GURL(kTestRpUrl)),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New());

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](FetchStatus fetch_status, const std::set<GURL>& urls) {
        EXPECT_EQ(ParseStatus::kHttpNotFoundError, fetch_status.parse_status);
        // We receive OK here because
        // IdpNetworkRequestManager::ComputeWellKnownUrl() fails.
        EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
        run_loop.Quit();
      });
  network_manager->FetchWellKnown(illegal_idp_url, std::move(callback));
  run_loop.Run();

  // Well-known download should not have been attempted.
  EXPECT_EQ(0, test_url_loader_factory.NumPending());
}

TEST_F(IdpNetworkRequestManagerTest, ParseWellKnown) {
  FetchStatus fetch_status;
  std::set<GURL> urls;

  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["https://idp.test/fedcm.json"]
  })");
  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(std::set<GURL>{GURL("https://idp.test/fedcm.json")}, urls);

  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["https://idp.test/path/fedcm.json"]
  })");
  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(std::set<GURL>{GURL("https://idp.test/path/fedcm.json")}, urls);

  // Value not a list
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": "https://idp.test/fedcm.json"
  })");
  EXPECT_EQ(ParseStatus::kInvalidResponseError, fetch_status.parse_status);

  // Toplevel not a dictionary
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"(
  ["https://idp.test/fedcm.json"]
  )");
  EXPECT_EQ(ParseStatus::kInvalidResponseError, fetch_status.parse_status);

  // Incorrect key
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "providers": ["https://idp.test/fedcm.json"]
  })");
  EXPECT_EQ(ParseStatus::kInvalidResponseError, fetch_status.parse_status);

  // Array entry not a string
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": [1]
  })");
  EXPECT_EQ(ParseStatus::kInvalidResponseError, fetch_status.parse_status);

  // Relative URLs
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["/fedcm.json"]
  })");
  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(std::set<GURL>{GURL("https://idp.test/fedcm.json")}, urls);

  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["fedcm.json"]
  })");
  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(std::set<GURL>{GURL("https://idp.test/.well-known/fedcm.json")},
            urls);

  // Empty well known list
  std::tie(fetch_status, urls) = SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": []
  })");
  EXPECT_EQ(ParseStatus::kEmptyListError, fetch_status.parse_status);
}

// Test that the "alpha" value in the "branding" JSON is ignored.
TEST_F(IdpNetworkRequestManagerTest, ParseConfigBrandingRemoveAlpha) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "#20202020"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(test_json);

  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ(SkColorSetARGB(0xff, 0x20, 0x20, 0x20),
            idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseConfigBrandingInvalidColor) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "fake_color"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(test_json);

  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseConfigIgnoreInsufficientContrastTextColor) {
  const char test_json[] = R"({
  "branding" : {
    "background_color": "#000000",
    "color": "#010101"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(test_json);

  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest,
       ParseConfigBrandingIgnoreCustomTextColorNoCustomBackgroundColor) {
  const char test_json[] = R"({
  "branding" : {
    "color": "blue"
  }
  })";

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(test_json);

  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_background_color);
  EXPECT_EQ(absl::nullopt, idp_metadata.brand_text_color);
}

TEST_F(IdpNetworkRequestManagerTest, ParseConfigBrandingSelectBestSize) {
  const char test_json[] = R"({
  "branding" : {
    "icons": [
      {
        "url": "https://example.com/10.png",
        "size": 10
      },
      {
        "url": "https://example.com/16.png",
        "size": 16
      },
      {
        "url": "https://example.com/31.png",
        "size": 31
      },
      {
        "url": "https://example.com/32.png",
        "size": 32
      },
      {
        "url": "https://example.com/33.png",
        "size": 33
      }
    ]
  }
  })";

  ASSERT_EQ(32, kTestIdpBrandIconIdealSize);

  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(test_json);

  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ("https://example.com/32.png", idp_metadata.brand_icon_url.spec());
}

// Test that the icon is rejected if there is an explicit brand icon size in the
// config and it is smaller than the `idp_brand_icon_minimum_size` parameter
// passed to IdpNetworkRequestManager::FetchConfig().
TEST_F(IdpNetworkRequestManagerTest, ParseConfigBrandingMinSize) {
  ASSERT_EQ(16, kTestIdpBrandIconMinimumSize);

  {
    const char test_json[] = R"({
    "branding" : {
      "icons": [
        {
          "url": "https://example.com/15.png",
          "size": 15
        }
      ]
    }
    })";

    FetchStatus fetch_status;
    IdentityProviderMetadata idp_metadata;
    std::tie(fetch_status, idp_metadata) =
        SendConfigRequestAndWaitForResponse(test_json);

    EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
    EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
    EXPECT_EQ(GURL(), idp_metadata.brand_icon_url);
  }

  {
    const char test_json[] = R"({
    "branding" : {
      "icons": [
        {
          "url": "https://example.com/16.png",
          "size": 16
        }
      ]
    }
    })";

    FetchStatus fetch_status;
    IdentityProviderMetadata idp_metadata;
    std::tie(fetch_status, idp_metadata) =
        SendConfigRequestAndWaitForResponse(test_json);

    EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
    EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
    EXPECT_EQ("https://example.com/16.png", idp_metadata.brand_icon_url.spec());
  }
}

// Tests that we send the correct origin for account requests.
TEST_F(IdpNetworkRequestManagerTest, AccountRequestOrigin) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1234",
      "email": "ken@idp.test",
      "name": "Ken R. Example"
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json);

  ASSERT_TRUE(called);
  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
}

// Verifies that we correctly check the signed-in status.
TEST_F(IdpNetworkRequestManagerTest, AccountSignedInStatus) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "approved_clients": ["xxx"]
    },
    {
      "id" : "2",
      "email": "jim@idp.test",
      "name": "Jim R. Example",
      "approved_clients": []
    },
    {
      "id" : "3",
      "email": "rashida@idp.test",
      "name": "Rashida R. Example",
      "approved_clients": ["yyy"]
    },
    {
      "id" : "4",
      "email": "wei@idp.test",
      "name": "Wei R. Example"
    },
    {
      "id" : "5",
      "email": "hans@idp.test",
      "name": "Hans R. Example",
      "approved_clients": ["xxx", "yyy"]
    }
  ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json, "xxx");

  EXPECT_TRUE(called);
  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  ASSERT_EQ(5ul, accounts.size());
  ASSERT_TRUE(accounts[0].login_state.has_value());
  EXPECT_EQ(LoginState::kSignIn, *accounts[0].login_state);
  ASSERT_TRUE(accounts[1].login_state.has_value());
  EXPECT_EQ(LoginState::kSignUp, *accounts[1].login_state);
  ASSERT_TRUE(accounts[2].login_state.has_value());
  EXPECT_EQ(LoginState::kSignUp, *accounts[2].login_state);
  EXPECT_FALSE(accounts[3].login_state.has_value());
  ASSERT_TRUE(accounts[4].login_state.has_value());
  EXPECT_EQ(LoginState::kSignIn, *accounts[4].login_state);
}

// Tests the token request implementation.
TEST_F(IdpNetworkRequestManagerTest, TokenRequest) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestTokenEndpoint), request.url);
        EXPECT_FALSE(request.referrer.is_valid());
        EXPECT_EQ(url::Origin::Create(GURL(kTestRpUrl)),
                  GetOriginHeader(request));

        // Check that the request body is correct (should be "request")
        ASSERT_NE(request.request_body, nullptr);
        ASSERT_EQ(1ul, request.request_body->elements()->size());
        const network::DataElement& elem =
            request.request_body->elements()->at(0);
        ASSERT_EQ(network::DataElement::Tag::kBytes, elem.type());
        const network::DataElementBytes& byte_elem =
            elem.As<network::DataElementBytes>();
        EXPECT_EQ("request", byte_elem.AsStringPiece());
      });
  test_url_loader_factory().SetInterceptor(interceptor);
  std::string token;
  FetchStatus fetch_status;
  std::tie(fetch_status, token) =
      SendTokenRequestAndWaitForResponse("account", "request");
  ASSERT_TRUE(called);
  EXPECT_EQ(ParseStatus::kSuccess, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  ASSERT_EQ("token", token);
}

// Tests the client metadata implementation.
TEST_F(IdpNetworkRequestManagerTest, ClientMetadata) {
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        std::string url_string =
            std::string(kTestClientMetadataEndpoint) + "?client_id=xxx";
        EXPECT_EQ(GURL(url_string), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
        EXPECT_EQ(url::Origin::Create(GURL(kTestRpUrl)),
                  GetOriginHeader(request));
      });
  test_url_loader_factory().SetInterceptor(interceptor);
  IdpClientMetadata data = SendClientMetadataRequestAndWaitForResponse("xxx");
  ASSERT_TRUE(called);
  ASSERT_EQ(GURL(), data.privacy_policy_url);
  ASSERT_EQ(GURL(), data.terms_of_service_url);
}

// Tests that we correctly records metrics regarding approved_clients.
TEST_F(IdpNetworkRequestManagerTest, RecordApprovedClientsMetrics) {
  base::HistogramTester histogram_tester;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(GURL(kTestAccountsEndpoint), request.url);
        EXPECT_EQ(request.request_body, nullptr);
        EXPECT_FALSE(request.referrer.is_valid());
        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kOrigin));
      });
  test_url_loader_factory().SetInterceptor(interceptor);

  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "approved_clients": []
    },
    {
      "id" : "2",
      "email": "jim@idp.test",
      "name": "Jim R. Example",
      "approved_clients": ["xxx"]
    },
    {
      "id" : "3",
      "email": "rashida@idp.test",
      "name": "Rashida R. Example",
      "approved_clients": ["xxx", "yyy"]
    },
    {
      "id" : "4",
      "email": "wei@idp.test",
      "name": "Wei R. Example"
    }
   ]
  })";

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) =
      SendAccountsRequestAndWaitForResponse(test_accounts_json, "xxx");

  EXPECT_TRUE(called);
  EXPECT_EQ(ParseStatus::kSuccess, accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  ASSERT_EQ(4ul, accounts.size());

  histogram_tester.ExpectTotalCount("Blink.FedCm.ApprovedClientsExistence", 4);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsExistence", 1,
                                     3);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsExistence", 0,
                                     1);

  histogram_tester.ExpectTotalCount("Blink.FedCm.ApprovedClientsSize", 3);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 0, 1);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 1, 1);
  histogram_tester.ExpectBucketCount("Blink.FedCm.ApprovedClientsSize", 2, 1);
}

// Test that the callback is not called after IdpNetworkRequestManager is
// destroyed.
TEST_F(IdpNetworkRequestManagerTest, DontCallCallbackAfterManagerDeletion) {
  const char test_accounts_json[] = R"({
  "accounts" : [
    {
      "id" : "1",
      "email": "ken@idp.test",
      "name": "Ken R. Example",
      "approved_clients": []
    }
   ]
  })";

  GURL accounts_endpoint(kTestAccountsEndpoint);
  AddResponse(accounts_endpoint, net::HTTP_OK, "application/json",
              test_accounts_json);

  bool callback_called = false;
  auto callback = base::BindLambdaForTesting(
      [&callback_called](FetchStatus response, AccountList accounts) {
        callback_called = true;
      });

  {
    std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
    manager->SendAccountsRequest(accounts_endpoint, /*client_id=*/"",
                                 std::move(callback));
    // Destroy `manager`.
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(callback_called);
}

TEST_F(IdpNetworkRequestManagerTest, ErrorFetchingWellKnown) {
  FetchStatus fetch_status;
  std::set<GURL> urls;
  std::tie(fetch_status, urls) =
      SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["https://idp.test/fedcm.json"]
  })",
                                             net::HTTP_REQUEST_TIMEOUT);
  EXPECT_EQ(ParseStatus::kNoResponseError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_REQUEST_TIMEOUT, fetch_status.response_code);
  EXPECT_EQ(std::set<GURL>{}, urls);
}

TEST_F(IdpNetworkRequestManagerTest, ErrorFetchingConfig) {
  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) =
      SendConfigRequestAndWaitForResponse(R"({
  "branding" : {
    "color": "blue"
  }
  })",
                                          net::HTTP_NOT_FOUND);
  EXPECT_EQ(ParseStatus::kHttpNotFoundError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_NOT_FOUND, fetch_status.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, ErrorFetchingAccounts) {
  FetchStatus fetch_status;
  AccountList accounts;
  std::tie(fetch_status, accounts) =
      SendAccountsRequestAndWaitForResponse(R"({
  "accounts" : []
  })",
                                            "", net::HTTP_BAD_REQUEST);
  EXPECT_EQ(ParseStatus::kNoResponseError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_BAD_REQUEST, fetch_status.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, ErrorFetchingToken) {
  std::string token;
  FetchStatus fetch_status;
  std::tie(fetch_status, token) = SendTokenRequestAndWaitForResponse(
      "account", "request", net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_EQ("", token);
  EXPECT_EQ(ParseStatus::kNoResponseError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, fetch_status.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, FetchClientMetadataValidUrls) {
  // Both HTTPS and HTTP URLs are allowed.
  const std::string privacy_policy_url = "https://privacy.policy";
  const std::string terms_of_service_url = "http://terms.of.service";

  IdpClientMetadata data = SendClientMetadataRequestAndWaitForResponse(
      /*client_id=*/"123", R"({"privacy_policy_url": ")" + privacy_policy_url +
                               R"(", "terms_of_service_url": ")" +
                               terms_of_service_url + R"("})");
  ASSERT_EQ(GURL(privacy_policy_url), data.privacy_policy_url);
  ASSERT_EQ(GURL(terms_of_service_url), data.terms_of_service_url);
}

TEST_F(IdpNetworkRequestManagerTest, FetchClientMetadataInvalidUrls) {
  // Non-HTTP(S) URLs should not be allowed.
  const std::string privacy_policy_url = "chrome://settings";
  const std::string terms_of_service_url = "file:///Users/you/file.html";

  IdpClientMetadata data = SendClientMetadataRequestAndWaitForResponse(
      /*client_id=*/"123", R"({"privacy_policy_url": ")" + privacy_policy_url +
                               R"(", "terms_of_service_url": ")" +
                               terms_of_service_url + R"("})");
  ASSERT_EQ(GURL(), data.privacy_policy_url);
  ASSERT_EQ(GURL(), data.terms_of_service_url);
}

TEST_F(IdpNetworkRequestManagerTest, WellKnownWrongMimeType) {
  FetchStatus fetch_status;
  std::set<GURL> urls;
  std::tie(fetch_status, urls) =
      SendWellKnownRequestAndWaitForResponse(R"({
  "provider_urls": ["https://idp.test/fedcm.json"]
  })",
                                             net::HTTP_OK, "text/html");
  EXPECT_EQ(ParseStatus::kInvalidContentTypeError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
  EXPECT_EQ(std::set<GURL>{}, urls);
}

TEST_F(IdpNetworkRequestManagerTest, ConfigWrongMimeType) {
  FetchStatus fetch_status;
  IdentityProviderMetadata idp_metadata;
  std::tie(fetch_status, idp_metadata) = SendConfigRequestAndWaitForResponse(
      R"({"branding" : { "color": "blue" } })", net::HTTP_OK, "text/html");
  EXPECT_EQ(ParseStatus::kInvalidContentTypeError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, AccountsWrongMimeType) {
  const auto* test_single_account_json = kSingleAccountEndpointValidJson;

  FetchStatus accounts_response;
  AccountList accounts;
  std::tie(accounts_response, accounts) = SendAccountsRequestAndWaitForResponse(
      test_single_account_json, /*client_id=*/"", net::HTTP_OK, "text/html");

  EXPECT_EQ(ParseStatus::kInvalidContentTypeError,
            accounts_response.parse_status);
  EXPECT_EQ(net::HTTP_OK, accounts_response.response_code);
  EXPECT_TRUE(accounts.empty());
}

TEST_F(IdpNetworkRequestManagerTest, TokenWrongMimeType) {
  std::string token;
  FetchStatus fetch_status;
  std::tie(fetch_status, token) = SendTokenRequestAndWaitForResponse(
      "account", "request", net::HTTP_OK, "text/html");
  EXPECT_EQ("", token);
  EXPECT_EQ(ParseStatus::kInvalidContentTypeError, fetch_status.parse_status);
  EXPECT_EQ(net::HTTP_OK, fetch_status.response_code);
}

TEST_F(IdpNetworkRequestManagerTest, FetchingTokenLeadsToAContinuationUrl) {
  net::HttpStatusCode http_status = net::HTTP_OK;
  const std::string& mime_type = "application/json";

  const char response[] =
      R"({"continue_on": "https://idp.test/an-absolute-url-for-continuation"})";
  GURL token_endpoint(kTestTokenEndpoint);
  AddResponse(token_endpoint, http_status, mime_type, response);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](FetchStatus status, const std::string& token_response) {});

  auto on_continue = base::BindLambdaForTesting([&](FetchStatus status,
                                                    const GURL& url) {
    // Checks that we got a continuation url event back.
    EXPECT_EQ("https://idp.test/an-absolute-url-for-continuation", url.spec());
    run_loop.Quit();
  });

  std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
  manager->SendTokenRequest(token_endpoint, "account", "request",
                            std::move(callback), std::move(on_continue));
  run_loop.Run();
}

TEST_F(IdpNetworkRequestManagerTest, ContinueOnCanBeRelativeUrl) {
  net::HttpStatusCode http_status = net::HTTP_OK;
  const std::string& mime_type = "application/json";

  const char response[] =
      R"({"continue_on": "/a-relative-url-for-continuation"})";
  GURL token_endpoint(kTestTokenEndpoint);
  AddResponse(token_endpoint, http_status, mime_type, response);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](FetchStatus status, const std::string& token_response) {});

  auto on_continue = base::BindLambdaForTesting([&](FetchStatus status,
                                                    const GURL& url) {
    // Checks that we got a continuation url event back.
    EXPECT_EQ("https://idp.test/a-relative-url-for-continuation", url.spec());
    run_loop.Quit();
  });

  std::unique_ptr<IdpNetworkRequestManager> manager = CreateTestManager();
  manager->SendTokenRequest(token_endpoint, "account", "request",
                            std::move(callback), std::move(on_continue));
  run_loop.Run();
}

}  // namespace

}  // namespace content
