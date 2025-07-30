// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"

#import "components/password_manager/core/browser/import/import_results.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
/// Test password information.
constexpr char kUrl[] = "chromium.org";
constexpr char kUsername[] = "tester";
constexpr char kPassword[] = "te$te^";
}  // namespace

/// A mock SafariDataItemConsumer for use in tests.
@interface MockSafariDataItemConsumer : NSObject <SafariDataItemConsumer>
@property(nonatomic, strong) SafariDataItem* lastPopulatedItem;
@end

@implementation MockSafariDataItemConsumer
- (void)populateItem:(SafariDataItem*)item {
  self.lastPopulatedItem = item;
}
@end

/// Test fixture for `IOSSafariDataImportClient`.
class IOSSafariDataImportClientTest : public PlatformTest {
 protected:
  IOSSafariDataImportClientTest() {
    consumer_ = [[MockSafariDataItemConsumer alloc] init];
    client_.SetSafariDataItemConsumer(consumer_);
  }

  /// Returns the Safari data import client.
  IOSSafariDataImportClient* client() { return &client_; }

  /// Returns the last populated item
  SafariDataItem* last_populated_item() { return consumer_.lastPopulatedItem; }

 private:
  IOSSafariDataImportClient client_;
  MockSafariDataItemConsumer* consumer_;
};

/// Tests that OnBookmarksReady() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnBookmarksReady) {
  client()->OnBookmarksReady(10);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kBookmarks);
  EXPECT_EQ(item.count, 10);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kReady);
}

/// Tests that OnHistoryReady() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnHistoryReady) {
  client()->OnHistoryReady(20, {});
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kHistory);
  EXPECT_EQ(item.count, 20);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kReady);
}

/// Tests that OnPasswordsReady() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnPasswordsReady) {
  password_manager::ImportResults results;
  results.number_to_import = 30;
  password_manager::ImportEntry entry;
  entry.url = kUrl;
  entry.username = kUsername;
  entry.password = kPassword;
  results.displayed_entries = {entry};
  EXPECT_EQ(client()->GetConflictingPasswords().count, 0u);
  client()->OnPasswordsReady(results);
  ASSERT_EQ(client()->GetConflictingPasswords().count, 1u);
  PasswordImportItem* password = client()->GetConflictingPasswords()[0];
  EXPECT_NSEQ(password.username, [NSString stringWithUTF8String:kUsername]);
  EXPECT_NSEQ(password.password, [NSString stringWithUTF8String:kPassword]);
  EXPECT_NSEQ(password.url, [NSString stringWithUTF8String:kUrl]);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kPasswords);
  EXPECT_EQ(item.count, 31);
  EXPECT_EQ(item.invalidCount, 0);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kReady);
}

/// Tests that OnPaymentCardsReady() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnPaymentCardsReady) {
  client()->OnPaymentCardsReady(40);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kPayment);
  EXPECT_EQ(item.count, 40);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kReady);
}

/// Tests that OnBookmarksImported() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnBookmarksImported) {
  client()->OnBookmarksImported(10);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kBookmarks);
  EXPECT_EQ(item.count, 10);
  EXPECT_EQ(item.invalidCount, 0);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kImported);
}

/// Tests that OnHistoryImported() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnHistoryImported) {
  client()->OnHistoryImported(20);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kHistory);
  EXPECT_EQ(item.count, 20);
  EXPECT_EQ(item.invalidCount, 0);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kImported);
}

/// Tests that OnPasswordsImported() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnPasswordsImported) {
  password_manager::ImportResults results;
  results.number_imported = 30;
  password_manager::ImportEntry entry;
  entry.url = kUrl;
  entry.username = kUsername;
  entry.password = kPassword;
  results.displayed_entries = {entry};
  EXPECT_EQ(client()->GetInvalidPasswords().count, 0u);
  client()->OnPasswordsImported(results);
  ASSERT_EQ(client()->GetInvalidPasswords().count, 1u);
  PasswordImportItem* password = client()->GetInvalidPasswords()[0];
  EXPECT_NSEQ(password.username, [NSString stringWithUTF8String:kUsername]);
  EXPECT_NSEQ(password.password, [NSString stringWithUTF8String:kPassword]);
  EXPECT_NSEQ(password.url, [NSString stringWithUTF8String:kUrl]);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kPasswords);
  EXPECT_EQ(item.count, 30);
  EXPECT_EQ(item.invalidCount, 1);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kImported);
}

/// Tests that OnPaymentCardsImported() populates the consumer.
TEST_F(IOSSafariDataImportClientTest, OnPaymentCardsImported) {
  client()->OnPaymentCardsImported(40);
  SafariDataItem* item = last_populated_item();
  ASSERT_TRUE(item);
  EXPECT_EQ(item.type, SafariDataItemType::kPayment);
  EXPECT_EQ(item.count, 40);
  EXPECT_EQ(item.invalidCount, 0);
  EXPECT_EQ(item.status, SafariDataItemImportStatus::kImported);
}
