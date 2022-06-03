// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Security/Security.h>

#include "device/fido/mac/fake_keychain.h"

#include "base/notreached.h"
#include "device/fido/mac/keychain.h"

namespace device {
namespace fido {
namespace mac {

FakeKeychain::FakeKeychain() = default;
FakeKeychain::~FakeKeychain() = default;

FakeKeychain::Item::Item() = default;
FakeKeychain::Item::Item(Item&&) = default;
FakeKeychain::Item& FakeKeychain::Item::operator=(Item&&) = default;
FakeKeychain::Item::~Item() = default;

base::ScopedCFTypeRef<SecKeyRef> FakeKeychain::KeyCreateRandomKey(
    CFDictionaryRef parameters,
    CFErrorRef* error) {
  // TODO(martinkr): Implement.
  NOTREACHED();
  return base::ScopedCFTypeRef<SecKeyRef>();
}

OSStatus FakeKeychain::ItemCopyMatching(CFDictionaryRef query,
                                        CFTypeRef* result) {
  // TODO(martinkr): Implement.
  NOTREACHED();
  return errSecItemNotFound;
}

OSStatus FakeKeychain::ItemDelete(CFDictionaryRef query) {
  // TODO(martinkr): Implement.
  NOTREACHED();
  return errSecItemNotFound;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
