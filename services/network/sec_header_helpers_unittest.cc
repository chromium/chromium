// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

constexpr char kSecureSite[] = "https://site.tld";
constexpr char kInsecureSite[] = "http://othersite.tld";

constexpr char kKnownSecChHeader[] = "Sec-CH-UA";
constexpr char kKnownSecFetchHeader[] = "Sec-Fetch-Site";
constexpr char kOtherSecHeader[] = "sec-other-info-header";
constexpr char kOtherHeader[] = "Other-Header";

constexpr char kHeaderValue[] = "testdata";

}  // namespace

namespace network {

class SecHeaderHelpersTest : public PlatformTest {
 public:
  SecHeaderHelpersTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        url_request_(context_.CreateRequest(GURL(kSecureSite),
                                            net::DEFAULT_PRIORITY,
                                            /*request_delegate=*/nullptr,
                                            TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  net::URLRequest* url_request() const { return url_request_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  net::TestURLRequestContext context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

// Validate that Sec- prefixed headers are all removed when a request is
// downgraded from trustworthy to not such as when an https => http redirect
// occurs. We should only remove sec-ch- and sec-fetch- prefixed headers. Others
// should remain as they may be valid in an insecure context.
TEST_F(SecHeaderHelpersTest, SecHeadersRemovedOnDowngrade) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));
  ASSERT_EQ(2, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherSecHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
}

// Validate that if no downgrade occurs any Sec- prefixed headers remain on the
// provided request.
TEST_F(SecHeaderHelpersTest, SecHeadersRemainOnSecureRedirect) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kSecureSite));
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherSecHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
}

// Validate that if Sec- headers exist as the first or last entries we properly
// remove them also.
TEST_F(SecHeaderHelpersTest, SecHeadersRemoveFirstLast) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(3, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));
  ASSERT_EQ(1, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
}

}  // namespace network
