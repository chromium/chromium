// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_

#include <string>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "device/fido/mac/keychain.h"

namespace device {
namespace fido {
namespace mac {

class FakeKeychain : public Keychain {
 public:
  struct Item {
    Item();
    Item(Item&&);
    Item& operator=(Item&&);

    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    ~Item();

    std::string label;
    std::string application_label;
    std::string application_tag;
    base::ScopedCFTypeRef<SecKeyRef> private_key;
  };

  FakeKeychain();

  FakeKeychain(const FakeKeychain&) = delete;
  FakeKeychain& operator=(const FakeKeychain&) = delete;

  ~FakeKeychain() override;

 protected:
  // Keychain:
  base::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error) override;
  OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) override;
  OSStatus ItemDelete(CFDictionaryRef query) override;

 private:
  std::vector<Item> items_;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_FAKE_KEYCHAIN_H_
