// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_title_util.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Test fixture for tab title utility functions.
class TabTitleUtilTest : public PlatformTest {
 protected:
  TabTitleUtilTest() {
    auto navigation_manager = std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_ = nullptr;
};

// Tests GetTabTitle when there is a download task in the download manager.
TEST_F(TabTitleUtilTest, GetTabTitleWithDownloadTest) {
  DownloadManagerTabHelper::CreateForWebState(&web_state_,
                                              /*delegate=*/nullptr);
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/"), /*mime_type=*/std::string());
  tab_helper->Download(std::move(task));
  base::string16 download_title =
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
  DownloadManagerTabHelper::CreateForWebState(&web_state_,
                                              /*delegate=*/nullptr);
  // No title set on the web state.
  base::string16 default_title =
      l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  NSString* ns_default_title = base::SysUTF16ToNSString(default_title);
  EXPECT_NSEQ(ns_default_title, tab_util::GetTabTitle(&web_state_));

  // Title is set on the web state.
  base::string16 custom_title = base::UTF8ToUTF16("TestTitle");
  NSString* ns_custom_title = base::SysUTF16ToNSString(custom_title);

  web_state_.SetTitle(custom_title);
  EXPECT_NSEQ(ns_custom_title, tab_util::GetTabTitle(&web_state_));
}

}  // namespace
