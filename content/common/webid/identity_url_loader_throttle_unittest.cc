// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_identity.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::mojom::IdpSigninStatus;

namespace content {

class IdentityUrlLoaderThrottleTest : public testing::Test {
 protected:
  SetIdpStatusCallback CreateCallback() {
    return base::BindRepeating(&IdentityUrlLoaderThrottleTest::SetIdpStatus,
                               base::Unretained(this));
  }

  void SetIdpStatus(const url::Origin& origin, IdpSigninStatus status) {
    ++cb_num_calls_;
    cb_origin_ = origin;
    cb_signin_status_ = status;
  }

  base::HistogramTester histogram_tester_;
  int cb_num_calls_ = 0;
  url::Origin cb_origin_;
  IdpSigninStatus cb_signin_status_ = IdpSigninStatus::kSignedOut;
};

class IdentityUrlLoaderThrottleTestParameterized
    : public IdentityUrlLoaderThrottleTest,
      public testing::WithParamInterface<
          std::tuple<IdpSigninStatus, bool, bool>> {};

TEST_F(IdentityUrlLoaderThrottleTest, DisabledByKillSwitch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "false"}});

  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  EXPECT_EQ(nullptr, throttle);
}

TEST_P(IdentityUrlLoaderThrottleTestParameterized, Headers) {
  IdpSigninStatus signin_status = std::get<0>(GetParam());
  bool has_user_gesture = std::get<1>(GetParam());
  bool is_google_header = std::get<2>(GetParam());

  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.google.com/");
  request.has_user_gesture = has_user_gesture;
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  std::string header;
  if (is_google_header) {
    base::SStringPrintf(
        &header,
        "Google-Accounts-Sign%s: email=\"foo@example.com\", sessionindex=0, "
        "obfuscatedid=123\n",
        signin_status == IdpSigninStatus::kSignedIn ? "In" : "Out");
  } else {
    base::SStringPrintf(
        &header, "idp-signin-status: action=sign%s",
        signin_status == IdpSigninStatus::kSignedIn ? "in" : "out-all");
  }

  network::mojom::URLResponseHead response_head;
  response_head.headers = net::HttpResponseHeaders::TryToCreate(
      base::StringPrintf("HTTP/1.1 200 OK\n%s\n", header.c_str()));
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(1, cb_num_calls_);
  EXPECT_EQ(signin_status, cb_signin_status_);
  EXPECT_EQ(url::Origin::Create(GURL("https://accounts.google.com/")),
            cb_origin_);
  if (signin_status == IdpSigninStatus::kSignedIn) {
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.IdpSigninRequestInitiatedByUser", has_user_gesture, 1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        "Blink.FedCm.IdpSignoutRequestInitiatedByUser", has_user_gesture, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    SignInOutAndUserGesture,
    IdentityUrlLoaderThrottleTestParameterized,
    testing::Combine(testing::Values(IdpSigninStatus::kSignedIn,
                                     IdpSigninStatus::kSignedOut),
                     testing::Values(false, true),
                     testing::Values(false, true)));

TEST_F(IdentityUrlLoaderThrottleTest, NoRelevantHeader) {
  std::unique_ptr<blink::URLLoaderThrottle> throttle =
      MaybeCreateIdentityUrlLoaderThrottle(CreateCallback());
  ASSERT_NE(nullptr, throttle);

  network::ResourceRequest request;
  request.url = GURL("https://accounts.google.com/");
  request.has_user_gesture = true;
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  network::mojom::URLResponseHead response_head;
  response_head.headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\n");
  throttle->WillProcessResponse(request.url, &response_head, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(0, cb_num_calls_);
}

}  // namespace content
