// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_KEYCHAIN_H_

#include <stdint.h>
#include <list>
#include <set>
#include <string>
#include <vector>

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "base/component_export.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"

namespace device {
namespace fido {
namespace mac {

// Keychain wraps some operations from the macOS Security framework to work with
// keys and keychain items.
//
// The Touch ID authenticator creates keychain items in the "iOS-style"
// keychain, which scopes item access based on the application-identifer or
// keychain-access-group entitlements, and therefore requires code signing with
// a real Apple developer ID. We therefore group these function here, so they
// can be mocked out in testing.
class COMPONENT_EXPORT(DEVICE_FIDO) API_AVAILABLE(macos(10.12.2)) Keychain {
 public:
  static Keychain& GetInstance();

  // KeyCreateRandomKey wraps the |SecKeyCreateRandomKey| function.
  virtual base::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error);
  // KeyCreateSignature wraps the |SecKeyCreateSignature| function.
  virtual base::ScopedCFTypeRef<CFDataRef> KeyCreateSignature(
      SecKeyRef key,
      SecKeyAlgorithm algorithm,
      CFDataRef data,
      CFErrorRef* error);
  // KeyCopyPublicKey wraps the |SecKeyCopyPublicKey| function.
  virtual base::ScopedCFTypeRef<SecKeyRef> KeyCopyPublicKey(SecKeyRef key);

  // ItemCopyMatching wraps the |SecItemCopyMatching| function.
  virtual OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
  // ItemDelete wraps the |SecItemDelete| function.
  virtual OSStatus ItemDelete(CFDictionaryRef query);

 protected:
  Keychain();
  virtual ~Keychain();

 private:
  friend class base::NoDestructor<Keychain>;
  friend class ScopedTouchIdTestEnvironment;

  // Set an override to the singleton instance returned by |GetInstance|. The
  // caller keeps ownership of the injected keychain and must remove the
  // override by calling |ClearInstanceOverride| before deleting it.
  static void SetInstanceOverride(Keychain* keychain);
  static void ClearInstanceOverride();

  DISALLOW_COPY_AND_ASSIGN(Keychain);
};

// Credential represents a WebAuthn credential from the keychain.
struct COMPONENT_EXPORT(FIDO) Credential {
  Credential(base::ScopedCFTypeRef<SecKeyRef> private_key,
             std::vector<uint8_t> credential_id);
  ~Credential();
  Credential(Credential&& other);
  Credential& operator=(Credential&& other);

  // An opaque reference to the private key that can be used for signing.
  base::ScopedCFTypeRef<SecKeyRef> private_key;

  // The credential ID is a handle to the key that gets passed to the RP. This
  // ID is opaque to the RP, but is obtained by encrypting the credential
  // metadata with a profile-specific metadata secret. See |CredentialMetadata|
  // for more information.
  std::vector<uint8_t> credential_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(Credential);
};

// FindCredentialsInKeychain returns a list of credentials for the given
// |rp_id| with credential IDs matching those from |allowed_credential_ids|,
// which may not be empty. The returned credentials may be resident or
// non-resident.
//
// An LAContext that has been successfully evaluated using |TouchIdContext| may
// be passed in |authenticaton_context|, in order to authorize the credential's
// private key for signing. The authentication may also be null if the caller
// only wants to check for existence of a key, but does not intend to create a
// signature from it. (I.e., the credential's SecKeyRef should not be passed to
// |KeyCreateSignature| if no authentication context was provided, since that
// would trigger a Touch ID prompt dialog).
COMPONENT_EXPORT(FIDO)
std::list<Credential> FindCredentialsInKeychain(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    const std::set<std::vector<uint8_t>>& allowed_credential_ids,
    LAContext* authentication_context) API_AVAILABLE(macosx(10.12.2));

// FindResidentCredentialsInKeychain returns a list of resident credentials for
// the given |rp_id|.
COMPONENT_EXPORT(FIDO)
std::list<Credential> FindResidentCredentialsInKeychain(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    LAContext* authentication_context) API_AVAILABLE(macosx(10.12.2));

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_KEYCHAIN_H_
