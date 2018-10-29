// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/compose_email_handler_collection_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/web/fake_mailto_handler_helpers.h"
#import "ios/chrome/browser/web/mailto_handler_manager.h"
#import "ios/chrome/browser/web/mailto_handler_system_mail.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MDCPalettes.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - ComposeEmailHandlerCollectionViewControllerTest

class ComposeEmailHandlerCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  // Before CreateController() is called, set |handers_| and optionally
  // |default_handler_id_| ivars. They will be used to seed the construction of
  // a MailtoHandlerManager which in turn used for the construction of the
  // CollectionViewController.
  CollectionViewController* InstantiateController() override {
    manager_ = [MailtoHandlerManager mailtoHandlerManagerWithStandardHandlers];
    // Clears the state so unit tests start from a known state.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kMailtoHandlerManagerUserDefaultsKey];
    [manager_ setDefaultHandlers:handlers_];
    if (default_handler_id_)
      [manager_ setDefaultHandlerID:default_handler_id_];
    return [[ComposeEmailHandlerCollectionViewController alloc]
        initWithManager:manager_];
  }

  // |handlers_| and |default_handler_id_| must be set before first call to
  // CreateController().
  NSArray<MailtoHandler*>* handlers_;
  NSString* default_handler_id_;
  MailtoHandlerManager* manager_;
};

TEST_F(ComposeEmailHandlerCollectionViewControllerTest, TestConstructor) {
  handlers_ = @[
    [[MailtoHandlerSystemMail alloc] init],
    [[FakeMailtoHandlerGmailInstalled alloc] init],
    [[FakeMailtoHandlerForTesting alloc] init]
  ];
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_COMPOSE_EMAIL_SETTING);

  // Checks that there are two sections: one with all the available
  // MailtoHandler objects listed and a second one with the "Always Ask" toggle.
  ASSERT_EQ(2, NumberOfSections());
  // Array returned by -defaultHandlers is sorted by the name of the Mail app
  // and may not be in the same order as |handlers_|.
  NSArray<MailtoHandler*>* handlers = [manager_ defaultHandlers];
  int number_of_handlers = [handlers count];
  EXPECT_EQ(number_of_handlers, NumberOfItemsInSection(0));
  for (int index = 0; index < number_of_handlers; ++index) {
    MailtoHandler* handler = handlers[index];
    SettingsTextItem* item = GetCollectionViewItem(0, index);
    // Checks that the title displayed is the name of the MailtoHandler.
    EXPECT_NSEQ([handler appName], item.text);
    EXPECT_FALSE(item.detailText);
    // The enable/disable state of each Mail client app depends on the state
    // of the "Always Ask" toggle. All rows should be disabled if user has
    // not selected a default Mail client app.
    BOOL is_enabled = [manager_ defaultHandlerID] != nil;
    // Checks that text cells are displayed differently depending on the
    // availability of the handlers.
    UIColor* darkest_tint = [[MDCPalette greyPalette] tint900];
    if (is_enabled && [handler isAvailable]) {
      EXPECT_EQ(darkest_tint, item.textColor);
      EXPECT_NE(UIAccessibilityTraitNotEnabled, item.accessibilityTraits);
    } else {
      EXPECT_NE(darkest_tint, item.textColor);
      EXPECT_EQ(UIAccessibilityTraitNotEnabled, item.accessibilityTraits);
    }
  }
  bool is_on = [manager_ defaultHandlerID] == nil;
  CheckSwitchCellStateAndTitleWithId(is_on, IDS_IOS_CHOOSE_EMAIL_ASK_TOGGLE, 1,
                                     0);
}

TEST_F(ComposeEmailHandlerCollectionViewControllerTest, TestSelection) {
  handlers_ = @[
    [[MailtoHandlerSystemMail alloc] init],
    [[FakeMailtoHandlerGmailInstalled alloc] init]
  ];
  // The UI will come up with the first handler listed in |handlers_|
  // in the selected state.
  default_handler_id_ = [handlers_[0] appStoreID];
  CreateController();
  CheckController();

  // Have an observer to make sure that selecting in the UI causes the
  // observer to be called.
  CountingMailtoHandlerManagerObserver* observer =
      [[CountingMailtoHandlerManagerObserver alloc] init];
  [manager_ setObserver:observer];

  // The array of |handlers| here is sorted for display and may not be in the
  // same order as |handlers_|. Finds another entry in the |handlers| that is
  // not currently selected and use that as the new selection. This test
  // must set up at least two handlers in |handlers_| which guarantees that
  // a new |selection| must be found, thus the DCHECK_GE.
  NSArray<MailtoHandler*>* handlers = [manager_ defaultHandlers];
  int selection = -1;
  int number_of_handlers = [handlers count];
  for (int index = 0; index < number_of_handlers; ++index) {
    if (![default_handler_id_ isEqualToString:[handlers[index] appStoreID]]) {
      selection = index;
      break;
    }
  }
  DCHECK_GE(selection, 0);
  // Trigger a selection in the Collection View Controller.
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:selection
                                                  inSection:0]];
  // Verify that the observer has been called and new selection has been set.
  EXPECT_EQ(1, [observer changeCount]);
  EXPECT_NSEQ([handlers[selection] appStoreID], [manager_ defaultHandlerID]);
}

// Tests the state of the mailto:// handler apps and as the "Always ask"
// switch is toggled.
TEST_F(ComposeEmailHandlerCollectionViewControllerTest, TestSwitchChanged) {
  handlers_ = @[
    [[MailtoHandlerSystemMail alloc] init],
    [[FakeMailtoHandlerGmailInstalled alloc] init]
  ];
  // No default handler.
  default_handler_id_ = nil;
  CreateController();
  CheckController();

  ComposeEmailHandlerCollectionViewController* test_view_controller =
      base::mac::ObjCCastStrict<ComposeEmailHandlerCollectionViewController>(
          controller());
  NSIndexPath* switch_index_path = [NSIndexPath indexPathForRow:0 inSection:1];
  LegacySettingsSwitchCell* switch_cell =
      base::mac::ObjCCastStrict<LegacySettingsSwitchCell>([test_view_controller
                  collectionView:[test_view_controller collectionView]
          cellForItemAtIndexPath:switch_index_path]);
  // Default state of the switch is ON so user is always prompted to make
  // a choice of which mailto:// handler app to use when tapping on a mailto://
  // URL.
  EXPECT_TRUE(switch_cell.switchView.on);

  // Toggling the switch to OFF and verify. Then check that none of the
  // mailto:// handler apps is checked. The list of Mail client apps should
  // reflect availability.
  switch_cell.switchView.on = NO;
  [switch_cell.switchView
      sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_FALSE(switch_cell.switchView.on);
  NSArray<MailtoHandler*>* handlers = [manager_ defaultHandlers];
  UIColor* darkest_tint = [[MDCPalette greyPalette] tint900];
  for (NSUInteger index = 0U; index < [handlers count]; ++index) {
    SettingsTextItem* item = GetCollectionViewItem(0, index);
    EXPECT_EQ(MDCCollectionViewCellAccessoryNone, item.accessoryType);
    MailtoHandler* handler = handlers[index];
    if ([handler isAvailable])
      EXPECT_EQ(darkest_tint, item.textColor);
    else
      EXPECT_NE(darkest_tint, item.textColor);
  }

  // Toggling the switch back ON and verify. The list of mailto:// handler apps
  // remain unchecked.
  switch_cell.switchView.on = YES;
  [switch_cell.switchView
      sendActionsForControlEvents:UIControlEventValueChanged];
  EXPECT_TRUE(switch_cell.switchView.on);
  handlers = [manager_ defaultHandlers];
  for (NSUInteger index = 0U; index < [handlers count]; ++index) {
    SettingsTextItem* item = GetCollectionViewItem(0, index);
    EXPECT_EQ(MDCCollectionViewCellAccessoryNone, item.accessoryType);
  }
}
