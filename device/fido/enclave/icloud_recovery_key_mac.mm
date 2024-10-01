// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/icloud_recovery_key_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <memory>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/device_event_log/device_event_log.h"
#include "components/trusted_vault/securebox.h"
#include "crypto/apple_keychain_v2.h"

namespace device::enclave {

namespace {

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

// The kSecAttrServiceValue must match the value used in IdentityKit so that
// these keys can be used as a recovery factor for folsom as well.
constexpr char kAttrService[] = "com.google.common.folsom.cloud.private";

// The value for kSecAttrType for all folsom data on the keychain. This is to
// ensure only Folsom data is returned from keychain queries, even when the
// access group is not set.
static const uint kSecAttrTypeFolsom = 'flsm';

// Returns the public key in uncompressed x9.62 format encoded in padded base64.
NSString* EncodePublicKey(const trusted_vault::SecureBoxPublicKey& public_key) {
  return base::SysUTF8ToNSString(
      base::Base64Encode(public_key.ExportToBytes()));
}

// Returns the private key as a NIST P-256 scalar in padded big-endian format.
NSData* EncodePrivateKey(
    const trusted_vault::SecureBoxPrivateKey& private_key) {
  std::vector<uint8_t> bytes = private_key.ExportToBytes();
  return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

NSMutableDictionary* GetDefaultQuery(std::string_view keychain_access_group) {
  return [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecAttrSynchronizable) : @YES,
    CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(kAttrService),
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrType) : @(kSecAttrTypeFolsom),
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(keychain_access_group),
    CFToNSPtrCast(kSecAttrAccessible) :
        CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),
  }];
}

}  // namespace

ICloudRecoveryKey::ICloudRecoveryKey(
    std::unique_ptr<trusted_vault::SecureBoxKeyPair> key)
    : key_(std::move(key)), id_(key_->public_key().ExportToBytes()) {}

ICloudRecoveryKey::~ICloudRecoveryKey() = default;

// static
void ICloudRecoveryKey::Create(CreateCallback callback,
                               std::string_view keychain_access_group) {
  // Creating a key requires disk access. Do it in a dedicated thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING});

  // Make a copy to ensure the string backing storage does not go away.
  std::string keychain_access_group_copy(keychain_access_group);
  worker_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateAndStoreKeySlowly,
                     std::move(keychain_access_group_copy)),
      std::move(callback));
}

// static
void ICloudRecoveryKey::Retrieve(RetrieveCallback callback,
                                 std::string_view keychain_access_group) {
  // Retrieving keys requires disk access. Do it in a dedicated thread.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING});

  // Make a copy to ensure the string backing storage does not go away.
  std::string keychain_access_group_copy(keychain_access_group);
  worker_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RetrieveKeysSlowly,
                     std::move(keychain_access_group_copy)),
      std::move(callback));
}

// static
std::unique_ptr<ICloudRecoveryKey> ICloudRecoveryKey::CreateForTest() {
  return base::WrapUnique(
      new ICloudRecoveryKey(trusted_vault::SecureBoxKeyPair::GenerateRandom()));
}

// static
std::unique_ptr<ICloudRecoveryKey> ICloudRecoveryKey::CreateAndStoreKeySlowly(
    std::string_view keychain_access_group) {
  std::unique_ptr<trusted_vault::SecureBoxKeyPair> key =
      trusted_vault::SecureBoxKeyPair::GenerateRandom();

  NSMutableDictionary* attributes = GetDefaultQuery(keychain_access_group);
  [attributes setValuesForKeysWithDictionary:@{
    CFToNSPtrCast(kSecAttrAccount) : EncodePublicKey(key->public_key()),
    CFToNSPtrCast(kSecValueData) : EncodePrivateKey(key->private_key()),
  }];
  OSStatus result =
      crypto::AppleKeychainV2::GetInstance().ItemAdd(NSToCFPtrCast(attributes),
                                                     /*result=*/nil);
  if (result != errSecSuccess) {
    FIDO_LOG(ERROR) << "Could not store iCloud recovery key: " << result;
    return nullptr;
  }
  return base::WrapUnique(new ICloudRecoveryKey(std::move(key)));
}

// static
std::vector<std::unique_ptr<ICloudRecoveryKey>>
ICloudRecoveryKey::RetrieveKeysSlowly(std::string_view keychain_access_group) {
  NSDictionary* query = GetDefaultQuery(keychain_access_group);
  [query setValuesForKeysWithDictionary:@{
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitAll),
    CFToNSPtrCast(kSecReturnData) : @YES,
    CFToNSPtrCast(kSecReturnRef) : @YES,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
  }];
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = crypto::AppleKeychainV2::GetInstance().ItemCopyMatching(
      NSToCFPtrCast(query), result.InitializeInto());
  std::vector<std::unique_ptr<ICloudRecoveryKey>> ret;
  if (status == errSecItemNotFound) {
    return ret;
  }
  if (status != errSecSuccess) {
    FIDO_LOG(ERROR) << "Could not retrieve iCloud recovery key: " << status;
    return ret;
  }
  CFArrayRef items = base::apple::CFCastStrict<CFArrayRef>(result.get());
  ret.reserve(CFArrayGetCount(items));
  for (CFIndex i = 0; i < CFArrayGetCount(items); ++i) {
    CFDictionaryRef item = base::apple::CFCastStrict<CFDictionaryRef>(
        CFArrayGetValueAtIndex(items, i));
    CFDataRef key = base::apple::CFCastStrict<CFDataRef>(
        CFDictionaryGetValue(item, kSecValueData));
    std::unique_ptr<trusted_vault::SecureBoxKeyPair> key_pair =
        trusted_vault::SecureBoxKeyPair::CreateByPrivateKeyImport(
            base::apple::CFDataToSpan(key));
    if (!key_pair) {
      FIDO_LOG(ERROR) << "iCloud recovery key is corrupted, skipping";
      continue;
    }
    ret.emplace_back(new ICloudRecoveryKey(std::move(key_pair)));
  }
  return ret;
}

}  // namespace device::enclave
