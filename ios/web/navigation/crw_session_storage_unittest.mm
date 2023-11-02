// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_storage.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_test_util.h"
#import "ios/web/navigation/serializable_user_data_manager_impl.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/session/crw_session_user_data.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Checks for equality between the item storages in `items1` and `items2`.
BOOL ItemStorageListsAreEqual(NSArray* items1, NSArray* items2) {
  __block BOOL items_are_equal = items1.count == items2.count;
  if (!items_are_equal)
    return NO;
  [items1 enumerateObjectsUsingBlock:^(CRWNavigationItemStorage* item,
                                       NSUInteger idx, BOOL* stop) {
    items_are_equal &= web::ItemStoragesAreEqual(item, items2[idx]);
    *stop = !items_are_equal;
  }];
  return items_are_equal;
}

// Checks for equality between `session1` and `session2`.
BOOL SessionStoragesAreEqual(CRWSessionStorage* session1,
                             CRWSessionStorage* session2) {
  // Check the rest of the properties.
  NSArray<CRWNavigationItemStorage*>* items1 = session1.itemStorages;
  NSArray<CRWNavigationItemStorage*>* items2 = session2.itemStorages;
  return ItemStorageListsAreEqual(items1, items2) &&
         session1.hasOpener == session2.hasOpener &&
         session1.lastCommittedItemIndex == session2.lastCommittedItemIndex &&
         session1.userAgentType == session2.userAgentType &&
         [session1.userData isEqual:session2.userData];
}

// Creates a CRWSessionUserData from an NSDictionary.
CRWSessionUserData* SessionUserDataFromDictionary(
    NSDictionary<NSString*, id<NSCoding>>* dictionary) {
  CRWSessionUserData* data = [[CRWSessionUserData alloc] init];
  for (NSString* key in dictionary) {
    [data setObject:dictionary[key] forKey:key];
  }
  return data;
}

}  // namespace

class CRWSessionStorageTest : public PlatformTest {
 protected:
  CRWSessionStorageTest() : session_storage_([[CRWSessionStorage alloc] init]) {
    // Set up `session_storage_`.
    session_storage_.hasOpener = YES;
    session_storage_.lastCommittedItemIndex = 4;
    session_storage_.userAgentType = web::UserAgentType::DESKTOP;
    session_storage_.stableIdentifier = [[NSUUID UUID] UUIDString];
    session_storage_.userData =
        SessionUserDataFromDictionary(@{@"key" : @"value"});

    // Create an item storage.
    CRWNavigationItemStorage* item_storage =
        [[CRWNavigationItemStorage alloc] init];
    item_storage.virtualURL = GURL("http://init.test");
    item_storage.referrer =
        web::Referrer(GURL("http://referrer.url"), web::ReferrerPolicyDefault);
    item_storage.timestamp = base::Time::Now();
    item_storage.title = base::SysNSStringToUTF16(@"Title");
    item_storage.displayState =
        web::PageDisplayState(CGPointZero, UIEdgeInsetsZero, 0.0, 0.0, 0.0);
    item_storage.HTTPRequestHeaders = @{@"HeaderKey" : @"HeaderValue"};
    session_storage_.itemStorages = @[ item_storage ];
  }

 protected:
  CRWSessionStorage* session_storage_;
};

namespace {

// Helper function to encode a CRWSessionStorage to NSData.
NSData* EncodeSessionStorage(CRWSessionStorage* session_storage) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  [archiver encodeObject:session_storage forKey:NSKeyedArchiveRootObjectKey];
  [archiver finishEncoding];
  return [archiver encodedData];
}

// Helper function to decode a CRWSessionStorage from NSData.
CRWSessionStorage* DecodeSessionStorage(NSData* data) {
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  return base::mac::ObjCCast<CRWSessionStorage>(
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey]);
}

}

// Tests that unarchiving CRWSessionStorage data results in an equivalent
// storage.
TEST_F(CRWSessionStorageTest, EncodeDecode) {
  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));

  EXPECT_TRUE(SessionStoragesAreEqual(session_storage_, decoded));
}

// Tests that unarchiving CRWSessionStorage data results in an equivalent
// storage when the user agent is automatic.
TEST_F(CRWSessionStorageTest, EncodeDecodeAutomatic) {
  session_storage_.userAgentType = web::UserAgentType::AUTOMATIC;
  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));

  EXPECT_TRUE(SessionStoragesAreEqual(session_storage_, decoded));
}

// Tests that unarchiving CRWSessionStorage correctly creates a fresh
// stable identifier if missing from the serialized data.
TEST_F(CRWSessionStorageTest, DecodeStableIdentifierMissing) {
  session_storage_.userData = SessionUserDataFromDictionary(@{});
  session_storage_.stableIdentifier = nil;

  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));
  EXPECT_GT(decoded.stableIdentifier.length, 0u);
}

// Tests that unarchiving CRWSessionStorage correctly initialises the
// stable identifier from the serializable user data key "TabId" if
// present, and that the value is cleared from the decoded object user
// data.
TEST_F(CRWSessionStorageTest, DecodeStableIdentifierFromTabId) {
  session_storage_.userData =
      SessionUserDataFromDictionary(@{@"TabId" : @"tabid-identifier"});
  session_storage_.stableIdentifier = nil;

  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));
  EXPECT_NSEQ(decoded.stableIdentifier, @"tabid-identifier");

  EXPECT_FALSE([decoded.userData objectForKey:@"TabId"]);
}
