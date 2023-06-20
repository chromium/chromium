// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_navigation_item_storage.h"

#import <Foundation/Foundation.h>
#import <stdint.h>

#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_builder.h"
#import "ios/web/navigation/navigation_item_storage_test_util.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class CRWNavigationItemStorageTest : public PlatformTest {
 protected:
  CRWNavigationItemStorageTest()
      : item_storage_([[CRWNavigationItemStorage alloc] init]) {
    // Set up `item_storage_`.
    [item_storage_ setURL:GURL("http://url.test")];
    [item_storage_ setVirtualURL:GURL("http://virtual.test")];
    [item_storage_ setReferrer:web::Referrer(GURL("http://referrer.url"),
                                             web::ReferrerPolicyDefault)];
    [item_storage_ setTimestamp:base::Time::Now()];
    [item_storage_ setTitle:base::SysNSStringToUTF16(@"Title")];
    [item_storage_ setHTTPRequestHeaders:@{@"HeaderKey" : @"HeaderValue"}];
    [item_storage_ setUserAgentType:web::UserAgentType::DESKTOP];
  }

  // Convenience getter to facilitate dot notation in tests.
  CRWNavigationItemStorage* item_storage() { return item_storage_; }

 protected:
  CRWNavigationItemStorage* item_storage_;
};

// Tests that unarchiving CRWNavigationItemStorage data results in an equivalent
// storage.
TEST_F(CRWNavigationItemStorageTest, EncodeDecode) {
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:item_storage()
                                       requiringSecureCoding:NO
                                                       error:nil];
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  id decoded = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  EXPECT_TRUE(web::ItemStoragesAreEqual(item_storage(), decoded));
}

// Tests that converting CRWNavigationItemStorage to proto and back results in
// an equivalent storage.
TEST_F(CRWNavigationItemStorageTest, EncodeDecodeToProto) {
  web::proto::NavigationItemStorage storage;
  [item_storage() serializeToProto:storage];

  CRWNavigationItemStorage* decoded =
      [[CRWNavigationItemStorage alloc] initWithProto:storage];
  EXPECT_TRUE(web::ItemStoragesAreEqual(item_storage(), decoded));
}

// Tests histograms recording.
TEST_F(CRWNavigationItemStorageTest, Histograms) {
  CRWNavigationItemStorage* storage = [[CRWNavigationItemStorage alloc] init];

  [storage setURL:GURL("http://" + std::string(2048, 'a') + ".test")];
  [storage setVirtualURL:GURL("http://" + std::string(3072, 'b') + ".test")];
  [storage setReferrer:web::Referrer(
                           GURL("http://" + std::string(4096, 'c') + ".test"),
                           web::ReferrerPolicyDefault)];
  [storage setTimestamp:base::Time::Now()];
  [storage setTitle:base::UTF8ToUTF16(std::string(5120, 'd'))];
  [storage setHTTPRequestHeaders:@{
    @"HeaderKey1" : @"HeaderValue1",
    @"HeaderKey2" : @"HeaderValue2",
    @"HeaderKey3" : @"HeaderValue3",
  }];
  [storage setUserAgentType:web::UserAgentType::DESKTOP];

  base::HistogramTester histogram_tester;
  [NSKeyedArchiver archivedDataWithRootObject:storage
                        requiringSecureCoding:NO
                                        error:nil];
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedSizeHistogram, 16 /*KB*/, 1);
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedVirtualURLSizeHistogram, 3 /*KB*/, 1);
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedURLSizeHistogram, 2 /*KB*/, 1);
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedReferrerURLSizeHistogram, 4 /*KB*/, 1);
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedTitleSizeHistogram, 5 /*KB*/, 1);
  histogram_tester.ExpectBucketCount(
      web::kNavigationItemSerializedRequestHeadersSizeHistogram, 1 /*KB*/, 1);
}

// CRWNavigationItemStorage does not store "virtualURL" if the it's the same
// as "URL" to save memory. This test verifies that virtualURL actually gets
// restored correctly.
TEST_F(CRWNavigationItemStorageTest, EncodeDecodeSameVirtualURL) {
  web::NavigationItemImpl item_to_store;
  item_to_store.SetURL(GURL("http://url.test"));
  item_to_store.SetVirtualURL(item_to_store.GetURL());

  CRWNavigationItemStorage* item_storage =
      web::NavigationItemStorageBuilder::BuildStorage(item_to_store);

  // Serialize and deserialize navigation item.
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:item_storage
                                       requiringSecureCoding:NO
                                                       error:nil];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  std::unique_ptr<web::NavigationItemImpl> restored_item =
      web::NavigationItemStorageBuilder::BuildNavigationItemImpl(
          [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey]);

  EXPECT_EQ(item_to_store.GetURL(), restored_item->GetURL());
  EXPECT_EQ(item_to_store.GetVirtualURL(), restored_item->GetVirtualURL());
}

// Tests that virtualURL will be the same as URL, if virtualURL is not
// overridden. This logic allows to store only one URL when virtualURL and URL
// are the same.
TEST_F(CRWNavigationItemStorageTest, VirtualURL) {
  CRWNavigationItemStorage* storage = [[CRWNavigationItemStorage alloc] init];
  storage.URL = GURL("https://foo.test/");
  EXPECT_EQ(storage.URL, storage.virtualURL);
}
