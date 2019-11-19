// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/offline_page_native_content.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class OfflinePageNativeContentTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Checks the OfflinePageNativeContent is correctly initialized.
TEST_F(OfflinePageNativeContentTest, BasicOfflinePageTest) {
  GURL entry_url("http://foo.bar");
  GURL distilled_url("http://foo.bar/distilled");
  GURL url = reading_list::OfflineURLForPath(
      base::FilePath("offline_id/page.html"), entry_url, distilled_url);
  OfflinePageNativeContent* content = [[OfflinePageNativeContent alloc]
      initWithBrowserState:chrome_browser_state_.get()
                  webState:web_state()
                       URL:url];
  ASSERT_EQ(url, [content url]);
  ASSERT_EQ(distilled_url, [content virtualURL]);
}

// Checks that dismissing offline page restores EntryURL.
TEST_F(OfflinePageNativeContentTest, DismissOfflineContent) {
  GURL offline_url("http://foo.bar/offline");
  GURL entry_url("http://foo.bar/entry");
  GURL virtual_url("http://foo.bar/virtual");
  LoadHtml(@"<html></html>", offline_url);
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  item->SetURL(offline_url);
  item->SetVirtualURL(virtual_url);

  GURL url = reading_list::OfflineURLForPath(
      base::FilePath("offline_id/page.html"), entry_url, virtual_url);
  OfflinePageNativeContent* content = [[OfflinePageNativeContent alloc]
      initWithBrowserState:chrome_browser_state_.get()
                  webState:web_state()
                       URL:url];
  ASSERT_EQ(url, [content url]);
  ASSERT_EQ(virtual_url, [content virtualURL]);
  [content willBeDismissed];
  DCHECK_EQ(item->GetURL(), entry_url);
  DCHECK_EQ(item->GetVirtualURL(), entry_url);
}
