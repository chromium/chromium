// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

namespace {

class FakeWebState : public web::FakeWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    last_open_url_params_ =
        std::make_unique<web::WebState::OpenURLParams>(params);
  }
  web::WebState::OpenURLParams* last_open_url_params() {
    return last_open_url_params_.get();
  }

 private:
  std::unique_ptr<web::WebState::OpenURLParams> last_open_url_params_;
};

class IOSSendTabToSelfInfoBarDelegateTest : public PlatformTest {
 public:
  IOSSendTabToSelfInfoBarDelegateTest() {
    web_state_ = std::make_unique<FakeWebState>();
    web_state_->SetBrowserState(&browser_state_);
    web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
  }

  FakeWebState* web_state() { return web_state_.get(); }

 protected:
  web::FakeBrowserState browser_state_;
  std::unique_ptr<FakeWebState> web_state_;
  FakeSendTabToSelfModel model_;
};

// Tests that the infobar delegate properties are correctly set.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Properties) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_);
  ConfirmInfoBarDelegate* confirm_delegate = delegate.get();

  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, confirm_delegate->GetButtons());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE),
            confirm_delegate->GetMessageText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL),
      confirm_delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
}

// Tests that Accept() correctly marks the entry as opened and opens the URL.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Accept) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_);
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  EXPECT_TRUE(delegate_ptr->Accept());

  EXPECT_EQ("test-guid", model_.last_opened_guid());

  web::WebState::OpenURLParams* params = web_state()->last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(GURL("http://www.test.com"), params->url);
  EXPECT_FALSE(params->internal_scroll_to_text_fragment.has_value());
}

// Tests that Accept() correctly passes the text fragment if a scroll position
// is present.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithScrollPosition) {
  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "start";
  page_context.scroll_position.text_fragment.text_end = "end";

  SendTabToSelfEntry entry("test-guid", GURL("http://www.test.com"), "title",
                           base::Time::Now(), "device", "target", page_context,
                           NavigationHistory());

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(&entry, &model_);
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  EXPECT_TRUE(delegate_ptr->Accept());

  web::WebState::OpenURLParams* params = web_state()->last_open_url_params();
  ASSERT_TRUE(params);
  EXPECT_EQ(GURL("http://www.test.com"), params->url);
  ASSERT_TRUE(params->internal_scroll_to_text_fragment.has_value());
  EXPECT_EQ("start,end", params->internal_scroll_to_text_fragment.value());
}

// Tests that Cancel() correctly dismisses the entry.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Cancel) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_);
  ConfirmInfoBarDelegate* confirm_delegate = delegate.get();

  EXPECT_TRUE(confirm_delegate->Cancel());
  EXPECT_EQ("test-guid", model_.last_dismissed_guid());
}

}  // namespace

}  // namespace send_tab_to_self
