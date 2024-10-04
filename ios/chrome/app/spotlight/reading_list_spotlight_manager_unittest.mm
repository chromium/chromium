// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/app/spotlight/fake_searchable_item_factory.h"
#import "ios/chrome/app/spotlight/fake_spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using testing::_;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {
const char kTestURL1[] = "http://www.example1.com/";
const char kTestURL2[] = "http://www.example2.com/";
const char kTestURL3[] = "http://www.example3.com/";

const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";
const char kTestTitle1[] = "Test Reading List Item Title1";
const char kTestTitle2[] = "Test Reading List Item Title2";
const char kTestTitle3[] = "Test Reading List Item Title3";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGSize size = CGSizeMake(w, h);
  UIImage* favicon = UIImageWithSizeAndSolidColor(size, [UIColor redColor]);
  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(
      new base::RefCountedBytes(base::apple::NSDataToSpan(png)));

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

}  // namespace

class ReadingListSpotlightManagerTest : public PlatformTest {
 public:
  ReadingListSpotlightManagerTest() {
    std::vector<scoped_refptr<ReadingListEntry>> initial_entries;
    initial_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
        GURL(kTestURL1), kTestTitle1, base::Time::Now()));
    initial_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
        GURL(kTestURL2), kTestTitle2, base::Time::Now()));

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::move(initial_entries)));

    profile_ = std::move(builder).Build();

    model_ =
        ReadingListModelFactory::GetInstance()->GetForProfile(profile_.get());

    CreateMockLargeIconService();
    spotlightInterface_ = [[FakeSpotlightInterface alloc] init];

    searchableItemFactory_ = [[FakeSearchableItemFactory alloc]
        initWithDomain:spotlight::DOMAIN_READING_LIST];
  }

 protected:
  void CreateMockLargeIconService() {
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, /*image_fetcher=*/nullptr,
        /*desired_size_in_dip_for_server_requests=*/0,
        /*icon_type_for_server_requests=*/favicon_base::IconType::kTouchIcon,
        /*google_server_client_param=*/"test_chrome"));

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly([](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback), CreateTestBitmap(24, 24)));
        });
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  raw_ptr<ReadingListModel> model_;
  FakeSpotlightInterface* spotlightInterface_;
  FakeSearchableItemFactory* searchableItemFactory_;
};

/// Tests that init propagates the `model` and -shutdown removes it.
TEST_F(ReadingListSpotlightManagerTest, testInitAndShutdown) {
  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:spotlightInterface_
         searchableItemFactory:searchableItemFactory_];

  EXPECT_EQ(manager.model, model_);
  [manager shutdown];
  EXPECT_EQ(manager.model, nil);
}

/// Tests that clearAndReindexReadingList actually clears all items (by calling
/// spotlight api) and adds the items ( by calling the class method
/// indexAllReadingListItemsg)
TEST_F(ReadingListSpotlightManagerTest, testClearsAndIndexesItems) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  // When the model is loaded we call clearAndReindexReadingList
  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:fakeSpotlightInterface
         searchableItemFactory:searchableItemFactory_];

  // We expect to attempt deleting searchable items.
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  // We expect that we call indexSearchableItems api twice because the fake
  // reading list storage initially contains 2 items.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount, 2u);

  [manager shutdown];
}

/// Test that adding an entry via the app (ADDED_VIA_CURRENT_APP) actually adds
/// the entry to spotlight via the indexSearchableItemApi
TEST_F(ReadingListSpotlightManagerTest, testAddEntry) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  // When the model is loaded we call clearAndReindexReadingList
  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:fakeSpotlightInterface
         searchableItemFactory:searchableItemFactory_];

  NSUInteger initialIndexedItemCount =
      fakeSpotlightInterface.indexSearchableItemsCallsCount;

  model_->AddOrReplaceEntry(GURL(kTestURL3), kTestTitle3,
                            reading_list::ADDED_VIA_CURRENT_APP,
                            /*estimated_read_time=*/base::TimeDelta());

  // We expect that we call indexSearchableItems spotlight api when adding a new
  // entry in reading list.
  EXPECT_EQ(fakeSpotlightInterface.indexSearchableItemsCallsCount,
            initialIndexedItemCount + 1);

  [manager shutdown];
}

/// Test that removing an entry  actually
/// removes the entry from spotlight via calling
/// deleteSearchableItemsWithIdentifiers spotlight api.
TEST_F(ReadingListSpotlightManagerTest, testRemoveEntry) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:fakeSpotlightInterface
         searchableItemFactory:searchableItemFactory_];

  model_->RemoveEntryByURL(GURL(kTestURL1), FROM_HERE);

  // We expect to attempt deleting the item that was removed, from spotlight.
  EXPECT_EQ(
      fakeSpotlightInterface.deleteSearchableItemsWithIdentifiersCallsCount,
      1u);

  [manager shutdown];
}

// Test that model updates in background don't do anything until the app is
// foregrounded, at which point they cause a full reindex.
TEST_F(ReadingListSpotlightManagerTest, testBackgroundPausesModelUpdates) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:fakeSpotlightInterface
         searchableItemFactory:searchableItemFactory_];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  model_->RemoveEntryByURL(GURL(kTestURL1), FROM_HERE);

  EXPECT_EQ(
      fakeSpotlightInterface.deleteSearchableItemsWithIdentifiersCallsCount,
      0u);
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(
      fakeSpotlightInterface.deleteSearchableItemsWithIdentifiersCallsCount,
      0u);
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            2u);

  [manager shutdown];
}

// Test that attempting public API calls don't have an immediate effect in
// background, and the update only happens when the app is foregrounded again.
TEST_F(ReadingListSpotlightManagerTest, testBackgroundPausesAPICalls) {
  FakeSpotlightInterface* fakeSpotlightInterface =
      [[FakeSpotlightInterface alloc] init];

  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_
            spotlightInterface:fakeSpotlightInterface
         searchableItemFactory:searchableItemFactory_];
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                  userInfo:nil];

  [manager clearAndReindexReadingList];
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil
                  userInfo:nil];
  EXPECT_EQ(fakeSpotlightInterface
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            2u);

  [manager shutdown];
}
