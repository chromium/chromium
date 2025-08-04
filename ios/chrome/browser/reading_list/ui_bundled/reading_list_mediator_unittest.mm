// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_mediator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/simple_test_clock.h"
#import "components/favicon_base/favicon_types.h"
#import "components/reading_list/core/fake_reading_list_model_storage.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/sync/base/storage_type.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync/test/test_sync_service.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_factory.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_table_view_item.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using testing::_;

namespace reading_list {

// ReadingListMediatorTest is parameterized on this enum to test both
// FaviconAttributesProvider and FaviconLoader.
// TODO(crbug.com/41410664): Remove as part of UIRefresh cleanup.
enum class FaviconServiceType {
  FAVICON_LOADER,
  ATTRIBUTES_PROVIDER,
};

class ReadingListMediatorTest
    : public PlatformTest,
      public ::testing::WithParamInterface<FaviconServiceType> {
 public:
  ReadingListMediatorTest() {
    auto storage = std::make_unique<FakeReadingListModelStorage>();
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();
    model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    // Complete the initial model load from storage.
    storage_ptr->TriggerLoadCompletion();
    sync_service_ = std::make_unique<syncer::TestSyncService>();

    no_title_entry_url_ = GURL("http://chromium.org/unread3");
    // The first 3 have the same update time on purpose.
    model_->AddOrReplaceEntry(GURL("http://chromium.org/unread1"), "unread1",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/std::nullopt,
                              /*creation_time=*/std::nullopt);
    model_->AddOrReplaceEntry(GURL("http://chromium.org/read1"), "read1",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/std::nullopt,
                              /*creation_time=*/std::nullopt);
    model_->SetReadStatusIfExists(GURL("http://chromium.org/read1"), true);
    model_->AddOrReplaceEntry(GURL("http://chromium.org/unread2"), "unread2",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/std::nullopt,
                              /*creation_time=*/std::nullopt);
    clock_.Advance(base::Milliseconds(10));
    model_->AddOrReplaceEntry(no_title_entry_url_, "",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/std::nullopt,
                              /*creation_time=*/std::nullopt);
    clock_.Advance(base::Milliseconds(10));
    model_->AddOrReplaceEntry(GURL("http://chromium.org/read2"), "read2",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/std::nullopt,
                              /*creation_time=*/std::nullopt);
    model_->SetReadStatusIfExists(GURL("http://chromium.org/read2"), true);

    mediator_ = [[ReadingListMediator alloc]
          initWithModel:model_.get()
            syncService:sync_service_.get()
          faviconLoader:&favicon_loader_
        listItemFactory:[[ReadingListListItemFactory alloc] init]];
  }

  ~ReadingListMediatorTest() { [mediator_ disconnect]; }

  ReadingListMediatorTest(const ReadingListMediatorTest&) = delete;
  ReadingListMediatorTest& operator=(const ReadingListMediatorTest&) = delete;

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ReadingListModelImpl> model_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  ReadingListMediator* mediator_;
  base::SimpleTestClock clock_;
  GURL no_title_entry_url_;
  TestFaviconLoader favicon_loader_;
};

TEST_P(ReadingListMediatorTest, fillItems) {
  // Setup.
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];

  // Action.
  [mediator_ fillReadItems:readArray unreadItems:unreadArray];

  // Tests.
  EXPECT_EQ(3U, [unreadArray count]);
  EXPECT_EQ(2U, [readArray count]);
  NSArray<ReadingListTableViewItem*>* rlReadArray = [readArray copy];
  NSArray<ReadingListTableViewItem*>* rlUneadArray = [unreadArray copy];
  EXPECT_NSEQ(rlUneadArray[0].title, @"");
  EXPECT_NSEQ(rlReadArray[0].title, @"read2");
  EXPECT_NSEQ(rlReadArray[1].title, @"read1");
}

INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instatiation name.
    ReadingListMediatorTest,
    ::testing::Values(FaviconServiceType::FAVICON_LOADER,
                      FaviconServiceType::ATTRIBUTES_PROVIDER));

}  // namespace reading_list
