// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_identity.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/origin.h"

using blink::mojom::IdpSigninStatus;

namespace content {

class TestDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {}
  void Resume() override { ++resume_called_; }
  int resume_called_ = 0;
};

class IdentityUrlLoaderThrottleTest : public testing::Test {
 protected:
  SetIdpStatusCallback CreateCallback() {
    return base::BindRepeating(&IdentityUrlLoaderThrottleTest::SetIdpStatus,
                               base::Unretained(this));
  }

  ParseSetLoginHeaderCallback CreateParseCallback() {
    return GetSetLoginHeaderInProcessParser();
  }

  void SetIdpStatus(const std::optional<url::Origin>& initiator,
                    const url::Origin& idp_origin,
                    IdpSigninStatus status) {
    ++cb_num_calls_;
    cb_initiator_ = initiator;
    cb_idp_origin_ = idp_origin;
    cb_signin_status_ = status;
  }

  int cb_num_calls_ = 0;
  std::optional<url::Origin> cb_initiator_;
  url::Origin cb_idp_origin_;
  IdpSigninStatus cb_signin_status_ = IdpSigninStatus::kSignedOut;
};

class IdentityUrlLoaderThrottleTestParameterized
    : public IdentityUrlLoaderThrottleTest,
      public testing::WithParamInterface<IdpSigninStatus> {};

TEST_P(IdentityUrlLoaderThrottleTestParameterized, Headers) {
  IdpSigninStatus signin_status = GetParam();

  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback(),
                                           CreateParseCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  request.request_initiator = url::Origin::Create(GURL("https://rp.example/"));
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  std::string header = base::StringPrintf(
      "set-login: logged-%s; foo=bar",
      signin_status == IdpSigninStatus::kSignedIn ? "in" : "out");

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      base::StringPrintf("HTTP/1.1 200 OK\n%s\n", header.c_str()));
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(1, cb_num_calls_);
  EXPECT_EQ(signin_status, cb_signin_status_);
  EXPECT_EQ(url::Origin::Create(GURL("https://accounts.idp.example/")),
            cb_idp_origin_);
  EXPECT_EQ(request.request_initiator, cb_initiator_);
  EXPECT_EQ(0, delegate.resume_called_);
}

INSTANTIATE_TEST_SUITE_P(SignInOut,
                         IdentityUrlLoaderThrottleTestParameterized,
                         testing::Values(IdpSigninStatus::kSignedIn,
                                         IdpSigninStatus::kSignedOut));

TEST_F(IdentityUrlLoaderThrottleTest, NoRelevantHeader) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback(),
                                           CreateParseCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(0, cb_num_calls_);
  EXPECT_EQ(0, delegate.resume_called_);
}

TEST_F(IdentityUrlLoaderThrottleTest, InvalidHeader) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback(),
                                           CreateParseCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  // Quoted string is not a token item.
  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: \"logged-in\"\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_EQ(0, cb_num_calls_);

  // Unrecognized token.
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: unknown-status\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_EQ(0, cb_num_calls_);

  // Integer is not a token item.
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: 123\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_EQ(0, cb_num_calls_);
  EXPECT_EQ(0, delegate.resume_called_);
}

TEST_F(IdentityUrlLoaderThrottleTest, AsyncParserCallback) {
  TestDelegate delegate;
  base::OnceCallback<void(
      std::optional<net::structured_headers::ParameterizedItem> item)>
      saved_completion_cb;

  ParseSetLoginHeaderCallback async_parse_cb = base::BindRepeating(
      [](base::OnceCallback<void(
             std::optional<net::structured_headers::ParameterizedItem> item)>*
             saved_completion_cb,
         const std::string& header_value,
         base::OnceCallback<void(
             std::optional<net::structured_headers::ParameterizedItem> item)>
             completion_cb) {
        *saved_completion_cb = std::move(completion_cb);
      },
      &saved_completion_cb);

  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback(), async_parse_cb);
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: logged-in\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);

  // Loading should be deferred while async header parsing is in progress.
  EXPECT_TRUE(defer);
  EXPECT_EQ(0, cb_num_calls_);
  ASSERT_TRUE(saved_completion_cb);

  // Simulating async completion from DataDecoder:
  std::move(saved_completion_cb)
      .Run(net::structured_headers::ParameterizedItem(
          net::structured_headers::Item(
              "logged-in", net::structured_headers::Item::ItemType::kTokenType),
          {}));

  EXPECT_EQ(1, cb_num_calls_);
  EXPECT_EQ(IdpSigninStatus::kSignedIn, cb_signin_status_);
  EXPECT_EQ(1, delegate.resume_called_);
}

TEST_F(IdentityUrlLoaderThrottleTest, ParseCallbackReturnsNull) {
  ParseSetLoginHeaderCallback failing_parse_cb = base::BindRepeating(
      [](const std::string& header_value,
         base::OnceCallback<void(
             std::optional<net::structured_headers::ParameterizedItem> item)>
             completion_cb) { std::move(completion_cb).Run(std::nullopt); });

  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback(), failing_parse_cb);
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: logged-in\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);

  EXPECT_EQ(0, cb_num_calls_);
  EXPECT_EQ(0, delegate.resume_called_);
}

TEST_F(IdentityUrlLoaderThrottleTest, InProcessParserCallback) {
  ParseSetLoginHeaderCallback parse_cb = GetSetLoginHeaderInProcessParser();

  std::optional<net::structured_headers::ParameterizedItem> result_item;
  parse_cb.Run(
      "logged-in; foo=bar",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_token());
  EXPECT_EQ("logged-in", result_item->item.GetString());

  result_item.reset();
  parse_cb.Run(
      "logged-out",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_token());
  EXPECT_EQ("logged-out", result_item->item.GetString());

  result_item.reset();
  parse_cb.Run(
      "\"logged-in\"",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_string());

  result_item.reset();
  parse_cb.Run(
      "123",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_integer());
}

TEST_F(IdentityUrlLoaderThrottleTest, DataDecoderParserCallback) {
  base::test::TaskEnvironment task_environment;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  ParseSetLoginHeaderCallback parse_cb = GetSetLoginHeaderDataDecoderParser();

  std::optional<net::structured_headers::ParameterizedItem> result_item;
  parse_cb.Run(
      "logged-in; foo=bar",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  task_environment.RunUntilIdle();
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_token());
  EXPECT_EQ("logged-in", result_item->item.GetString());

  result_item.reset();
  parse_cb.Run(
      "logged-out",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  task_environment.RunUntilIdle();
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_token());
  EXPECT_EQ("logged-out", result_item->item.GetString());

  result_item.reset();
  parse_cb.Run(
      "\"logged-in\"",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  task_environment.RunUntilIdle();
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_string());

  result_item.reset();
  parse_cb.Run(
      "123",
      base::BindOnce(
          [](std::optional<net::structured_headers::ParameterizedItem>* out,
             std::optional<net::structured_headers::ParameterizedItem> res) {
            *out = res;
          },
          &result_item));
  task_environment.RunUntilIdle();
  ASSERT_TRUE(result_item);
  EXPECT_TRUE(result_item->item.is_integer());
}

}  // namespace content
