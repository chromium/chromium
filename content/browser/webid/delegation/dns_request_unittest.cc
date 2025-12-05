// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/dns_request.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"
#include "content/browser/webid/network_request_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

using ValueOrError = data_decoder::DataDecoder::ValueOrError;

static constexpr FetchStatus kStatusOk = {ParseStatus::kSuccess, net::HTTP_OK};

class MockNetworkRequestManager : public EmailVerifierNetworkRequestManager {
 public:
  MockNetworkRequestManager()
      : EmailVerifierNetworkRequestManager(url::Origin(),
                                           nullptr,
                                           nullptr,
                                           content::FrameTreeNodeId()) {}
  MOCK_METHOD(void,
              DownloadAndParseUncredentialedUrl,
              (const GURL& url, ParseJsonCallback callback),
              (override));
};

ValueOrError ParseJson(std::string_view json) {
  std::optional<base::Value> val =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  CHECK(val);
  return ValueOrError(std::move(*val));
}

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  std::string GetDnsTxtResolverUrlPrefix() override {
    return "https://dns.google/resolve?type=txt&do=1&name=";
  }
};

}  // namespace

class DnsRequestTest : public testing::Test {
 public:
  DnsRequestTest() {
    request_manager_getter_ = base::BindRepeating(
        [](MockNetworkRequestManager* manager)
            -> EmailVerifierNetworkRequestManager* { return manager; },
        &mock_network_request_manager_);
  }

 protected:
  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&test_browser_client_);
  }

  void TearDown() override { SetBrowserClientForTesting(original_client_); }

  base::test::TaskEnvironment task_environment_;
  MockNetworkRequestManager mock_network_request_manager_;
  DnsRequest::NetworkRequestManagerGetter request_manager_getter_;
  TestContentBrowserClient test_browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(DnsRequestTest, Success) {
  EXPECT_CALL(mock_network_request_manager_, DownloadAndParseUncredentialedUrl)
      .WillOnce(
          WithArgs<0, 1>([&](const GURL& url, ParseJsonCallback callback) {
            static constexpr char kResponse[] = R"({
    "Status": 0,
    "TC": false,
    "RD": true,
    "RA": true,
    "AD": false,
    "CD": false,
    "Question": [{
      "name":"hostname.",
      "type":16
    }],
    "Answer":
      [{
        "name":"hostname.",
        "type":16,
        "TTL":21600,
        "data":"iss=record1"
        }
      ],
    "Comment":"Response from 127.0.0.1."
    })";
            EXPECT_EQ(url.spec(),
                      "https://dns.google/resolve?type=txt&do=1&name=" +
                          base::EscapeQueryParamValue("hostname",
                                                      /*use_plus=*/true));
            std::move(callback).Run(kStatusOk, ParseJson(kResponse));
          }));

  DnsRequest dns_request(request_manager_getter_);

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback,
              Run(testing::Optional(std::vector<std::string>{"iss=record1"})))
      .WillOnce([&]() { run_loop.Quit(); });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, NetError) {
  EXPECT_CALL(mock_network_request_manager_, DownloadAndParseUncredentialedUrl)
      .WillOnce(WithArgs<1>([](ParseJsonCallback callback) {
        std::move(callback).Run(
            {ParseStatus::kInvalidResponseError, net::ERR_FAILED},
            base::unexpected("err"));
      }));

  DnsRequest dns_request(request_manager_getter_);

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt))).WillOnce([&]() {
    run_loop.Quit();
  });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, NetworkContextGetterReturnsNull) {
  DnsRequest dns_request(base::BindRepeating(
      []() -> EmailVerifierNetworkRequestManager* { return nullptr; }));

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt))).WillOnce([&]() {
    run_loop.Quit();
  });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, MultipleTxtRecords) {
  static constexpr char kResponse[] = R"({
    "Status": 0,
    "TC": false,
    "RD": true,
    "RA": true,
    "AD": false,
    "CD": false,
    "Question": [{
      "name":"hostname.",
      "type":16
    }],
    "Answer":
      [{
        "name":"hostname.",
        "type":16,
        "TTL":21600,
        "data":"iss=hello.coop"
       },
       {
        "name":"hostname.",
        "type":16,
        "TTL":21600,
        "data":"iss=foo.com"
       }
      ],
    "Comment":"Response from 127.0.0.1."
    })";

  EXPECT_CALL(mock_network_request_manager_, DownloadAndParseUncredentialedUrl)
      .WillOnce(WithArgs<1>([](ParseJsonCallback callback) {
        std::move(callback).Run(kStatusOk, ParseJson(kResponse));
      }));

  DnsRequest dns_request(request_manager_getter_);

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Optional(std::vector<std::string>{
                            "iss=hello.coop", "iss=foo.com"})))
      .WillOnce([&]() { run_loop.Quit(); });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

}  // namespace content::webid
