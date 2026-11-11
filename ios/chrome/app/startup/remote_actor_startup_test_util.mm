// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/remote_actor_startup_test_util.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "crypto/random.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"
#import "ios/chrome/browser/passwords/model/remote_actor/test/remote_actor_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"

RemoteActorTestData CreateRemoteActorTestData(NSString* scheme) {
  std::vector<uint8_t> key = crypto::RandBytesAsVector(32);
  EXPECT_TRUE(SetRemoteActorSharedSymmetricKeyForTesting(
      key, nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService));

  remote_actor::proto::RemoteActorPasswordUploadRequest request;
  request.set_gaia_id("test_gaia_id");
  request.set_domain("https://example.com");
  request.set_task_id("task_123");

  std::optional<std::pair<std::string, std::string>> encrypted =
      CreateEncryptedRemoteActorPayloadForTesting(request, key);
  EXPECT_TRUE(encrypted.has_value());

  std::string payload = encrypted ? encrypted->first : "";
  std::string nonce = encrypted ? encrypted->second : "";

  NSString* escaped_payload = [base::SysUTF8ToNSString(payload)
      stringByAddingPercentEncodingWithAllowedCharacters:
          [NSCharacterSet alphanumericCharacterSet]];
  NSString* escaped_nonce = [base::SysUTF8ToNSString(nonce)
      stringByAddingPercentEncodingWithAllowedCharacters:
          [NSCharacterSet alphanumericCharacterSet]];
  NSString* url_string = [NSString
      stringWithFormat:@"%@://%@?%@=%@&%@=%@", scheme, kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam, escaped_payload,
                       kRemoteActorNonceQueryParam, escaped_nonce];
  NSURL* url = [NSURL URLWithString:url_string];

  return {
      .url = url,
      .expectation =
          RemoteActorRequest{
              .payload = std::move(payload),
              .nonce = std::move(nonce),
              .request = std::move(request),
          },
  };
}

void CleanupRemoteActorTestData() {
  DeleteRemoteActorSharedSymmetricKeyForTesting(
      nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService);
}
