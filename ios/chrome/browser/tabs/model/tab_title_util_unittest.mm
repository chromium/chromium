// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_title_util.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Test fixture for tab title utility functions.
class TabTitleUtilTest : public PlatformTest {
 protected:
  TabTitleUtilTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    DownloadManagerTabHelper::CreateForWebState(&web_state_);
  }

  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
};

// Tests GetTabTitle when there is a download task in the download manager.
TEST_F(TabTitleUtilTest, GetTabTitleWithDownloadTest) {
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/"), /*mime_type=*/std::string());
  tab_helper->SetCurrentDownload(std::move(task));
  std::u16string download_title =
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TAB_TITLE);
  NSString* ns_download_title = base::SysUTF16ToNSString(download_title);
  EXPECT_NSEQ(ns_download_title, tab_util::GetTabTitle(&web_state_));

  // If the navigation manager has a visible item, GetTabTitle should not
  // return the download title.
  auto item = web::NavigationItem::Create();
  navigation_manager_->SetVisibleItem(item.get());
  EXPECT_FALSE(
      [ns_download_title isEqualToString:tab_util::GetTabTitle(&web_state_)]);
}

// Tests GetTabTitle when there is no download task in the download manager.
TEST_F(TabTitleUtilTest, GetTabTitleWithNoDownloadTest) {
  // No title set on the web state.
  std::u16string default_title =
      l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  NSString* ns_default_title = base::SysUTF16ToNSString(default_title);
  EXPECT_NSEQ(ns_default_title, tab_util::GetTabTitle(&web_state_));

  // Title is set on the web state.
  std::u16string custom_title = u"TestTitle";
  NSString* ns_custom_title = base::SysUTF16ToNSString(custom_title);

  web_state_.SetTitle(custom_title);
  EXPECT_NSEQ(ns_custom_title, tab_util::GetTabTitle(&web_state_));
}

}  // namespace
