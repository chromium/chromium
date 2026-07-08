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
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
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
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    web_state_ptr_ = fake_web_state.get();
    web_state_ptr_->SetBrowserState(&browser_state_);
    web_state_ptr_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(web_state_ptr_);

    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

    dispatcher_ = [[CommandDispatcher alloc] init];
    mock_scene_commands_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [dispatcher_ startDispatchingToTarget:mock_scene_commands_
                              forProtocol:@protocol(SceneCommands)];
  }

  web::FakeWebState* web_state() { return web_state_ptr_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::FakeBrowserState browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> web_state_ptr_;
  FakeSendTabToSelfModel model_;
  __strong CommandDispatcher* dispatcher_;
  id<SceneCommands> mock_scene_commands_;
};

// Tests that the infobar delegate properties are correctly set.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Properties) {
  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device1",
                              PageContext(), NavigationHistory());
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
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
  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device1",
                              PageContext(), NavigationHistory());
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  std::string guid = entry->GetGUID();
  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(nil, command.textFragment);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  EXPECT_EQ(guid, model_.last_opened_guid());

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

  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device",
                              page_context, NavigationHistory());

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  std::string guid = entry->GetGUID();
  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(nil, command.textFragment);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  [(id)mock_scene_commands_ verify];
}

// Tests that Accept() (called when the user taps the primary button on the
// infobar) correctly passes nil for the text fragment if no scroll
// position is present.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithoutScrollPosition) {
  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device1",
                              PageContext(), NavigationHistory());

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  std::string guid = entry->GetGUID();
  OCMExpect([mock_scene_commands_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL("http://www.test.com"), command.URL);
        EXPECT_NSEQ(nil, command.textFragment);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  EXPECT_TRUE(delegate_ptr->Accept());

  EXPECT_OCMOCK_VERIFY((id)mock_scene_commands_);
}

// Tests that Cancel() correctly dismisses the entry.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, Cancel) {
  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device1",
                              PageContext(), NavigationHistory());
  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* confirm_delegate = delegate.get();

  EXPECT_TRUE(confirm_delegate->Cancel());
  EXPECT_EQ(entry->GetGUID(), model_.last_dismissed_guid());
}

// Tests that Accept() directly activates the single received tab when auto-open
// is enabled.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithSingleTabReceived) {
  base::test::ScopedFeatureList feature_list(
      send_tab_to_self::kSendTabToSelfAutoOpen);

  const SendTabToSelfEntry* entry =
      model_.AddEntryRemotely(GURL("http://www.test.com"), "title", "device1",
                              PageContext(), NavigationHistory());

  // Create and insert a background tab simulating the remotely received tab.
  // Attach SendTabToSelfTabCardLabelData to mark it as part of the batch.
  auto received_tab = std::make_unique<web::FakeWebState>();
  web::FakeWebState* received_tab_ptr = received_tab.get();
  received_tab_ptr->SetBrowserState(&browser_state_);
  received_tab_ptr->SetCurrentURL(GURL("http://www.test.com"));
  SendTabToSelfTabCardLabelData::CreateForWebState(
      received_tab_ptr, entry->GetGUID(), entry->GetDeviceName(),
      base::Time::Now());
  web_state_list_->InsertWebState(std::move(received_tab),
                                  WebStateList::InsertionParams::AtIndex(1));

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  // Verify that before accepting the infobar, the initial tab remains active.
  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_TRUE(delegate_ptr->Accept());
  // Verify that accepting the infobar directly activates the received tab
  // (index 1) in the foreground without opening the Tab Grid.
  EXPECT_EQ(1, web_state_list_->active_index());
  EXPECT_EQ(received_tab_ptr, web_state_list_->GetActiveWebState());
}

// Tests that Accept() activates the latest received tab directly without
// opening the Tab Grid when multiple tabs are received.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest, AcceptWithMultipleTabsReceived) {
  base::test::ScopedFeatureList feature_list(
      send_tab_to_self::kSendTabToSelfAutoOpen);

  const SendTabToSelfEntry* entry1 =
      model_.AddEntryRemotely(GURL("http://www.test1.com"), "title1", "device1",
                              PageContext(), NavigationHistory());
  const SendTabToSelfEntry* entry2 =
      model_.AddEntryRemotely(GURL("http://www.test2.com"), "title2", "device1",
                              PageContext(), NavigationHistory());

  const base::Time now = base::Time::Now();
  // Simulate two background tabs arriving in the same batch with identical
  // creation timestamps.
  auto tab1 = std::make_unique<web::FakeWebState>();
  web::FakeWebState* tab1_ptr = tab1.get();
  tab1_ptr->SetBrowserState(&browser_state_);
  SendTabToSelfTabCardLabelData::CreateForWebState(
      tab1_ptr, entry1->GetGUID(), entry1->GetDeviceName(), now);
  web_state_list_->InsertWebState(std::move(tab1),
                                  WebStateList::InsertionParams::AtIndex(1));

  auto tab2 = std::make_unique<web::FakeWebState>();
  web::FakeWebState* tab2_ptr = tab2.get();
  tab2_ptr->SetBrowserState(&browser_state_);
  SendTabToSelfTabCardLabelData::CreateForWebState(
      tab2_ptr, entry2->GetGUID(), entry2->GetDeviceName(), now);
  web_state_list_->InsertWebState(std::move(tab2),
                                  WebStateList::InsertionParams::AtIndex(2));

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      entry2, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_TRUE(delegate_ptr->Accept());
  // When multiple tabs share the same timestamp, verify that the last tab
  // inserted at the highest index (index 2) is activated directly.
  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(tab2_ptr, web_state_list_->GetActiveWebState());
}

// Tests that Accept() ignores older previously received tabs and treats a
// single new tab as a direct open.
TEST_F(IOSSendTabToSelfInfoBarDelegateTest,
       AcceptWithPreviouslyReceivedTabIgnored) {
  base::test::ScopedFeatureList feature_list(
      send_tab_to_self::kSendTabToSelfAutoOpen);

  const SendTabToSelfEntry* old_entry =
      model_.AddEntryRemotely(GURL("http://www.old.com"), "old_title",
                              "device1", PageContext(), NavigationHistory());
  const SendTabToSelfEntry* new_entry =
      model_.AddEntryRemotely(GURL("http://www.new.com"), "new_title",
                              "device1", PageContext(), NavigationHistory());

  const base::Time now = base::Time::Now();
  const base::Time old_time = now - base::Hours(1);

  // Insert an older tab simulating a previously received tab from an earlier
  // session or batch.
  auto old_tab = std::make_unique<web::FakeWebState>();
  web::FakeWebState* old_tab_ptr = old_tab.get();
  old_tab_ptr->SetBrowserState(&browser_state_);
  SendTabToSelfTabCardLabelData::CreateForWebState(
      old_tab_ptr, old_entry->GetGUID(), old_entry->GetDeviceName(), old_time);
  web_state_list_->InsertWebState(std::move(old_tab),
                                  WebStateList::InsertionParams::AtIndex(1));

  // Insert the newest received tab with the current timestamp.
  auto new_tab = std::make_unique<web::FakeWebState>();
  web::FakeWebState* new_tab_ptr = new_tab.get();
  new_tab_ptr->SetBrowserState(&browser_state_);
  SendTabToSelfTabCardLabelData::CreateForWebState(
      new_tab_ptr, new_entry->GetGUID(), new_entry->GetDeviceName(), now);
  web_state_list_->InsertWebState(std::move(new_tab),
                                  WebStateList::InsertionParams::AtIndex(2));

  auto delegate = IOSSendTabToSelfInfoBarDelegate::Create(
      new_entry, &model_, mock_scene_commands_, web_state_list_.get());
  ConfirmInfoBarDelegate* delegate_ptr = delegate.get();

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state());
  manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));

  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_TRUE(delegate_ptr->Accept());
  // Verify that the older tab is ignored in favor of the tab with the newest
  // timestamp (index 2), activating it directly without opening Tab Grid.
  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(new_tab_ptr, web_state_list_->GetActiveWebState());
}

}  // namespace

}  // namespace send_tab_to_self
