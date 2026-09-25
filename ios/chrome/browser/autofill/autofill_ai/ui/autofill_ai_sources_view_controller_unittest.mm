// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Creates a test source item with dummy data.
AutofillAiSourceItem* CreateTestSourceItem(NSString* title,
                                           const std::string& url_string) {
  return
      [[AutofillAiSourceItem alloc] initWithTitle:title
                                         subtitle:nil
                                              URL:GURL(url_string)
                                             type:AutofillAiSourceType::kGmail
                                             icon:[[UIImage alloc] init]];
}

}  // namespace

// Fake delegate capturing callbacks from AutofillAiSourcesViewController.
@interface FakeAutofillAiSourcesViewControllerDelegate
    : NSObject <AutofillAiSourcesViewControllerDelegate>
@property(nonatomic, assign) BOOL didDismissCalled;
@property(nonatomic, strong) AutofillAiSourceItem* selectedItem;
@end

@implementation FakeAutofillAiSourcesViewControllerDelegate

- (void)sourcesViewController:(AutofillAiSourcesViewController*)viewController
          didSelectSourceItem:(AutofillAiSourceItem*)sourceItem {
  _selectedItem = sourceItem;
}

- (void)sourcesViewControllerDidDismiss:
    (AutofillAiSourcesViewController*)viewController {
  _didDismissCalled = YES;
}

@end

class AutofillAiSourcesViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    item1_ =
        CreateTestSourceItem(@"Gmail message 1", "https://mail.google.com/1");
    item2_ =
        CreateTestSourceItem(@"Gmail message 2", "https://mail.google.com/2");
    photo_item_ =
        CreateTestSourceItem(@"Saved photo 1", "https://photos.google.com/1");

    AutofillAiSourceGroup* gmail_group =
        [[AutofillAiSourceGroup alloc] initWithTitle:@"Gmail"
                                               items:@[ item1_, item2_ ]];
    AutofillAiSourceGroup* photos_group =
        [[AutofillAiSourceGroup alloc] initWithTitle:@"Photos"
                                               items:@[ photo_item_ ]];

    groups_ = @[ gmail_group, photos_group ];
    view_controller_ = [[AutofillAiSourcesViewController alloc]
        initWithSubtitle:@"Order · AN-147338"
                  groups:groups_];
    fake_delegate_ = [[FakeAutofillAiSourcesViewControllerDelegate alloc] init];
    view_controller_.delegate = fake_delegate_;
  }

  AutofillAiSourceItem* item1_;
  AutofillAiSourceItem* item2_;
  AutofillAiSourceItem* photo_item_;
  NSArray<AutofillAiSourceGroup*>* groups_;
  AutofillAiSourcesViewController* view_controller_;
  FakeAutofillAiSourcesViewControllerDelegate* fake_delegate_;
};

// Tests view hierarchy, table configuration, and accessibility identifiers.
TEST_F(AutofillAiSourcesViewControllerTest, TestViewHierarchyAndSections) {
  [view_controller_ loadViewIfNeeded];

  UITableView* tableView = view_controller_.tableView;
  ASSERT_NE(tableView, nil);
  EXPECT_NSEQ(tableView.accessibilityIdentifier, kAutofillAISourcesTableViewId);
  EXPECT_NE(tableView.tableHeaderView, nil);
  EXPECT_EQ(tableView.sectionHeaderTopPadding, 0);

  UIBarButtonItem* closeButton =
      view_controller_.navigationItem.rightBarButtonItem;
  ASSERT_NE(closeButton, nil);
  EXPECT_NSEQ(closeButton.accessibilityIdentifier,
              kAutofillAISourcesCancelButtonId);

  id<UITableViewDataSource> dataSource = tableView.dataSource;
  ASSERT_NE(dataSource, nil);

  EXPECT_EQ([dataSource numberOfSectionsInTableView:tableView], 2);
  EXPECT_EQ([dataSource tableView:tableView numberOfRowsInSection:0], 2);
  EXPECT_EQ([dataSource tableView:tableView numberOfRowsInSection:1], 1);
  EXPECT_NSEQ([dataSource tableView:tableView titleForHeaderInSection:0],
              @"Gmail");
  EXPECT_NSEQ([dataSource tableView:tableView titleForHeaderInSection:1],
              @"Photos");
}

// Tests row cell content configuration and accessory view.
TEST_F(AutofillAiSourcesViewControllerTest, TestCellConfiguration) {
  [view_controller_ loadViewIfNeeded];

  UITableView* tableView = view_controller_.tableView;
  id<UITableViewDataSource> dataSource = tableView.dataSource;
  ASSERT_NE(dataSource, nil);

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  UITableViewCell* cell = [dataSource tableView:tableView
                          cellForRowAtIndexPath:indexPath];
  ASSERT_NE(cell, nil);

  UIListContentConfiguration* config =
      base::apple::ObjCCastStrict<UIListContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(config, nil);
  EXPECT_NSEQ(config.text, @"Gmail message 1");
  EXPECT_NE(cell.accessoryView, nil);
}

// Tests selecting a row calls the delegate with the selected source item.
TEST_F(AutofillAiSourcesViewControllerTest, TestDidSelectRow) {
  [view_controller_ loadViewIfNeeded];

  id<UITableViewDelegate> delegate = view_controller_.tableView.delegate;
  ASSERT_NE(delegate, nil);

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:1];
  [delegate tableView:view_controller_.tableView
      didSelectRowAtIndexPath:indexPath];

  EXPECT_EQ(fake_delegate_.selectedItem, photo_item_);
}

// Tests initializing without subtitle formats navigation titleView correctly.
TEST_F(AutofillAiSourcesViewControllerTest, TestInitializationWithoutSubtitle) {
  AutofillAiSourcesViewController* vc =
      [[AutofillAiSourcesViewController alloc] initWithSubtitle:nil
                                                         groups:groups_];
  [vc loadViewIfNeeded];

  UILabel* titleLabel =
      base::apple::ObjCCastStrict<UILabel>(vc.navigationItem.titleView);
  ASSERT_NE(titleLabel, nil);
  EXPECT_NSEQ(titleLabel.text,
              l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_TITLE));
}
