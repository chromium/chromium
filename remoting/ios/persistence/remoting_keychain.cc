// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/persistence/remoting_keychain.h"

#import <Security/Security.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"

namespace remoting {

namespace {

using ScopedMutableDictionary =
    base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>;

const char kServicePrefix[] = "com.google.ChromeRemoteDesktop.";

base::apple::ScopedCFTypeRef<CFDataRef> CFDataFromStdString(
    const std::string& data) {
  const UInt8* data_pointer = reinterpret_cast<const UInt8*>(data.data());
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(kCFAllocatorDefault, data_pointer, data.size()));
}

ScopedMutableDictionary CreateScopedMutableDictionary() {
  return ScopedMutableDictionary(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

ScopedMutableDictionary CreateQueryForUpdate(const std::string& service,
                                             const std::string& account) {
  ScopedMutableDictionary dictionary = CreateScopedMutableDictionary();
  CFDictionarySetValue(dictionary.get(), kSecClass, kSecClassGenericPassword);
  base::apple::ScopedCFTypeRef<CFStringRef> service_cf(
      base::SysUTF8ToCFStringRef(service));
  CFDictionarySetValue(dictionary.get(), kSecAttrService, service_cf.get());
  base::apple::ScopedCFTypeRef<CFStringRef> account_cf(
      base::SysUTF8ToCFStringRef(account));
  CFDictionarySetValue(dictionary.get(), kSecAttrAccount, account_cf.get());

  return dictionary;
}

ScopedMutableDictionary CreateQueryForLookup(const std::string& service,
                                             const std::string& account) {
  ScopedMutableDictionary dictionary = CreateQueryForUpdate(service, account);
  CFDictionarySetValue(dictionary.get(), kSecMatchLimit, kSecMatchLimitOne);
  CFDictionarySetValue(dictionary.get(), kSecReturnData, kCFBooleanTrue);
  return dictionary;
}

ScopedMutableDictionary CreateDictionaryForInsertion(const std::string& service,
                                                     const std::string& account,
                                                     const std::string& data) {
  ScopedMutableDictionary dictionary = CreateQueryForUpdate(service, account);
  CFDictionarySetValue(dictionary.get(), kSecValueData,
                       CFDataFromStdString(data).get());
  return dictionary;
}

}  // namespace

RemotingKeychain::RemotingKeychain() : service_prefix_(kServicePrefix) {}

RemotingKeychain::~RemotingKeychain() {}

// static
RemotingKeychain* RemotingKeychain::GetInstance() {
  static base::NoDestructor<RemotingKeychain> instance;
  return instance.get();
}

void RemotingKeychain::SetData(Key key,
                               const std::string& account,
                               const std::string& data) {
  DCHECK(data.size());

  std::string service = KeyToService(key);

  std::string existing_data = GetData(key, account);
  if (!existing_data.empty()) {
    // Item already exists. Update it.

    ScopedMutableDictionary update_query =
        CreateQueryForUpdate(service, account);

    ScopedMutableDictionary updated_attributes =
        CreateScopedMutableDictionary();
    CFDictionarySetValue(updated_attributes.get(), kSecValueData,
                         CFDataFromStdString(data).get());
    OSStatus status =
        SecItemUpdate(update_query.get(), updated_attributes.get());
    if (status != errSecSuccess) {
      LOG(FATAL) << "Failed to update keychain item. Status: " << status;
    }
    return;
  }

  // Item doesn't exist. Add it.
  ScopedMutableDictionary insertion_dictionary =
      CreateDictionaryForInsertion(service, account, data);
  OSStatus status = SecItemAdd(insertion_dictionary.get(), NULL);
  if (status != errSecSuccess) {
    LOG(FATAL) << "Failed to add new keychain item. Status: " << status;
  }
}

std::string RemotingKeychain::GetData(Key key,
                                      const std::string& account) const {
  std::string service = KeyToService(key);

  ScopedMutableDictionary query = CreateQueryForLookup(service, account);
  base::apple::ScopedCFTypeRef<CFDataRef> cf_result;
  OSStatus status =
      SecItemCopyMatching(query.get(), (CFTypeRef*)cf_result.InitializeInto());
  if (status == errSecItemNotFound) {
    return "";
  }
  if (status != errSecSuccess) {
    LOG(FATAL) << "Failed to query keychain data. Status: " << status;
  }

  return std::string(
      base::as_string_view(base::apple::CFDataToSpan(cf_result.get())));
}

void RemotingKeychain::RemoveData(Key key, const std::string& account) {
  std::string service = KeyToService(key);

  ScopedMutableDictionary query = CreateQueryForUpdate(service, account);
  OSStatus status = SecItemDelete(query.get());
  if (status != errSecSuccess && status != errSecItemNotFound) {
    LOG(FATAL) << "Failed to delete a keychain item. Status: " << status;
  }
}

void RemotingKeychain::SetServicePrefixForTesting(
    const std::string& service_prefix) {
  DCHECK(service_prefix.length());
  service_prefix_ = service_prefix;
}

std::string RemotingKeychain::KeyToService(Key key) const {
  return service_prefix_ + KeyToString(key);
}

}  // namespace remoting
