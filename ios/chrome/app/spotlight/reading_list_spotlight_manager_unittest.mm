// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_test_utils.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {
const char kTestURL[] = "http://www.example.com/";
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";
const char kTestTitle[] = "Test Reading List Item Title";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGSize size = CGSizeMake(w, h);
  UIImage* favicon = UIImageWithSizeAndSolidColor(size, [UIColor redColor]);
  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes(
      static_cast<const unsigned char*>([png bytes]), [png length]));

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
        GURL(kTestURL), kTestTitle, base::Time::Now()));

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::move(initial_entries)));

    browser_state_ = builder.Build();

    model_ = ReadingListModelFactory::GetInstance()->GetForBrowserState(
        browser_state_.get());

    CreateMockLargeIconService();
    readingListSpotlightManager_ = [[ReadingListSpotlightManager alloc]
        initWithLargeIconService:large_icon_service_.get()
                readingListModel:model_];
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

  ~ReadingListSpotlightManagerTest() override {
    [readingListSpotlightManager_ shutdown];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  ReadingListModel* model_;
  ReadingListSpotlightManager* readingListSpotlightManager_;
};

/// Tests that init propagates the `model` and -shutdown removes it.
TEST_F(ReadingListSpotlightManagerTest, testInitAndShutdown) {
  // For this test, don't use readingListSpotlightManager_, and init a new copy
  // instead.

  ReadingListSpotlightManager* manager = [[ReadingListSpotlightManager alloc]
      initWithLargeIconService:large_icon_service_.get()
              readingListModel:model_];

  EXPECT_EQ(manager.model, model_);
  [manager shutdown];
  EXPECT_EQ(manager.model, nil);
}

/// Tests that clearAndReindexReadingList actually clears all items (by calling
/// super) and adds the items (again, by calling super
/// -refreshItemsWithURL:title:)
TEST_F(ReadingListSpotlightManagerTest, testClearsAndIndexesItems) {
  id partialMgrMock =
      [OCMockObject partialMockForObject:readingListSpotlightManager_];
  [[[partialMgrMock expect] andForwardToRealObject]
      clearAllSpotlightItems:[OCMArg any]];

  GURL ignoredURL = GURL("http://chromium.org");
  __block BOOL invocationFired = false;
  [[[[partialMgrMock expect] ignoringNonObjectArgs] andDo:^(NSInvocation* inv) {
    GURL* url;
    [inv getArgument:&url atIndex:2];
    EXPECT_EQ(kTestURL, url->spec());
    __unsafe_unretained NSString* title;
    [inv getArgument:&title atIndex:3];
    EXPECT_NSEQ(title, base::SysUTF8ToNSString(kTestTitle));
    invocationFired = true;
  }] refreshItemsWithURL:ignoredURL title:[OCMArg any]];

  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(3), ^bool {
        return invocationFired;
      }));

  [partialMgrMock clearAndReindexReadingListWithCompletionBlock:nil];

  EXPECT_OCMOCK_VERIFY(partialMgrMock);
}
