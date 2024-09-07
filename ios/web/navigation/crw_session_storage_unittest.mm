// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_storage.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/serializable_user_data_manager_impl.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"

namespace {

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
  CRWSessionStorageTest() {
    // Set up `session_storage_`.
    const base::Time now = base::Time::Now();
    session_storage_ = [[CRWSessionStorage alloc] init];
    session_storage_.hasOpener = YES;
    session_storage_.creationTime = now - base::Minutes(15);
    session_storage_.lastActiveTime = now;
    session_storage_.lastCommittedItemIndex = 0;
    session_storage_.userAgentType = web::UserAgentType::DESKTOP;
    session_storage_.stableIdentifier = [[NSUUID UUID] UUIDString];
    session_storage_.uniqueIdentifier = web::WebStateID::NewUnique();
    session_storage_.certPolicyCacheStorage =
        [[CRWSessionCertificatePolicyCacheStorage alloc] init];
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
  return base::apple::ObjCCast<CRWSessionStorage>(
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey]);
}

}

// Tests that unarchiving CRWSessionStorage data results in an equivalent
// storage.
TEST_F(CRWSessionStorageTest, EncodeDecode) {
  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));

  EXPECT_NSEQ(session_storage_, decoded);
}

// Tests that unarchiving CRWSessionStorage data set lastActiveTime to
// creationTime if the value is unset.
TEST_F(CRWSessionStorageTest, EncodeDecode_LastActiveTimeUnset) {
  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));

  EXPECT_NSEQ(session_storage_, decoded);

  // Reset the lastActiveTime to simulate having it incorrectly initialized
  // in WebStateImpl constructor.
  decoded.lastActiveTime = base::Time();
  ASSERT_NSNE(session_storage_, decoded);

  // Check that encoding and then decoding the value with an unitialized
  // last active time correctly restore the property (to creation time).
  decoded = DecodeSessionStorage(EncodeSessionStorage(decoded));
  EXPECT_EQ(decoded.lastActiveTime, decoded.creationTime);
}

// Tests that conversion to/from proto results in an equivalent storage.
TEST_F(CRWSessionStorageTest, EncodeDecodeToProto) {
  web::proto::WebStateStorage storage;
  [session_storage_ serializeToProto:storage];

  CRWSessionStorage* decoded =
      [[CRWSessionStorage alloc] initWithProto:storage
                              uniqueIdentifier:web::WebStateID::NewUnique()
                              stableIdentifier:[[NSUUID UUID] UUIDString]];

  // The serialization to proto does not maintain the following properties
  // - stableIdentifier
  // - uniqueIdentifier
  // - userData
  //
  // For stableIdentifier and uniqueIdentifier, the decoding generates new
  // random values (since they are not present in the protobuf message but
  // CRWSessionStorage getter assert that the values are set). Expect them
  // to be different from the original values.
  EXPECT_NE(decoded.uniqueIdentifier, session_storage_.uniqueIdentifier);
  EXPECT_NSNE(decoded.stableIdentifier, session_storage_.stableIdentifier);

  // For userData, the decoded object should have the property set to nil.
  EXPECT_FALSE(decoded.userData);

  // Copy the properties that are not serialized by the protobuf message
  // format from the original object to the decoded value, then compare
  // the object to ensure the object is correctly deserialized.
  ASSERT_NSNE(session_storage_, decoded);

  decoded.uniqueIdentifier = session_storage_.uniqueIdentifier;
  decoded.stableIdentifier = session_storage_.stableIdentifier;
  decoded.userData = session_storage_.userData;

  EXPECT_NSEQ(session_storage_, decoded);
}

// Tests that when converting to proto, the metadata information are correct.
TEST_F(CRWSessionStorageTest, MetadataWhenEncodingToProto) {
  web::proto::WebStateStorage storage;
  [session_storage_ serializeToProto:storage];

  // Check that the protobuf message has the expected fields.
  EXPECT_TRUE(storage.has_metadata());
  const web::proto::WebStateMetadataStorage& metadata = storage.metadata();

  EXPECT_EQ(web::TimeFromProto(metadata.creation_time()),
            session_storage_.creationTime);
  EXPECT_EQ(web::TimeFromProto(metadata.last_active_time()),
            session_storage_.lastActiveTime);
  EXPECT_GT(metadata.navigation_item_count(), 0);
  EXPECT_EQ(static_cast<NSUInteger>(metadata.navigation_item_count()),
            session_storage_.itemStorages.count);

  // Fetch the last committed item and check the value in the last active page
  // are initialized from this item.
  ASSERT_GE(session_storage_.lastCommittedItemIndex, 0);
  ASSERT_LT(static_cast<NSUInteger>(session_storage_.lastCommittedItemIndex),
            session_storage_.itemStorages.count);

  CRWNavigationItemStorage* item =
      session_storage_.itemStorages[session_storage_.lastCommittedItemIndex];

  EXPECT_TRUE(metadata.has_active_page());
  const web::proto::PageMetadataStorage& active_page = metadata.active_page();

  EXPECT_EQ(item.title, base::UTF8ToUTF16(active_page.page_title()));
  EXPECT_EQ(item.virtualURL, GURL(active_page.page_url()));
}

// Tests that unarchiving CRWSessionStorage data results in an equivalent
// storage when the user agent is automatic.
TEST_F(CRWSessionStorageTest, EncodeDecodeAutomatic) {
  session_storage_.userAgentType = web::UserAgentType::AUTOMATIC;
  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));

  EXPECT_NSEQ(session_storage_, decoded);
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

// Tests that unarchiving CRWSessionStorage drops invalid itemStorages.
// This is a test for the fix for https://crbug.com/358616893 (where a
// couple of user have corrupt data on disk).
TEST_F(CRWSessionStorageTest, TestWorkaroundForIssue_358616893) {
  session_storage_.itemStorages = @[ @"Not a CRWNavigationItemStorage" ];

  CRWSessionStorage* decoded =
      DecodeSessionStorage(EncodeSessionStorage(session_storage_));
  EXPECT_EQ(decoded.itemStorages.count, 0u);
}
