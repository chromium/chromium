// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/remote_actor_url_parser.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using RemoteActorURLParserTest = PlatformTest;

// Tests that `ExtractRemoteActorRequestFromURL()` extracts and unescapes the
// `payload` and `nonce` query parameters without populating `request`.
TEST_F(RemoteActorURLParserTest, ExtractRemoteActorRequestFromURL_Valid) {
  NSString* url_string = [NSString
      stringWithFormat:@"chromium://%@?%@=a%%2Bb%%3D&%@=c%%2Fd%%3D",
                       kRemoteActorURLHost, kRemoteActorPayloadQueryParam,
                       kRemoteActorNonceQueryParam];

  std::optional<RemoteActorRequest> extracted =
      ExtractRemoteActorRequestFromURL(
          net::GURLWithNSURL([NSURL URLWithString:url_string]));
  ASSERT_TRUE(extracted.has_value());
  EXPECT_EQ("a+b=", extracted->payload);
  EXPECT_EQ("c/d=", extracted->nonce);
  EXPECT_FALSE(extracted->request.has_value());
}

// Tests that `ExtractRemoteActorRequestFromURL()` returns `std::nullopt` when
// payload or nonce query parameters are missing or empty.
TEST_F(RemoteActorURLParserTest,
       ExtractRemoteActorRequestFromURL_MissingPayloadOrNonce) {
  NSString* missing_nonce_string = [NSString
      stringWithFormat:@"chromium://%@?%@=some_payload", kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam];
  EXPECT_FALSE(ExtractRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:missing_nonce_string])));

  NSString* missing_payload_string = [NSString
      stringWithFormat:@"chromium://%@?%@=some_nonce", kRemoteActorURLHost,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ExtractRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:missing_payload_string])));

  NSString* empty_payload_string = [NSString
      stringWithFormat:@"chromium://%@?%@=&%@=some_nonce", kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ExtractRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:empty_payload_string])));

  NSString* empty_nonce_string = [NSString
      stringWithFormat:@"chromium://%@?%@=some_payload&%@=",
                       kRemoteActorURLHost, kRemoteActorPayloadQueryParam,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ExtractRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:empty_nonce_string])));
}

}  // namespace
