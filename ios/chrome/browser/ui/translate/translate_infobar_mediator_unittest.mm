// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/legacy_translate_infobar_mediator.h"

#include <memory>

#import "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/translate/translate_option_selection_handler.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/translate/cells/select_language_popup_menu_item.h"
#import "ios/chrome/browser/ui/translate/cells/translate_popup_menu_item.h"
#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/deprecated/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate::testing::MockTranslateInfoBarDelegate;
using translate::testing::MockTranslateInfoBarDelegateFactory;

// A protocol used to mock a
// id<LanguageSelectionHandler,TranslateOptionSelectionHandler>.
@protocol TestSelectionHandlerProtocol <LanguageSelectionHandler,
                                        TranslateOptionSelectionHandler>
@end

// Test class that conforms to PopupMenuConsumer and exposes the menu items.
@interface TestPopupMenuConsumer : NSObject <PopupMenuConsumer>

@property(nonatomic, strong)
    NSMutableArray<TableViewItem<PopupMenuItem>*>* items;

@end

@implementation TestPopupMenuConsumer

@synthesize itemToHighlight;

- (void)setPopupMenuItems:
    (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  _items = [[NSMutableArray alloc] init];

  for (NSArray* innerArray in items) {
    [_items addObjectsFromArray:innerArray];
  }
}

- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items {
  EXPECT_TRUE(false) << "This method should not be called.";
}

@end

class TranslateInfobarMediatorTest : public PlatformTest {
 protected:
  TranslateInfobarMediatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        web_state_list_(
            std::make_unique<WebStateList>(&web_state_list_delegate_)),
        delegate_factory_("fr", "en"),
        selection_handler_([OCMockObject
            niceMockForProtocol:@protocol(TestSelectionHandlerProtocol)]),
        notification_handler_([OCMockObject
            niceMockForProtocol:@protocol(TranslateNotificationHandler)]),
        mediator_([[LegacyTranslateInfobarMediator alloc]
            initWithSelectionHandler:selection_handler_
                 notificationHandler:notification_handler_]) {
    CreateTranslateClient();
  }

  ~TranslateInfobarMediatorTest() override { [mediator_ disconnect]; }

  WebStateList* web_state_list() { return web_state_list_.get(); }

  id selection_handler() { return selection_handler_; }

  id notification_handler() { return notification_handler_; }

  LegacyTranslateInfobarMediator* mediator() { return mediator_; }

  void CreateTranslateClient() {
    auto web_state = std::make_unique<web::TestWebState>();

    // Set up browser state.
    web_state->SetBrowserState(browser_state_.get());

    // Set up navigation manager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetBrowserState(browser_state_.get());
    web_state->SetNavigationManager(std::move(navigation_manager));

    // Set up JS injection receiver.
    CRWTestJSInjectionReceiver* injectionReceiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state->SetJSInjectionReceiver(injectionReceiver);

    // Create IOSLanguageDetectionTabHelper.
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        web_state.get(),
        /*url_language_histogram=*/nullptr);

    // Create ChromeIOSTranslateClient.
    ChromeIOSTranslateClient::CreateForWebState(web_state.get());

    int effective_index = web_state_list_->InsertWebState(
        0, std::move(web_state), WebStateList::INSERT_NO_FLAGS,
        WebStateOpener());
    web_state_list_->ActivateWebStateAt(effective_index);
  }

  ChromeIOSTranslateClient* GetTranslateClient() {
    return ChromeIOSTranslateClient::FromWebState(
        web_state_list_->GetActiveWebState());
  }

  MockTranslateInfoBarDelegate* GetDelegate() {
    return delegate_factory_.GetMockTranslateInfoBarDelegate();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  MockTranslateInfoBarDelegateFactory delegate_factory_;
  id selection_handler_;
  id notification_handler_;
  LegacyTranslateInfobarMediator* mediator_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfobarMediatorTest);
};

// Tests that the mediator installs UI handlers on existing
// ChromeIOSTranslateClient instances as well as new ones that become available.
TEST_F(TranslateInfobarMediatorTest, InstallHandlers) {
  ChromeIOSTranslateClient* translate_client = GetTranslateClient();

  // Make sure the handlers are not set.
  EXPECT_EQ(nil, translate_client->language_selection_handler());
  EXPECT_EQ(nil, translate_client->translate_option_selection_handler());
  EXPECT_EQ(nil, translate_client->translate_notification_handler());

  LegacyTranslateInfobarMediator* translate_infobar_mediator = mediator();
  translate_infobar_mediator.webStateList = web_state_list();

  EXPECT_EQ(selection_handler(),
            translate_client->language_selection_handler());
  EXPECT_EQ(selection_handler(),
            translate_client->translate_option_selection_handler());
  EXPECT_EQ(notification_handler(),
            translate_client->translate_notification_handler());

  CreateTranslateClient();
  ChromeIOSTranslateClient* new_translate_client = GetTranslateClient();
  EXPECT_NE(new_translate_client, translate_client);

  EXPECT_EQ(selection_handler(),
            new_translate_client->language_selection_handler());
  EXPECT_EQ(selection_handler(),
            new_translate_client->translate_option_selection_handler());
  EXPECT_EQ(notification_handler(),
            new_translate_client->translate_notification_handler());
}

// Tests that the mediator sets the expected menu items for the translate
// options popup menu on its consumer.
TEST_F(TranslateInfobarMediatorTest, TranslateOptionMenuItems) {
  // Set up what TranslateInfoBarDelegate should return.
  EXPECT_CALL(*GetDelegate(), original_language_name())
      .WillRepeatedly(testing::Return(base::UTF8ToUTF16("French")));
  EXPECT_CALL(*GetDelegate(), ShouldAlwaysTranslate())
      .WillOnce(testing::Return(true));

  LegacyTranslateInfobarMediator* translate_infobar_mediator = mediator();
  translate_infobar_mediator.type =
      TranslatePopupMenuTypeTranslateOptionSelection;
  translate_infobar_mediator.infobarDelegate = GetDelegate();
  TestPopupMenuConsumer* consumer = [[TestPopupMenuConsumer alloc] init];
  translate_infobar_mediator.consumer = consumer;

  ASSERT_EQ(5U, consumer.items.count);

  TranslatePopupMenuItem* firstItem =
      base::mac::ObjCCastStrict<TranslatePopupMenuItem>(consumer.items[0]);
  EXPECT_EQ(PopupMenuActionChangeTargetLanguage, firstItem.actionIdentifier);
  EXPECT_FALSE(firstItem.selected);

  TranslatePopupMenuItem* secondItem =
      base::mac::ObjCCastStrict<TranslatePopupMenuItem>(consumer.items[1]);
  EXPECT_EQ(PopupMenuActionAlwaysTranslateSourceLanguage,
            secondItem.actionIdentifier);
  EXPECT_TRUE(secondItem.selected);

  TranslatePopupMenuItem* thirdItem =
      base::mac::ObjCCastStrict<TranslatePopupMenuItem>(consumer.items[2]);
  EXPECT_EQ(PopupMenuActionNeverTranslateSourceLanguage,
            thirdItem.actionIdentifier);
  EXPECT_FALSE(thirdItem.selected);

  TranslatePopupMenuItem* fourthItem =
      base::mac::ObjCCastStrict<TranslatePopupMenuItem>(consumer.items[3]);
  EXPECT_EQ(PopupMenuActionNeverTranslateSite, fourthItem.actionIdentifier);
  EXPECT_FALSE(fourthItem.selected);

  TranslatePopupMenuItem* fifthItem =
      base::mac::ObjCCastStrict<TranslatePopupMenuItem>(consumer.items[4]);
  EXPECT_EQ(PopupMenuActionChangeSourceLanguage, fifthItem.actionIdentifier);
  EXPECT_FALSE(fifthItem.selected);
}

// Tests that the mediator sets the expected menu items for the language
// selection popup menu on its consumer.
TEST_F(TranslateInfobarMediatorTest, LanguageSelectionMenuItems) {
  // Set up what TranslateInfoBarDelegate should return.
  EXPECT_CALL(*GetDelegate(), num_languages())
      .WillRepeatedly(testing::Return(3ul));
  EXPECT_CALL(*GetDelegate(), language_code_at(0))
      .WillOnce(testing::Return("en"));
  EXPECT_CALL(*GetDelegate(), language_name_at(0))
      .WillOnce(testing::Return(base::UTF8ToUTF16("English")));
  EXPECT_CALL(*GetDelegate(), language_code_at(2))
      .WillOnce(testing::Return("fr"));
  EXPECT_CALL(*GetDelegate(), language_name_at(2))
      .WillOnce(testing::Return(base::UTF8ToUTF16("French")));

  LegacyTranslateInfobarMediator* translate_infobar_mediator = mediator();
  translate_infobar_mediator.type = TranslatePopupMenuTypeLanguageSelection;
  translate_infobar_mediator.infobarDelegate = GetDelegate();
  translate_infobar_mediator.unavailableLanguageIndex = 1;
  TestPopupMenuConsumer* consumer = [[TestPopupMenuConsumer alloc] init];
  translate_infobar_mediator.consumer = consumer;

  ASSERT_EQ(2U, consumer.items.count);

  SelectLanguagePopupMenuItem* firstItem =
      base::mac::ObjCCastStrict<SelectLanguagePopupMenuItem>(consumer.items[0]);
  EXPECT_EQ(PopupMenuActionSelectLanguage, firstItem.actionIdentifier);
  EXPECT_FALSE(firstItem.selected);
  EXPECT_TRUE([firstItem.languageCode isEqualToString:@"en"]);
  EXPECT_TRUE([firstItem.title isEqualToString:@"English"]);

  SelectLanguagePopupMenuItem* secondItem =
      base::mac::ObjCCastStrict<SelectLanguagePopupMenuItem>(consumer.items[1]);
  EXPECT_EQ(PopupMenuActionSelectLanguage, secondItem.actionIdentifier);
  EXPECT_FALSE(secondItem.selected);
  EXPECT_TRUE([secondItem.languageCode isEqualToString:@"fr"]);
  EXPECT_TRUE([secondItem.title isEqualToString:@"French"]);
}
