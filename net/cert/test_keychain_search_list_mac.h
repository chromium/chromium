// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TEST_KEYCHAIN_SEARCH_LIST_MAC_H_
#define NET_CERT_TEST_KEYCHAIN_SEARCH_LIST_MAC_H_

#include <memory>

#include <CoreServices/CoreServices.h>
#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT TestKeychainSearchList {
 public:
  ~TestKeychainSearchList();

  // Creates a TestKeychainSearchList, which will be used by HasInstance and
  // GetInstance.
  // Only one TestKeychainSearchList object may exist at a time, returns nullptr
  // if one exists already.
  static std::unique_ptr<TestKeychainSearchList> Create();

  // Returns true if a TestKeychainSearchList currently exists.
  static bool HasInstance();

  // Returns the current TestKeychainSearchList instance, if any.
  static TestKeychainSearchList* GetInstance();

  // Copies the test keychain search list into |keychain_search_list|.
  OSStatus CopySearchList(CFArrayRef* keychain_search_list) const;

  // Adds |keychain| to the end of the test keychain search list.
  void AddKeychain(SecKeychainRef keychain);

 private:
  TestKeychainSearchList();

  base::apple::ScopedCFTypeRef<CFMutableArrayRef> scoped_keychain_search_list;
};

}  // namespace net

#endif  // NET_CERT_TEST_KEYCHAIN_SEARCH_LIST_MAC_H_
