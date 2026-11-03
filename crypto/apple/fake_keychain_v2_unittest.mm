// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/fake_keychain_v2.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::apple {
namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
constexpr char kTestService[] = "org.chromium.test-service";
constexpr char kOtherService[] = "org.chromium.other-service";

// Builds a generic-password query for `account`. A null `service` omits
// `kSecAttrService` entirely, which SecItem* treats as a distinct primary-key
// value rather than a wildcard when adding.
NSDictionary* MakeGenericPasswordQuery(
    std::string_view account,
    std::optional<std::string_view> service = kTestService) {
  NSMutableDictionary* query = [@{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(kSecClassGenericPassword),
    base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kTestKeychainAccessGroup),
    base::apple::CFToNSPtrCast(kSecAttrAccount) :
        base::SysUTF8ToNSString(account),
  } mutableCopy];
  if (service) {
    query[base::apple::CFToNSPtrCast(kSecAttrService)] =
        base::SysUTF8ToNSString(*service);
  }
  return query;
}

// Builds a key query for the item whose application label is `label`.
NSDictionary* MakeKeyQuery(std::string_view label) {
  return @{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(kSecClassKey),
    base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kTestKeychainAccessGroup),
    base::apple::CFToNSPtrCast(kSecAttrApplicationLabel) :
        [NSData dataWithBytes:label.data() length:label.size()],
  };
}

// Builds a query that matches every item of `item_class` in the test group.
NSDictionary* MakeMatchAllQuery(CFStringRef item_class) {
  return @{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(item_class),
    base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kTestKeychainAccessGroup),
    base::apple::CFToNSPtrCast(kSecMatchLimit) :
        base::apple::CFToNSPtrCast(kSecMatchLimitAll),
    base::apple::CFToNSPtrCast(kSecReturnAttributes) : @YES,
  };
}

class FakeKeychainV2Test : public testing::Test {
 protected:
  // Adds a minimal `kSecClassKey` item dictionary keyed by application label
  // `label`.
  OSStatus AddKey(std::string_view label) {
    return KeychainV2::GetInstance().ItemAdd(
        base::apple::NSToCFPtrCast(MakeKeyQuery(label)), /*result=*/nullptr);
  }

  OSStatus DeleteKey(std::string_view label) {
    return KeychainV2::GetInstance().ItemDelete(
        base::apple::NSToCFPtrCast(MakeKeyQuery(label)));
  }

  OSStatus UpdateKeyTag(std::string_view label, std::string_view tag) {
    NSDictionary* update = @{
      base::apple::CFToNSPtrCast(kSecAttrApplicationTag) :
          [NSData dataWithBytes:tag.data() length:tag.size()],
    };
    return KeychainV2::GetInstance().ItemUpdate(
        base::apple::NSToCFPtrCast(MakeKeyQuery(label)),
        base::apple::NSToCFPtrCast(update));
  }

  // Returns the attributes of every item of `item_class`.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> CopyAllOfClass(
      CFStringRef item_class) {
    std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items;
    base::apple::ScopedCFTypeRef<CFTypeRef> result;
    OSStatus status = KeychainV2::GetInstance().ItemCopyMatching(
        base::apple::NSToCFPtrCast(MakeMatchAllQuery(item_class)),
        result.InitializeInto());
    if (status == errSecItemNotFound) {
      return items;
    }
    EXPECT_EQ(status, errSecSuccess);
    CFArrayRef array = base::apple::CFCast<CFArrayRef>(result.get());
    for (CFIndex i = 0; array && i < CFArrayGetCount(array); ++i) {
      items.emplace_back(base::apple::CFCast<CFDictionaryRef>(
                             CFArrayGetValueAtIndex(array, i)),
                         base::scoped_policy::RETAIN);
    }
    return items;
  }

  bool HasKey(std::string_view label) {
    return KeychainV2::GetInstance().ItemCopyMatching(
               base::apple::NSToCFPtrCast(MakeKeyQuery(label)),
               /*result=*/nullptr) == errSecSuccess;
  }

  base::apple::ScopedCFTypeRef<SecKeyRef> CreateRandomKey(
      std::string_view label = "test-key") {
    NSDictionary* params = @{
      base::apple::CFToNSPtrCast(kSecAttrLabel) :
          base::SysUTF8ToNSString(label),
      base::apple::CFToNSPtrCast(kSecAttrKeyType) :
          base::apple::CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
      base::apple::CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
      base::apple::CFToNSPtrCast(kSecAttrTokenID) :
          base::apple::CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
      base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
          base::SysUTF8ToNSString(kTestKeychainAccessGroup),
      base::apple::CFToNSPtrCast(kSecPrivateKeyAttrs) : @{
        base::apple::CFToNSPtrCast(kSecAttrIsPermanent) : @YES,
      },
    };
    return KeychainV2::GetInstance().KeyCreateRandomKey(
        base::apple::NSToCFPtrCast(params), /*error=*/nullptr);
  }

  OSStatus AddGenericPassword(
      std::string_view account,
      std::string_view value,
      std::optional<std::string_view> service = kTestService) {
    NSMutableDictionary* attributes =
        [MakeGenericPasswordQuery(account, service) mutableCopy];
    NSData* data = [NSData dataWithBytes:value.data() length:value.size()];
    attributes[base::apple::CFToNSPtrCast(kSecValueData)] = data;
    return KeychainV2::GetInstance().ItemAdd(
        base::apple::NSToCFPtrCast(attributes), /*result=*/nullptr);
  }

  OSStatus UpdateGenericPassword(
      std::string_view account,
      std::string_view new_value,
      std::optional<std::string_view> service = kTestService) {
    NSDictionary* query = MakeGenericPasswordQuery(account, service);
    NSData* data = [NSData dataWithBytes:new_value.data()
                                  length:new_value.size()];
    NSDictionary* update = @{
      base::apple::CFToNSPtrCast(kSecValueData) : data,
    };
    return KeychainV2::GetInstance().ItemUpdate(
        base::apple::NSToCFPtrCast(query), base::apple::NSToCFPtrCast(update));
  }

  OSStatus DeleteGenericPassword(
      std::string_view account,
      std::optional<std::string_view> service = kTestService) {
    NSDictionary* query = MakeGenericPasswordQuery(account, service);
    return KeychainV2::GetInstance().ItemDelete(
        base::apple::NSToCFPtrCast(query));
  }

  std::optional<std::string> ReadGenericPassword(
      std::string_view account,
      std::optional<std::string_view> service = kTestService) {
    NSMutableDictionary* query =
        [MakeGenericPasswordQuery(account, service) mutableCopy];
    query[base::apple::CFToNSPtrCast(kSecReturnAttributes)] = @YES;
    query[base::apple::CFToNSPtrCast(kSecReturnData)] = @YES;
    base::apple::ScopedCFTypeRef<CFTypeRef> result;
    OSStatus status = KeychainV2::GetInstance().ItemCopyMatching(
        base::apple::NSToCFPtrCast(query), result.InitializeInto());
    if (status != errSecSuccess || !result) {
      return std::nullopt;
    }
    CFDictionaryRef dict = base::apple::CFCast<CFDictionaryRef>(result.get());
    if (!dict) {
      return std::nullopt;
    }
    CFDataRef data =
        base::apple::GetValueFromDictionary<CFDataRef>(dict, kSecValueData);
    if (!data) {
      return std::nullopt;
    }
    return std::string(base::as_string_view(base::apple::CFDataToSpan(data)));
  }

  ScopedFakeKeychainV2 scoped_fake_keychain_{kTestKeychainAccessGroup};
};

TEST_F(FakeKeychainV2Test, AddAndCopyGenericPasswordByAccount) {
  ASSERT_EQ(AddGenericPassword("account-a", "secret-payload-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), "secret-payload-a");
}

TEST_F(FakeKeychainV2Test, CopyGenericPasswordDoesNotMatchOtherAccount) {
  ASSERT_EQ(AddGenericPassword("account-a", "secret-payload-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-b"), std::nullopt);
}

TEST_F(FakeKeychainV2Test, AddDuplicateGenericPasswordReturnsDuplicateItem) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  EXPECT_EQ(AddGenericPassword("account-a", "value-b"), errSecDuplicateItem);
}

TEST_F(FakeKeychainV2Test, UpdateGenericPasswordByAccount) {
  ASSERT_EQ(AddGenericPassword("account-a", "initial-value"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "other-value"), errSecSuccess);
  ASSERT_EQ(UpdateGenericPassword("account-a", "updated-value"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), "updated-value");
}

TEST_F(FakeKeychainV2Test, UpdateGenericPasswordLeavesOtherAccountUnchanged) {
  ASSERT_EQ(AddGenericPassword("account-a", "initial-value"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "other-value"), errSecSuccess);
  ASSERT_EQ(UpdateGenericPassword("account-a", "updated-value"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-b"), "other-value");
}

TEST_F(FakeKeychainV2Test, UpdateMissingGenericPasswordReturnsNotFound) {
  EXPECT_EQ(UpdateGenericPassword("nonexistent-account", "value"),
            errSecItemNotFound);
}

TEST_F(FakeKeychainV2Test, DeleteGenericPasswordByAccount) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "value-b"), errSecSuccess);
  ASSERT_EQ(DeleteGenericPassword("account-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), std::nullopt);
}

TEST_F(FakeKeychainV2Test, DeleteGenericPasswordLeavesOtherAccountPresent) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "value-b"), errSecSuccess);
  ASSERT_EQ(DeleteGenericPassword("account-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-b"), "value-b");
}

TEST_F(FakeKeychainV2Test, DeleteMissingGenericPasswordReturnsNotFound) {
  EXPECT_EQ(DeleteGenericPassword("nonexistent-account"), errSecItemNotFound);
}

// The same account under two different services is two distinct items: add,
// copy, update and delete must all key on the service as well as the account.
TEST_F(FakeKeychainV2Test, GenericPasswordsWithDifferentServicesDoNotCollide) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-test", kTestService),
            errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-a", "value-other", kOtherService),
            errSecSuccess);

  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), "value-test");
  EXPECT_EQ(ReadGenericPassword("account-a", kOtherService), "value-other");

  ASSERT_EQ(UpdateGenericPassword("account-a", "updated-test", kTestService),
            errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), "updated-test");
  EXPECT_EQ(ReadGenericPassword("account-a", kOtherService), "value-other");

  ASSERT_EQ(DeleteGenericPassword("account-a", kTestService), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), std::nullopt);
  EXPECT_EQ(ReadGenericPassword("account-a", kOtherService), "value-other");
}

// On a query (unlike on add), an absent kSecAttrService is a wildcard: a
// service-less query matches every item for the account, whatever its service.
TEST_F(FakeKeychainV2Test, QueryWithoutServiceMatchesAllServices) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-test", kTestService),
            errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-a", "value-other", kOtherService),
            errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "value-b", kTestService),
            errSecSuccess);

  NSMutableDictionary* query =
      [MakeGenericPasswordQuery("account-a", std::nullopt) mutableCopy];
  query[base::apple::CFToNSPtrCast(kSecMatchLimit)] =
      base::apple::CFToNSPtrCast(kSecMatchLimitAll);
  query[base::apple::CFToNSPtrCast(kSecReturnAttributes)] = @YES;
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  ASSERT_EQ(KeychainV2::GetInstance().ItemCopyMatching(
                base::apple::NSToCFPtrCast(query), result.InitializeInto()),
            errSecSuccess);
  CFArrayRef matches = base::apple::CFCast<CFArrayRef>(result.get());
  ASSERT_TRUE(matches);
  EXPECT_EQ(CFArrayGetCount(matches), 2);

  // Deleting without a service removes both of account-a's items and leaves
  // account-b alone.
  ASSERT_EQ(DeleteGenericPassword("account-a", std::nullopt), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), std::nullopt);
  EXPECT_EQ(ReadGenericPassword("account-a", kOtherService), std::nullopt);
  EXPECT_EQ(ReadGenericPassword("account-b", kTestService), "value-b");
}

// A missing kSecAttrService is a distinct primary-key value, not a wildcard, so
// an item with a service and an item without one never collide, regardless of
// the order in which they are added.
TEST_F(FakeKeychainV2Test,
       AddGenericPasswordWithAndWithoutServiceIsOrderIndependent) {
  ASSERT_EQ(AddGenericPassword("account-a", "with-service", kTestService),
            errSecSuccess);
  EXPECT_EQ(AddGenericPassword("account-a", "no-service", std::nullopt),
            errSecSuccess);

  ASSERT_EQ(AddGenericPassword("account-b", "no-service", std::nullopt),
            errSecSuccess);
  EXPECT_EQ(AddGenericPassword("account-b", "with-service", kTestService),
            errSecSuccess);

  // A second add that matches on both (absent) service and account is still a
  // duplicate.
  EXPECT_EQ(AddGenericPassword("account-b", "again", std::nullopt),
            errSecDuplicateItem);
}

TEST_F(FakeKeychainV2Test, ItemAddReturnsInjectedFailureAndStoresNothing) {
  scoped_fake_keychain_.keychain()->set_item_add_result(errSecIO);
  EXPECT_EQ(AddGenericPassword("account-a", "value-a"), errSecIO);
  EXPECT_EQ(ReadGenericPassword("account-a"), std::nullopt);

  scoped_fake_keychain_.keychain()->set_item_add_result(errSecSuccess);
  EXPECT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");
}

TEST_F(FakeKeychainV2Test, ItemAddWithoutClassReturnsParamError) {
  NSMutableDictionary* attributes =
      [MakeGenericPasswordQuery("account-a") mutableCopy];
  [attributes removeObjectForKey:base::apple::CFToNSPtrCast(kSecClass)];
  EXPECT_EQ(KeychainV2::GetInstance().ItemAdd(
                base::apple::NSToCFPtrCast(attributes), /*result=*/nullptr),
            errSecParam);
}

// Keys and generic passwords have disjoint attributes (an application label vs.
// a service and account). Every operation must filter on the class first, so
// an operation on one class never inspects or touches the other.
TEST_F(FakeKeychainV2Test, CopyMatchingFiltersOnClass) {
  ASSERT_EQ(AddKey("key-label"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);

  EXPECT_EQ(CopyAllOfClass(kSecClassKey).size(), 1u);
  EXPECT_EQ(CopyAllOfClass(kSecClassGenericPassword).size(), 1u);
}

TEST_F(FakeKeychainV2Test, DeleteKeyLeavesGenericPasswordPresent) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  ASSERT_EQ(AddKey("key-label"), errSecSuccess);

  EXPECT_EQ(DeleteKey("key-label"), errSecSuccess);
  EXPECT_FALSE(HasKey("key-label"));
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");
}

TEST_F(FakeKeychainV2Test, DeleteGenericPasswordLeavesKeyPresent) {
  ASSERT_EQ(AddKey("key-label"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);

  EXPECT_EQ(DeleteGenericPassword("account-a"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), std::nullopt);
  EXPECT_TRUE(HasKey("key-label"));
}

TEST_F(FakeKeychainV2Test, UpdateOneClassLeavesTheOtherUnchanged) {
  ASSERT_EQ(AddKey("key-label"), errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);

  ASSERT_EQ(UpdateKeyTag("key-label", "new-tag"), errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> passwords =
      CopyAllOfClass(kSecClassGenericPassword);
  ASSERT_EQ(passwords.size(), 1u);
  EXPECT_FALSE(
      CFDictionaryContainsKey(passwords[0].get(), kSecAttrApplicationTag));

  ASSERT_EQ(UpdateGenericPassword("account-a", "updated-value"), errSecSuccess);
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys =
      CopyAllOfClass(kSecClassKey);
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_FALSE(CFDictionaryContainsKey(keys[0].get(), kSecValueData));
}

// SecKeyCreateRandomKey() parameters carry no class, so the fake has to tag the
// key it stores. Otherwise class-qualified queries, which every real caller
// sends, would never find it once other classes are present.
TEST_F(FakeKeychainV2Test, KeyCreateRandomKeyStoresKeyClass) {
  base::apple::ScopedCFTypeRef<SecKeyRef> key = CreateRandomKey("test-key");
  ASSERT_TRUE(key);
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);

  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> keys =
      CopyAllOfClass(kSecClassKey);
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_TRUE(CFEqual(base::apple::GetValueFromDictionary<CFStringRef>(
                          keys[0].get(), kSecAttrLabel),
                      CFSTR("test-key")));
  EXPECT_EQ(CopyAllOfClass(kSecClassGenericPassword).size(), 1u);
}

// Generic passwords (and key items added via ItemAdd without a SecKeyRef) do
// not carry kSecValueRef. KeyCopyAttributes must skip them without crashing in
// CFEqual, and return null for a null key.
TEST_F(FakeKeychainV2Test,
       KeyCopyAttributesSkipsItemsWithoutValueRefAndHandlesNullKey) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  ASSERT_EQ(AddKey("key-without-ref"), errSecSuccess);
  base::apple::ScopedCFTypeRef<SecKeyRef> key = CreateRandomKey("test-key");
  ASSERT_TRUE(key);

  EXPECT_FALSE(KeychainV2::GetInstance().KeyCopyAttributes(nullptr));

  base::apple::ScopedCFTypeRef<CFDictionaryRef> attrs =
      KeychainV2::GetInstance().KeyCopyAttributes(key.get());
  ASSERT_TRUE(attrs);
  EXPECT_TRUE(CFEqual(base::apple::GetValueFromDictionary<CFStringRef>(
                          attrs.get(), kSecAttrLabel),
                      CFSTR("test-key")));
  EXPECT_FALSE(CFDictionaryContainsKey(attrs.get(), kSecValueRef));
}

TEST_F(FakeKeychainV2Test,
       MissingOrMismatchedAccessGroupReturnsMissingEntitlement) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);

  NSMutableDictionary* no_group =
      [MakeGenericPasswordQuery("account-b") mutableCopy];
  [no_group removeObjectForKey:base::apple::CFToNSPtrCast(kSecAttrAccessGroup)];
  no_group[base::apple::CFToNSPtrCast(kSecValueData)] =
      [@"value-b" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_EQ(KeychainV2::GetInstance().ItemAdd(
                base::apple::NSToCFPtrCast(no_group), /*result=*/nullptr),
            errSecMissingEntitlement);

  NSMutableDictionary* delete_no_group =
      [MakeGenericPasswordQuery("account-a") mutableCopy];
  [delete_no_group
      removeObjectForKey:base::apple::CFToNSPtrCast(kSecAttrAccessGroup)];
  EXPECT_EQ(KeychainV2::GetInstance().ItemDelete(
                base::apple::NSToCFPtrCast(delete_no_group)),
            errSecMissingEntitlement);
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");

  NSDictionary* update = @{
    base::apple::CFToNSPtrCast(kSecValueData) :
        [@"updated" dataUsingEncoding:NSUTF8StringEncoding],
  };
  EXPECT_EQ(KeychainV2::GetInstance().ItemUpdate(
                base::apple::NSToCFPtrCast(delete_no_group),
                base::apple::NSToCFPtrCast(update)),
            errSecMissingEntitlement);
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");

  NSMutableDictionary* wrong_group =
      [MakeGenericPasswordQuery("account-a") mutableCopy];
  wrong_group[base::apple::CFToNSPtrCast(kSecAttrAccessGroup)] =
      @"other-access-group";
  EXPECT_EQ(KeychainV2::GetInstance().ItemCopyMatching(
                base::apple::NSToCFPtrCast(wrong_group), /*result=*/nullptr),
            errSecMissingEntitlement);
}

TEST_F(FakeKeychainV2Test,
       UpdateRejectsDuplicateGenericPasswordAndLeavesItemsUnchanged) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a", kTestService),
            errSecSuccess);
  ASSERT_EQ(AddGenericPassword("account-b", "value-b", kTestService),
            errSecSuccess);

  // Renaming account-b onto account-a under the same service must fail with
  // errSecDuplicateItem without modifying either item.
  NSDictionary* collide_account = @{
    base::apple::CFToNSPtrCast(kSecAttrAccount) : @"account-a",
    base::apple::CFToNSPtrCast(kSecValueData) :
        [@"clobbered" dataUsingEncoding:NSUTF8StringEncoding],
  };
  EXPECT_EQ(KeychainV2::GetInstance().ItemUpdate(
                base::apple::NSToCFPtrCast(
                    MakeGenericPasswordQuery("account-b", kTestService)),
                base::apple::NSToCFPtrCast(collide_account)),
            errSecDuplicateItem);
  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), "value-a");
  EXPECT_EQ(ReadGenericPassword("account-b", kTestService), "value-b");

  // A service-less query matching two items under different services must also
  // reject an update that collapses them onto the same service.
  ASSERT_EQ(AddGenericPassword("account-a", "other-value", kOtherService),
            errSecSuccess);
  NSDictionary* collide_service = @{
    base::apple::CFToNSPtrCast(kSecAttrService) : @"org.chromium.merged",
  };
  EXPECT_EQ(KeychainV2::GetInstance().ItemUpdate(
                base::apple::NSToCFPtrCast(
                    MakeGenericPasswordQuery("account-a", std::nullopt)),
                base::apple::NSToCFPtrCast(collide_service)),
            errSecDuplicateItem);
  EXPECT_EQ(ReadGenericPassword("account-a", kTestService), "value-a");
  EXPECT_EQ(ReadGenericPassword("account-a", kOtherService), "other-value");

  // Renaming account-b to a non-colliding account succeeds.
  NSDictionary* rename_account = @{
    base::apple::CFToNSPtrCast(kSecAttrAccount) : @"account-c",
  };
  ASSERT_EQ(KeychainV2::GetInstance().ItemUpdate(
                base::apple::NSToCFPtrCast(
                    MakeGenericPasswordQuery("account-b", kTestService)),
                base::apple::NSToCFPtrCast(rename_account)),
            errSecSuccess);
  EXPECT_EQ(ReadGenericPassword("account-b", kTestService), std::nullopt);
  EXPECT_EQ(ReadGenericPassword("account-c", kTestService), "value-b");
}

TEST_F(FakeKeychainV2Test, UpdateRejectsClassInAttributesToUpdate) {
  ASSERT_EQ(AddGenericPassword("account-a", "value-a"), errSecSuccess);
  NSDictionary* change_class = @{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(kSecClassKey),
  };
  EXPECT_EQ(
      KeychainV2::GetInstance().ItemUpdate(
          base::apple::NSToCFPtrCast(MakeGenericPasswordQuery("account-a")),
          base::apple::NSToCFPtrCast(change_class)),
      errSecParam);
  EXPECT_EQ(ReadGenericPassword("account-a"), "value-a");
}

TEST_F(FakeKeychainV2Test, ItemAddStoresMutableCopyIsolatedFromCaller) {
  NSMutableDictionary* attributes =
      [MakeGenericPasswordQuery("account-a") mutableCopy];
  attributes[base::apple::CFToNSPtrCast(kSecValueData)] =
      [@"initial" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_EQ(KeychainV2::GetInstance().ItemAdd(
                base::apple::NSToCFPtrCast(attributes), /*result=*/nullptr),
            errSecSuccess);

  // Mutating the caller's dictionary after ItemAdd() must not affect `items_`.
  attributes[base::apple::CFToNSPtrCast(kSecValueData)] =
      [@"caller-mutated" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_EQ(ReadGenericPassword("account-a"), "initial");

  // Stored entries in `items()` must remain mutable for existing tests (such as
  // ICloudRecoveryKeyTest) that corrupt stored items in place.
  ASSERT_EQ(scoped_fake_keychain_.keychain()->items().size(), 1u);
  CFMutableDictionaryRef stored = const_cast<CFMutableDictionaryRef>(
      scoped_fake_keychain_.keychain()->items().at(0).get());
  NSData* corrupted = [@"corrupted" dataUsingEncoding:NSUTF8StringEncoding];
  CFDictionarySetValue(stored, kSecValueData,
                       base::apple::NSToCFPtrCast(corrupted));
  EXPECT_EQ(ReadGenericPassword("account-a"), "corrupted");
}

}  // namespace
}  // namespace crypto::apple
