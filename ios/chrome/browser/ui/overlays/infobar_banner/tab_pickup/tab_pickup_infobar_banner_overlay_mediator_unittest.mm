// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/tab_pickup/tab_pickup_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/tab_pickup_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Creates a distant session.
synced_sessions::DistantSession& CreateDistantSession() {
  static synced_sessions::DistantSession distant_session;
  distant_session.tag = "distant session";
  distant_session.name = "distant session";
  distant_session.modified_time = base::Time::Now() - base::Minutes(5);
  distant_session.form_factor = syncer::DeviceInfo::FormFactor::kDesktop;

  auto tab = std::make_unique<synced_sessions::DistantTab>();
  tab->session_tag = distant_session.tag;
  tab->tab_id = SessionID::FromSerializedValue(1);
  tab->title = u"Tab Title";
  tab->virtual_url = GURL("https://www.google.com/");
  distant_session.tabs.push_back(std::move(tab));

  return distant_session;
}

}  // namespace

// Test fixture for TabPickupBannerOverlayMediator.
class TabPickupBannerOverlayMediatorTest : public PlatformTest {
 public:
  TabPickupBannerOverlayMediatorTest() {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kTabPickupThreshold);

    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    local_state_.Get()->SetBoolean(prefs::kTabPickupEnabled, true);
    local_state_.Get()->SetTime(prefs::kTabPickupLastDisplayedTime,
                                base::Time());
    local_state_.Get()->SetString(prefs::kTabPickupLastDisplayedURL,
                                  std::string());

    synced_sessions::DistantSession& session = CreateDistantSession();
    std::unique_ptr<TabPickupInfobarDelegate> delegate =
        std::make_unique<TabPickupInfobarDelegate>(browser_.get(), &session,
                                                   session.tabs.front().get());
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTailoredSecurityService, std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ =
        [[TabPickupBannerOverlayMediator alloc] initWithRequest:request_.get()];
    mediator_.consumer = consumer_;
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  TabPickupInfobarDelegate* delegate_ = nil;
  FakeInfobarBannerConsumer* consumer_ = nil;
  TabPickupBannerOverlayMediator* mediator_ = nil;
};

// Tests that the TabPickupBannerOverlayMediator correctly sets up its consumer.
TEST_F(TabPickupBannerOverlayMediatorTest, SetUpConsumer) {
  NSString* title =
      l10n_util::GetNSStringF(IDS_IOS_TAB_PICKUP_BANNER_TITLE,
                              base::UTF8ToUTF16(delegate_->GetSessionName()));
  NSString* subtitle = @"google.com • 5 mins ago";
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_TAB_PICKUP_BANNER_BUTTON);
  UIImage* defaultFavicon =
      CustomSymbolWithPointSize(kRecentTabsSymbol, kInfobarBannerIconSize);

  EXPECT_NSEQ(title, consumer_.titleText);
  EXPECT_NSEQ(subtitle, consumer_.subtitleText);
  EXPECT_NSEQ(subtitle, consumer_.subtitleText);
  EXPECT_NSEQ(buttonText, consumer_.buttonText);
  EXPECT_NSEQ(defaultFavicon, consumer_.iconImage);
}
