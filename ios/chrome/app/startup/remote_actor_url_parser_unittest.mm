// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/remote_actor_url_parser.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/app/startup/remote_actor_startup_test_util.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "ios/chrome/browser/passwords/model/remote_actor/test/scoped_keychain_access_group_override.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

class RemoteActorURLParserTest : public PlatformTest {
 protected:
  ~RemoteActorURLParserTest() override { CleanupRemoteActorTestData(); }

  ScopedRemoteActorKeychainAccessGroupOverrideForTesting keychain_override_{
      nil};
};

// Tests that `ParseRemoteActorRequestFromURL()` successfully parses and
// decrypts a valid Remote Actor URL.
TEST_F(RemoteActorURLParserTest, ParseRemoteActorRequestFromURL_Valid) {
  RemoteActorTestData test_data = CreateRemoteActorTestData(@"chromium");

  std::optional<RemoteActorRequest> parsed =
      ParseRemoteActorRequestFromURL(net::GURLWithNSURL(test_data.url));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(test_data.expectation.payload, parsed->payload);
  EXPECT_EQ(test_data.expectation.nonce, parsed->nonce);
  EXPECT_EQ(test_data.expectation.request.gaia_id(), parsed->request.gaia_id());
  EXPECT_EQ(test_data.expectation.request.domain(), parsed->request.domain());
  EXPECT_EQ(test_data.expectation.request.task_id(), parsed->request.task_id());
}

// Tests that `ParseRemoteActorRequestFromURL()` returns `std::nullopt` when
// payload or nonce query parameters are missing or empty.
TEST_F(RemoteActorURLParserTest,
       ParseRemoteActorRequestFromURL_MissingPayloadOrNonce) {
  NSString* missing_nonce_string = [NSString
      stringWithFormat:@"chromium://%@?%@=some_payload", kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam];
  EXPECT_FALSE(ParseRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:missing_nonce_string])));

  NSString* missing_payload_string = [NSString
      stringWithFormat:@"chromium://%@?%@=some_nonce", kRemoteActorURLHost,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ParseRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:missing_payload_string])));

  NSString* empty_payload_string = [NSString
      stringWithFormat:@"chromium://%@?%@=&%@=some_nonce", kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ParseRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:empty_payload_string])));
}

// Tests that `ParseRemoteActorRequestFromURL()` returns `std::nullopt` when
// the payload cannot be decrypted.
TEST_F(RemoteActorURLParserTest,
       ParseRemoteActorRequestFromURL_InvalidPayload) {
  NSString* url_string = [NSString
      stringWithFormat:@"chromium://%@?%@=invalid_payload&%@=invalid_nonce",
                       kRemoteActorURLHost, kRemoteActorPayloadQueryParam,
                       kRemoteActorNonceQueryParam];
  EXPECT_FALSE(ParseRemoteActorRequestFromURL(
      net::GURLWithNSURL([NSURL URLWithString:url_string])));
}

}  // namespace
