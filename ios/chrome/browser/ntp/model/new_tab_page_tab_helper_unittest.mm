// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const char kTestURL[] = "http://foo.bar";
}  // namespace

// Test fixture for testing NewTabPageTabHelper class.
class NewTabPageTabHelperTest : public PlatformTest {
 protected:
  NewTabPageTabHelperTest() {
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());

    profile_ = std::move(test_cbs_builder).Build();

    auto fake_navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = fake_navigation_manager.get();
    pending_item_ = web::NavigationItem::Create();
    pending_item_->SetURL(GURL(kChromeUIAboutNewTabURL));
    fake_navigation_manager->SetPendingItem(pending_item_.get());
    fake_web_state_.SetNavigationManager(std::move(fake_navigation_manager));
    fake_web_state_.SetBrowserState(profile_.get());

    delegate_ = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  }

  NewTabPageTabHelper* tab_helper() {
    return NewTabPageTabHelper::FromWebState(&fake_web_state_);
  }

  void CreateTabHelper() {
    NewTabPageTabHelper::CreateForWebState(&fake_web_state_);
    NewTabPageTabHelper::FromWebState(&fake_web_state_)->SetDelegate(delegate_);
  }

  id delegate_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<web::NavigationItem> pending_item_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
  web::FakeWebState fake_web_state_;
};

// Tests a newly created NTP webstate.
TEST_F(NewTabPageTabHelperTest, TestAlreadyNTP) {
  GURL url(kChromeUINewTabURL);
  fake_web_state_.SetVisibleURL(url);
  CreateTabHelper();
  EXPECT_TRUE(tab_helper()->IsActive());
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_NEW_TAB_TITLE),
              base::SysUTF16ToNSString(pending_item_->GetTitle()));
}

// Tests a newly created NTP webstate using about://newtab/.
TEST_F(NewTabPageTabHelperTest, TestAlreadyAboutNTP) {
  GURL url(kChromeUIAboutNewTabURL);
  fake_web_state_.SetVisibleURL(url);
  CreateTabHelper();
  EXPECT_TRUE(tab_helper()->IsActive());
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_NEW_TAB_TITLE),
              base::SysUTF16ToNSString(pending_item_->GetTitle()));
}

// Tests a newly created non-NTP webstate.
TEST_F(NewTabPageTabHelperTest, TestNotNTP) {
  GURL url(kTestURL);
  fake_web_state_.SetVisibleURL(url);
  CreateTabHelper();
  EXPECT_FALSE(tab_helper()->IsActive());
  EXPECT_NSEQ(@"", base::SysUTF16ToNSString(pending_item_->GetTitle()));
}

// Tests navigating back and forth between an NTP and non-NTP page.
TEST_F(NewTabPageTabHelperTest, TestToggleToAndFromNTP) {
  CreateTabHelper();
  EXPECT_FALSE(tab_helper()->IsActive());

  GURL url(kChromeUIAboutNewTabURL);
  fake_web_state_.SetCurrentURL(url);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  fake_navigation_manager_->SetLastCommittedItem(pending_item_.get());
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(tab_helper()->IsActive());

  GURL not_ntp_url(kTestURL);
  context.SetUrl(not_ntp_url);
  pending_item_->SetURL(not_ntp_url);
  fake_navigation_manager_->SetPendingItem(pending_item_.get());
  fake_web_state_.OnNavigationStarted(&context);
  EXPECT_FALSE(tab_helper()->IsActive());
  fake_web_state_.SetCurrentURL(not_ntp_url);
  fake_navigation_manager_->SetLastCommittedItem(pending_item_.get());
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(tab_helper()->IsActive());

  pending_item_->SetURL(url);
  context.SetUrl(url);
  fake_navigation_manager_->SetPendingItem(pending_item_.get());
  fake_web_state_.OnNavigationStarted(&context);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_NEW_TAB_TITLE),
              base::SysUTF16ToNSString(pending_item_->GetTitle()));
  fake_web_state_.SetCurrentURL(url);
  fake_navigation_manager_->SetLastCommittedItem(pending_item_.get());
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(tab_helper()->IsActive());

  context.SetUrl(not_ntp_url);
  pending_item_->SetURL(url);
  fake_web_state_.SetCurrentURL(not_ntp_url);
  fake_web_state_.OnNavigationStarted(&context);
  EXPECT_FALSE(tab_helper()->IsActive());
  fake_navigation_manager_->SetLastCommittedItem(pending_item_.get());
  fake_web_state_.OnNavigationFinished(&context);
  EXPECT_FALSE(tab_helper()->IsActive());
}

// Tests double navigations from an NTP and non-NTP page at the same time.
TEST_F(NewTabPageTabHelperTest, TestMismatchedPendingItem) {
  // Test an NTP url with a mismatched pending item.
  GURL url(kChromeUIAboutNewTabURL);
  GURL not_ntp_url(kTestURL);
  fake_web_state_.SetCurrentURL(url);
  pending_item_->SetURL(not_ntp_url);
  CreateTabHelper();
  // In this edge case, although the NTP is visible, the pending item is not
  // incorrectly updated
  EXPECT_EQ(GURL(kTestURL), pending_item_->GetVirtualURL());

  // On commit, the web state url is correct, and the NTP is inactive.
  web::FakeNavigationContext context;
  context.SetUrl(not_ntp_url);
  pending_item_->SetURL(not_ntp_url);
  fake_web_state_.SetCurrentURL(not_ntp_url);
  fake_navigation_manager_->SetLastCommittedItem(pending_item_.get());
  fake_web_state_.OnNavigationFinished(&context);
  EXPECT_EQ(GURL(kTestURL), pending_item_->GetVirtualURL());
}
