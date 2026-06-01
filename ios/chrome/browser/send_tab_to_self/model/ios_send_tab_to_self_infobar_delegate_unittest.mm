// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/infobars/core/infobar.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

namespace {

class IOSSendTabToSelfInfoBarDelegateTest : public PlatformTest {
 public:
  IOSSendTabToSelfInfoBarDelegateTest() {
    feature_list_.InitWithFeatures({kSendTabToSelfPropagateScrollPosition,
                                    kSendTabToSelfPropagateFormFields},
                                   {});
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(&browser_state_);
    web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(web_state_.get());

    dispatcher_ = [[CommandDispatcher alloc] init];
    mock_scene_commands_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [dispatcher_ startDispatchingToTarget:mock_scene_commands_
                              forProtocol:@protocol(SceneCommands)];
  }

  web::FakeWebState* web_state() { return web_state_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::FakeBrowserState browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  FakeSendTabToSelfModel model_;
  __strong CommandDispatcher* dispatcher_;
  id<SceneCommands> mock_scene_commands_;
};

// Tests that the infobar delegate properties are correctly set.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Properties) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_,
                                                          mock_scene_commands_);
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
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_,
                                                          mock_scene_commands_);
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(nil, command.textFragment);
        EXPECT_NSEQ(@"test-guid", command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  EXPECT_EQ("test-guid", model_.last_opened_guid());

  [(id)mock_scene_commands_ verify];
}

// Tests that Accept() correctly passes the text fragment if a scroll position
// is present.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithScrollPosition) {
  base::test::ScopedFeatureList feature_list(
      kSendTabToSelfPropagateScrollPosition);

  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "start";
  page_context.scroll_position.text_fragment.text_end = "end";

  SendTabToSelfEntry entry("test-guid", GURL("http://www.test.com"), "title",
                           base::Time::Now(), "device", "target", page_context,
                           NavigationHistory());

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(&entry, &model_,
                                                          mock_scene_commands_);
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(@"start,end", command.textFragment);
        EXPECT_NSEQ(@"test-guid", command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  [(id)mock_scene_commands_ verify];
}

// Tests that Accept() (called when the user taps the primary button on the
// infobar) correctly passes nil for the text fragment if no scroll
// position is present.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithoutScrollPosition) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_,
                                                          mock_scene_commands_);
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(nil, command.textFragment);
        EXPECT_NSEQ(@"test-guid", command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  EXPECT_OCMOCK_VERIFY((id)mock_scene_commands_);
}

// Tests that Cancel() correctly dismisses the entry.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Cancel) {
  std::unique_ptr<SendTabToSelfEntry> entry =
      SendTabToSelfEntry::FromRequiredFields(
          "test-guid", GURL("http://www.test.com"), "device1");
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(entry.get(), &model_,
                                                          mock_scene_commands_);
  ConfirmInfoBarDelegate* confirm_delegate = delegate.get();

  EXPECT_TRUE(confirm_delegate->Cancel());
  EXPECT_EQ("test-guid", model_.last_dismissed_guid());
}

}  // namespace

}  // namespace send_tab_to_self
