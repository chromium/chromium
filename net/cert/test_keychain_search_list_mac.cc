// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_keychain_search_list_mac.h"

#include "base/memory/ptr_util.h"

namespace net {

namespace {

TestKeychainSearchList* g_test_keychain_search_list = nullptr;

}  // namespace

TestKeychainSearchList::TestKeychainSearchList() {
  g_test_keychain_search_list = this;
  scoped_keychain_search_list.reset(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
}

TestKeychainSearchList::~TestKeychainSearchList() {
  g_test_keychain_search_list = nullptr;
}

// static
std::unique_ptr<TestKeychainSearchList> TestKeychainSearchList::Create() {
  if (g_test_keychain_search_list)
    return nullptr;
  return base::WrapUnique(new TestKeychainSearchList);
}

// static
bool TestKeychainSearchList::HasInstance() {
  return !!g_test_keychain_search_list;
}

// static
TestKeychainSearchList* TestKeychainSearchList::GetInstance() {
  return g_test_keychain_search_list;
}

OSStatus TestKeychainSearchList::CopySearchList(
    CFArrayRef* keychain_search_list) const {
  *keychain_search_list =
      CFArrayCreateCopy(kCFAllocatorDefault, scoped_keychain_search_list.get());
  return *keychain_search_list ? 0 : errSecAllocate;
}

void TestKeychainSearchList::AddKeychain(SecKeychainRef keychain) {
  CFArrayAppendValue(scoped_keychain_search_list.get(), keychain);
}

}  // namespace net
