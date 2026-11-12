// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/web_identity.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
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

  void SetIdpStatus(network::mojom::RequestDestination destination,
                    const std::optional<url::Origin>& initiator,
                    const url::Origin& idp_origin,
                    IdpSigninStatus status) {
    ++cb_num_calls_;
    cb_destination_ = destination;
    cb_initiator_ = initiator;
    cb_idp_origin_ = idp_origin;
    cb_signin_status_ = status;
  }

  int cb_num_calls_ = 0;
  network::mojom::RequestDestination cb_destination_ =
      network::mojom::RequestDestination::kEmpty;
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
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  request.request_initiator = url::Origin::Create(GURL("https://rp.example/"));
  request.destination = network::mojom::RequestDestination::kDocument;
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
  EXPECT_EQ(request.destination, cb_destination_);
  EXPECT_EQ(0, delegate.resume_called_);
}

INSTANTIATE_TEST_SUITE_P(SignInOut,
                         IdentityUrlLoaderThrottleTestParameterized,
                         testing::Values(IdpSigninStatus::kSignedIn,
                                         IdpSigninStatus::kSignedOut));

TEST_F(IdentityUrlLoaderThrottleTest, NoRelevantHeader) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
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

TEST_F(IdentityUrlLoaderThrottleTest, NonDefaultDestination) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  request.destination = network::mojom::RequestDestination::kScript;
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: logged-in\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(1, cb_num_calls_);
  EXPECT_EQ(network::mojom::RequestDestination::kScript, cb_destination_);
}

TEST_F(IdentityUrlLoaderThrottleTest, InvalidHeader) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);

  // Malformed structured-header item.
  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: !!!\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0, cb_num_calls_);

  // Quoted string is not a token item.
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: \"logged-in\"\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0, cb_num_calls_);

  // Unrecognized token.
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: unknown-status\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0, cb_num_calls_);

  // Integer is not a token item.
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: 123\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0, cb_num_calls_);
  EXPECT_EQ(0, delegate.resume_called_);
}

TEST_F(IdentityUrlLoaderThrottleTest, CertError) {
  TestDelegate delegate;
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);
  throttle->set_delegate(&delegate);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.idp.example/");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.cert_status = net::CERT_STATUS_DATE_INVALID;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\nSet-Login: logged-in\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(0, cb_num_calls_);
}

}  // namespace content
