// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/test/remote_actor_credential_bottom_sheet_app_interface.h"

#import <cstdint>
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

@implementation RemoteActorCredentialBottomSheetAppInterface

+ (NSString*)createValidRemoteActorURLForDomain:(NSString*)domain
                                         gaiaID:(NSString*)gaiaID {
  SetRemoteActorKeychainAccessGroupOverrideForTesting(nil);

  std::vector<uint8_t> key = crypto::RandBytesAsVector(32);
  if (!SetRemoteActorSharedSymmetricKeyForTesting(
          key, nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService)) {
    return nil;
  }

  remote_actor::proto::RemoteActorPasswordUploadRequest request;
  request.set_gaia_id(base::SysNSStringToUTF8(gaiaID));
  request.set_domain(base::SysNSStringToUTF8(domain));
  request.set_task_id("task-123");

  std::optional<std::pair<std::string, std::string>> encrypted =
      CreateEncryptedRemoteActorPayloadForTesting(request, key);
  if (!encrypted.has_value()) {
    return nil;
  }

  NSString* escapedPayload = [base::SysUTF8ToNSString(encrypted->first)
      stringByAddingPercentEncodingWithAllowedCharacters:
          [NSCharacterSet alphanumericCharacterSet]];
  NSString* escapedNonce = [base::SysUTF8ToNSString(encrypted->second)
      stringByAddingPercentEncodingWithAllowedCharacters:
          [NSCharacterSet alphanumericCharacterSet]];

  return [NSString
      stringWithFormat:@"chromium://%@?%@=%@&%@=%@", kRemoteActorURLHost,
                       kRemoteActorPayloadQueryParam, escapedPayload,
                       kRemoteActorNonceQueryParam, escapedNonce];
}

+ (void)cleanUpRemoteActorKeychain {
  DeleteRemoteActorSharedSymmetricKeyForTesting(
      nil, kRemoteActorKeychainAccount, kRemoteActorKeychainService);
  ResetRemoteActorKeychainAccessGroupOverrideForTesting();
}

@end
