// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/snapshots/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ui::test::uiimage_utils::UIImagesAreEqual;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {
const char kExpectedUrl[] = "http://www.testurl.net/";
const char kExpectedTitle[] = "title";
}  // namespace


class ShareToDataBuilderTest : public PlatformTest {
 public:
  ShareToDataBuilderTest() {
    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();

    auto navigation_manager = std::make_unique<web::TestNavigationManager>();
    navigation_manager->AddItem(GURL(kExpectedUrl), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItem(navigation_manager->GetItemAtIndex(
        navigation_manager->GetLastCommittedItemIndex()));
    navigation_manager->GetLastCommittedItem()->SetTitle(
        base::UTF8ToUTF16(kExpectedTitle));

    web_state_ = std::make_unique<web::TestWebState>();
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(chrome_browser_state_.get());
    web_state_->SetVisibleURL(GURL(kExpectedUrl));

    // Attach SnapshotTabHelper to allow snapshot generation.
    SnapshotTabHelper::CreateForWebState(web_state_.get(),
                                         [[NSUUID UUID] UUIDString]);
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    SnapshotTabHelper::FromWebState(web_state_.get())->SetDelegate(delegate_);
    // Needed by the ShareToDataForWebState to get the tab title.
    DownloadManagerTabHelper::CreateForWebState(web_state_.get(),
                                                /*delegate=*/nullptr);
    web_state_->SetTitle(base::UTF8ToUTF16(kExpectedTitle));

    // Add a fake view to the TestWebState. This will be used to capture the
    // snapshot. By default the WebState is not ready for taking snapshot.
    CGRect frame = {CGPointZero, CGSizeMake(300, 400)};
    delegate_.view = [[UIView alloc] initWithFrame:frame];
    delegate_.view.backgroundColor = [UIColor blueColor];
  }

  web::WebState* web_state() { return web_state_.get(); }

 private:
  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ios::ChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::TestWebState> web_state_;

  DISALLOW_COPY_AND_ASSIGN(ShareToDataBuilderTest);
};

// Verifies that ShareToData is constructed properly for a given Tab when there
// is a URL provided for share extensions.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandlingNpShareUrl) {
  const char* kExpectedShareUrl = "http://www.testurl.com/";
  ShareToData* actual_data = activity_services::ShareToDataForWebState(
      web_state(), GURL(kExpectedShareUrl));

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(kExpectedShareUrl, actual_data.shareURL);
  EXPECT_EQ(kExpectedUrl, actual_data.visibleURL);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kExpectedTitle), actual_data.title);
  EXPECT_TRUE(actual_data.isOriginalTitle);
  EXPECT_FALSE(actual_data.isPagePrintable);

  const CGSize size = CGSizeMake(40, 40);
  EXPECT_TRUE(UIImagesAreEqual(
      [actual_data.thumbnailGenerator thumbnailWithSize:size],
      UIImageWithSizeAndSolidColor(size, [UIColor blueColor])));
}

// Verifies that ShareToData is constructed properly for a given Tab when the
// URL designated for share extensions is empty.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandlingNoShareUrl) {
  ShareToData* actual_data =
      activity_services::ShareToDataForWebState(web_state(), GURL());

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(kExpectedUrl, actual_data.shareURL);
  EXPECT_EQ(kExpectedUrl, actual_data.visibleURL);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kExpectedTitle), actual_data.title);
  EXPECT_TRUE(actual_data.isOriginalTitle);
  EXPECT_FALSE(actual_data.isPagePrintable);

  const CGSize size = CGSizeMake(40, 40);
  EXPECT_TRUE(UIImagesAreEqual(
      [actual_data.thumbnailGenerator thumbnailWithSize:size],
      UIImageWithSizeAndSolidColor(size, [UIColor blueColor])));
}

// Verifies that |ShareToDataForWebState()| returns nil if the WebState passed
// is nullptr.
TEST_F(ShareToDataBuilderTest, TestReturnsNilWhenClosing) {
  EXPECT_EQ(nil, activity_services::ShareToDataForWebState(nullptr, GURL()));
}
