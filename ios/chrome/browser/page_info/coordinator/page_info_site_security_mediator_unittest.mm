// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/coordinator/page_info_site_security_mediator.h"

#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/page_info/ui/page_info_navigation_item_title_view.h"
#import "ios/chrome/browser/page_info/ui/page_info_site_security_description.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

class PageInfoSiteSecurityMediatorTest : public PlatformTest {
 protected:
  PageInfoSiteSecurityMediatorTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = fake_navigation_manager.get();
    fake_web_state_->SetNavigationManager(std::move(fake_navigation_manager));
  }

  void SetVisibleURL(const GURL& url) {
    auto item = web::NavigationItem::Create();
    item->SetVirtualURL(url);
    fake_navigation_manager_->SetVisibleItem(item.get());
    items_.push_back(std::move(item));
  }

  web::FakeWebState* web_state() { return fake_web_state_.get(); }

 private:
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
  std::vector<std::unique_ptr<web::NavigationItem>> items_;
};

// Tests configuration for an internal Chrome page.
TEST_F(PageInfoSiteSecurityMediatorTest, TestChromePageConfiguration) {
  SetVisibleURL(GURL("chrome://version"));

  PageInfoSiteSecurityDescription* config =
      [PageInfoSiteSecurityMediator configurationForWebState:web_state()];

  EXPECT_TRUE(config.isEmpty);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PAGE_INFO_CHROME_PAGE_LABEL),
              config.siteURL);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_PAGE_INFO_INTERNAL_PAGE),
              config.message);
}

// Tests configuration for a standard web page.
TEST_F(PageInfoSiteSecurityMediatorTest, TestWebPageConfiguration) {
  SetVisibleURL(GURL("https://example.com/path"));

  PageInfoSiteSecurityDescription* config =
      [PageInfoSiteSecurityMediator configurationForWebState:web_state()];

  EXPECT_FALSE(config.isEmpty);
  EXPECT_NSEQ(@"example.com", config.siteURL);
}

// Tests navigation item title view with both title and URL labels.
TEST_F(PageInfoSiteSecurityMediatorTest,
       TestNavigationItemTitleViewWithTitleAndURL) {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);
  NSString* siteURL =
      @"accounts.google.com.secure-signin-verification-portal.example.com";

  UIView* titleView = CreatePageInfoNavigationItemTitleView(title, siteURL);
  EXPECT_NE(nil, titleView);
  EXPECT_TRUE([titleView isKindOfClass:[UIStackView class]]);

  UIStackView* stackView = static_cast<UIStackView*>(titleView);
  EXPECT_EQ(2u, stackView.arrangedSubviews.count);

  UILabel* promptLabel = static_cast<UILabel*>(stackView.arrangedSubviews[0]);
  UILabel* titleLabel = static_cast<UILabel*>(stackView.arrangedSubviews[1]);

  EXPECT_NSEQ(siteURL, promptLabel.text);
  EXPECT_EQ(NSLineBreakByTruncatingHead, promptLabel.lineBreakMode);
  EXPECT_FALSE(promptLabel.adjustsFontSizeToFitWidth);

  EXPECT_NSEQ(title, titleLabel.text);
  EXPECT_EQ(NSLineBreakByTruncatingTail, titleLabel.lineBreakMode);
  EXPECT_TRUE(titleLabel.adjustsFontSizeToFitWidth);
  EXPECT_FLOAT_EQ(0.8f, titleLabel.minimumScaleFactor);
}

// Tests navigation item title view when title is empty.
TEST_F(PageInfoSiteSecurityMediatorTest,
       TestNavigationItemTitleViewWithEmptyTitle) {
  NSString* siteURL = @"example.com";

  UIView* titleView = CreatePageInfoNavigationItemTitleView(@"", siteURL);
  EXPECT_NE(nil, titleView);
  EXPECT_TRUE([titleView isKindOfClass:[UILabel class]]);

  UILabel* promptLabel = static_cast<UILabel*>(titleView);
  EXPECT_NSEQ(siteURL, promptLabel.text);
  EXPECT_EQ(NSLineBreakByTruncatingHead, promptLabel.lineBreakMode);
  EXPECT_FALSE(promptLabel.adjustsFontSizeToFitWidth);
}

// Tests navigation item title view when URL is empty.
TEST_F(PageInfoSiteSecurityMediatorTest,
       TestNavigationItemTitleViewWithEmptyURL) {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);

  UIView* titleView = CreatePageInfoNavigationItemTitleView(title, @"");
  EXPECT_NE(nil, titleView);
  EXPECT_TRUE([titleView isKindOfClass:[UILabel class]]);

  UILabel* titleLabel = static_cast<UILabel*>(titleView);
  EXPECT_NSEQ(title, titleLabel.text);
  EXPECT_EQ(NSLineBreakByTruncatingTail, titleLabel.lineBreakMode);
  EXPECT_TRUE(titleLabel.adjustsFontSizeToFitWidth);
}

// Tests navigation item title view when both title and URL are empty.
TEST_F(PageInfoSiteSecurityMediatorTest,
       TestNavigationItemTitleViewWithEmptyTitleAndURL) {
  UIView* titleView = CreatePageInfoNavigationItemTitleView(@"", @"");
  EXPECT_EQ(nil, titleView);
}
