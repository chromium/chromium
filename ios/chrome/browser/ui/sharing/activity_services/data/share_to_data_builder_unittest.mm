// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data_builder.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

using ui::test::uiimage_utils::UIImagesAreEqual;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColorAndScale;

namespace {
const char kExpectedUrl[] = "http://www.testurl.net/";
const char16_t kExpectedTitle[] = u"title";
}  // namespace

class ShareToDataBuilderTest : public PlatformTest {
 public:
  ShareToDataBuilderTest() {
    profile_ = TestProfileIOS::Builder().Build();

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(GURL(kExpectedUrl), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItem(navigation_manager->GetItemAtIndex(
        navigation_manager->GetLastCommittedItemIndex()));
    navigation_manager->GetLastCommittedItem()->SetTitle(kExpectedTitle);

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetNavigationManager(std::move(navigation_manager));
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetVisibleURL(GURL(kExpectedUrl));

    // Attach SnapshotTabHelper to allow snapshot generation.
    SnapshotTabHelper::CreateForWebState(web_state_.get());
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    SnapshotTabHelper::FromWebState(web_state_.get())->SetDelegate(delegate_);
    // Needed by the ShareToDataForWebState to get the tab title.
    DownloadManagerTabHelper::CreateForWebState(web_state_.get());
    web_state_->SetTitle(kExpectedTitle);

    // Add a fake view to the FakeWebState. This will be used to capture the
    // snapshot. By default the WebState is not ready for taking snapshot.
    CGRect frame = {CGPointZero, CGSizeMake(300, 400)};
    delegate_.view = [[UIView alloc] initWithFrame:frame];
    delegate_.view.backgroundColor = [UIColor blueColor];
  }

  ShareToDataBuilderTest(const ShareToDataBuilderTest&) = delete;
  ShareToDataBuilderTest& operator=(const ShareToDataBuilderTest&) = delete;

  web::WebState* web_state() { return web_state_.get(); }

 private:
  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Verifies that ShareToData is constructed properly for a given Tab when there
// is a URL provided for share extensions.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandlingWithShareUrl) {
  const char* kExpectedShareUrl = "http://www.testurl.com/";
  ShareToData* actual_data = activity_services::ShareToDataForWebState(
      web_state(), GURL(kExpectedShareUrl));

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(kExpectedShareUrl, actual_data.shareURL);
  EXPECT_EQ(kExpectedUrl, actual_data.visibleURL);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kExpectedTitle), actual_data.title);
  EXPECT_TRUE(actual_data.isOriginalTitle);
  EXPECT_FALSE(actual_data.isPagePrintable);
}

// Verifies that ShareToData is constructed properly for a given Tab when the
// URL designated for share extensions is empty.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandlingNoShareUrl) {
  ShareToData* actual_data =
      activity_services::ShareToDataForWebState(web_state(), GURL());

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(kExpectedUrl, actual_data.shareURL);
  EXPECT_EQ(kExpectedUrl, actual_data.visibleURL);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kExpectedTitle), actual_data.title);
  EXPECT_TRUE(actual_data.isOriginalTitle);
  EXPECT_FALSE(actual_data.isPagePrintable);
}

// Tests that the ShareToDataForURL function creates a ShareToData instance with
// valid properties.
TEST_F(ShareToDataBuilderTest, ShareToDataForURL) {
  GURL testURL = GURL("http://www.testurl.com/");
  NSString* testTitle = @"Some Title";
  NSString* additionalText = @"Foo, Bar!";

  ShareToData* data = activity_services::ShareToDataForURL(testURL, testTitle,
                                                           additionalText, nil);

  EXPECT_EQ(testURL, data.shareURL);
  EXPECT_EQ(testURL, data.visibleURL);
  EXPECT_EQ(testTitle, data.title);
  EXPECT_EQ(additionalText, data.additionalText);
  EXPECT_TRUE(data.isOriginalTitle);
  EXPECT_FALSE(data.isPagePrintable);
  EXPECT_FALSE(data.isPageSearchable);
  EXPECT_FALSE(data.canSendTabToSelf);
  EXPECT_EQ(web::UserAgentType::NONE, data.userAgent);
  EXPECT_FALSE(data.thumbnailGenerator);
}
