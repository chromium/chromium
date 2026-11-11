// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/test/remote_actor_test_util.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import <vector>

#import "base/base64.h"
#import "base/containers/span.h"
#import "crypto/aead.h"
#import "crypto/random.h"

std::optional<std::pair<std::string, std::string>>
CreateEncryptedRemoteActorPayloadForTesting(  // IN-TEST
    const remote_actor::proto::RemoteActorPasswordUploadRequest& request,
    base::span<const uint8_t> key) {
  if (key.size() != crypto::aead::KeySizeFor(crypto::aead::AES_256_GCM)) {
    return std::nullopt;
  }

  std::string proto_bytes;
  if (!request.SerializeToString(&proto_bytes)) {
    return std::nullopt;
  }

  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(
      crypto::aead::NonceSizeFor(crypto::aead::AES_256_GCM));

  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  std::vector<uint8_t> ciphertext =
      aead.Seal(base::as_byte_span(proto_bytes), nonce, /*additional_data=*/{});
  if (ciphertext.empty()) {
    return std::nullopt;
  }

  std::string base64_payload = base::Base64Encode(ciphertext);
  std::string base64_nonce = base::Base64Encode(nonce);
  return std::make_pair(base64_payload, base64_nonce);
}

bool DeleteRemoteActorSharedSymmetricKeyForTesting(  // IN-TEST
    NSString* access_group,
    NSString* account,
    NSString* service) {
  NSMutableDictionary* query = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
  } mutableCopy];

  if (access_group.length > 0) {
    query[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
  return status == errSecSuccess || status == errSecItemNotFound;
}

bool SetRemoteActorSharedSymmetricKeyForTesting(  // IN-TEST
    base::span<const uint8_t> key,
    NSString* access_group,
    NSString* account,
    NSString* service) {
  DeleteRemoteActorSharedSymmetricKeyForTesting(  // IN-TEST
      access_group, account, service);

  NSMutableDictionary* attributes = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
    (__bridge id)kSecValueData : [NSData dataWithBytes:key.data()
                                                length:key.size()],
  } mutableCopy];

  if (access_group.length > 0) {
    attributes[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, nullptr);
  return status == errSecSuccess;
}
