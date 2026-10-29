// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_view_controller.h"

#import <string_view>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr CGFloat kExpectedIconSize = 24.0;
constexpr CGFloat kExpectedIconBorderWidth = 0.0;
constexpr CGFloat kExpectedIconCornerRadius = 0.0;
constexpr CGRect kTestHostViewFrame = {{0.0, 0.0}, {390.0, 844.0}};

constexpr char kGmailUrl1[] = "https://mail.google.com/1";
constexpr char kGmailUrl2[] = "https://mail.google.com/2";
constexpr char kPhotoUrl1[] = "https://photos.google.com/1";

NSString* const kGmailGroupTitle = @"Gmail";
NSString* const kPhotosGroupTitle = @"Photos";
NSString* const kGmailItem1Title = @"Gmail message 1";
NSString* const kGmailItem2Title = @"Gmail message 2";
NSString* const kPhotoItemTitle = @"Saved photo 1";
NSString* const kTestSubtitle = @"Order · AN-147338";
NSString* const kRowAccessibilityLabelFormat = @"%@, %@";

// Creates a test `AutofillAiSourceItem` with dummy data.
AutofillAiSourceItem* CreateTestSourceItem(NSString* title,
                                           std::string_view url_string) {
  return
      [[AutofillAiSourceItem alloc] initWithTitle:title
                                         subtitle:nil
                                              URL:GURL(url_string)
                                             type:AutofillAiSourceType::kGmail
                                             icon:[[UIImage alloc] init]];
}

}  // namespace

// Fake delegate capturing callbacks from `AutofillAiSourcesViewController`.
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

    item1_ = CreateTestSourceItem(kGmailItem1Title, kGmailUrl1);
    item2_ = CreateTestSourceItem(kGmailItem2Title, kGmailUrl2);
    photo_item_ = CreateTestSourceItem(kPhotoItemTitle, kPhotoUrl1);

    AutofillAiSourceGroup* gmail_group =
        [[AutofillAiSourceGroup alloc] initWithTitle:kGmailGroupTitle
                                               items:@[ item1_, item2_ ]];
    AutofillAiSourceGroup* photos_group =
        [[AutofillAiSourceGroup alloc] initWithTitle:kPhotosGroupTitle
                                               items:@[ photo_item_ ]];

    groups_ = @[ gmail_group, photos_group ];
    view_controller_ =
        [[AutofillAiSourcesViewController alloc] initWithSubtitle:kTestSubtitle
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

// Tests that the view hierarchy, table configuration, compact Footnote section
// header views, and accessibility identifiers are configured properly.
TEST_F(AutofillAiSourcesViewControllerTest, TestViewHierarchyAndSections) {
  [view_controller_ loadViewIfNeeded];

  UITableView* table_view = view_controller_.tableView;
  ASSERT_NE(table_view, nil);
  EXPECT_NSEQ(table_view.accessibilityIdentifier,
              kAutofillAISourcesTableViewId);
  EXPECT_FALSE(table_view.alwaysBounceVertical);
  EXPECT_NE(table_view.tableHeaderView, nil);
  EXPECT_EQ(table_view.sectionHeaderTopPadding, 0);

  UIView* title_view = view_controller_.navigationItem.titleView;
  ASSERT_NE(title_view, nil);
  UILabel* title_label = chrome_test_util::FindLabelWithText(
      title_view, l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_TITLE));
  ASSERT_NE(title_label, nil);
  EXPECT_NE(title_label.accessibilityTraits & UIAccessibilityTraitHeader, 0u);

  UIBarButtonItem* close_button =
      view_controller_.navigationItem.rightBarButtonItem;
  ASSERT_NE(close_button, nil);
  EXPECT_NSEQ(close_button.accessibilityIdentifier,
              kAutofillAISourcesCancelButtonId);

  id<UITableViewDataSource> data_source = table_view.dataSource;
  ASSERT_NE(data_source, nil);

  EXPECT_EQ([data_source numberOfSectionsInTableView:table_view], 2);
  EXPECT_EQ([data_source tableView:table_view numberOfRowsInSection:0], 2);
  EXPECT_EQ([data_source tableView:table_view numberOfRowsInSection:1], 1);

  id<UITableViewDelegate> delegate = table_view.delegate;
  ASSERT_NE(delegate, nil);

  UIView* gmail_header = [delegate tableView:table_view
                      viewForHeaderInSection:0];
  ASSERT_NE(gmail_header, nil);
  UILabel* gmail_label =
      chrome_test_util::FindLabelWithText(gmail_header, kGmailGroupTitle);
  ASSERT_NE(gmail_label, nil);
  EXPECT_NSEQ(gmail_label.font,
              [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]);
  EXPECT_NSEQ(gmail_label.textColor, [UIColor colorNamed:kTextSecondaryColor]);
  EXPECT_NE(gmail_label.accessibilityTraits & UIAccessibilityTraitHeader, 0u);

  UIView* photos_header = [delegate tableView:table_view
                       viewForHeaderInSection:1];
  ASSERT_NE(photos_header, nil);
  UILabel* photos_label =
      chrome_test_util::FindLabelWithText(photos_header, kPhotosGroupTitle);
  ASSERT_NE(photos_label, nil);
  EXPECT_NSEQ(photos_label.font,
              [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]);
  EXPECT_NSEQ(photos_label.textColor, [UIColor colorNamed:kTextSecondaryColor]);
  EXPECT_NE(photos_label.accessibilityTraits & UIAccessibilityTraitHeader, 0u);
}

// Tests that row cells use `TableViewCellContentConfiguration` with an
// unbordered `ImageContentConfiguration` leading icon, quaternary external-link
// accessory, text-aligned separator insets, and link/new-tab accessibility
// metadata.
TEST_F(AutofillAiSourcesViewControllerTest, TestCellConfiguration) {
  [view_controller_ loadViewIfNeeded];
  view_controller_.view.frame = kTestHostViewFrame;
  [view_controller_.view layoutIfNeeded];

  UITableView* table_view = view_controller_.tableView;
  ASSERT_NE(table_view, nil);

  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:0];
  TableViewCell* cell = base::apple::ObjCCastStrict<TableViewCell>(
      [table_view cellForRowAtIndexPath:index_path]);
  ASSERT_NE(cell, nil);

  TableViewCellContentConfiguration* config =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(config, nil);
  EXPECT_NSEQ(config.title, kGmailItem1Title);
  EXPECT_EQ(cell.separatorInset.left, [config separatorInsets].left);

  ImageContentConfiguration* leading_config =
      base::apple::ObjCCastStrict<ImageContentConfiguration>(
          config.leadingConfiguration);
  ASSERT_NE(leading_config, nil);
  EXPECT_EQ(leading_config.imageSize.width, kExpectedIconSize);
  EXPECT_EQ(leading_config.imageSize.height, kExpectedIconSize);
  EXPECT_EQ(leading_config.imageCornerRadius, kExpectedIconCornerRadius);
  EXPECT_NSEQ(leading_config.imageTintColor,
              [UIColor colorNamed:kTextSecondaryColor]);

  UIView<ChromeContentView>* icon_view = [leading_config makeChromeContentView];
  ASSERT_NE(icon_view, nil);
  EXPECT_EQ(icon_view.layer.borderWidth, kExpectedIconBorderWidth);

  UIImageView* accessory_view =
      base::apple::ObjCCastStrict<UIImageView>(cell.accessoryView);
  ASSERT_NE(accessory_view, nil);
  EXPECT_NSEQ(accessory_view.tintColor,
              [UIColor colorNamed:kTextQuaternaryColor]);

  NSString* expected_accessibility_label = [NSString
      stringWithFormat:kRowAccessibilityLabelFormat, kGmailItem1Title,
                       l10n_util::GetNSString(IDS_IOS_OPENS_IN_NEW_TAB)];
  EXPECT_NSEQ(cell.accessibilityLabel, expected_accessibility_label);
  EXPECT_EQ(cell.accessibilityHint, nil);
  EXPECT_NE(cell.accessibilityTraits & UIAccessibilityTraitLink, 0u);
}

// Tests that `-preferredHeightForContent` scales dynamically with the number of
// source groups and rows, and that `-preferredHeightDetent` creates a custom
// detent.
TEST_F(AutofillAiSourcesViewControllerTest,
       TestPreferredHeightForContentScalesWithContent) {
  AutofillAiSourceGroup* single_group =
      [[AutofillAiSourceGroup alloc] initWithTitle:kGmailGroupTitle
                                             items:@[ item1_ ]];
  AutofillAiSourcesViewController* single_row_view_controller =
      [[AutofillAiSourcesViewController alloc]
          initWithSubtitle:kTestSubtitle
                    groups:@[ single_group ]];
  UINavigationController* single_navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:single_row_view_controller];
  single_navigation_controller.view.frame = kTestHostViewFrame;
  [single_navigation_controller.view layoutIfNeeded];

  UINavigationController* multi_navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:view_controller_];
  multi_navigation_controller.view.frame = kTestHostViewFrame;
  [multi_navigation_controller.view layoutIfNeeded];

  CGFloat single_height =
      [single_row_view_controller preferredHeightForContent];
  CGFloat multi_height = [view_controller_ preferredHeightForContent];

  EXPECT_GT(single_height,
            single_navigation_controller.navigationBar.frame.size.height);
  EXPECT_GT(multi_height, single_height);
  EXPECT_NE([view_controller_ preferredHeightDetent], nil);
}

// Tests that selecting a row calls the delegate with the selected source item.
TEST_F(AutofillAiSourcesViewControllerTest, TestDidSelectRow) {
  [view_controller_ loadViewIfNeeded];

  id<UITableViewDelegate> delegate = view_controller_.tableView.delegate;
  ASSERT_NE(delegate, nil);

  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:1];
  [delegate tableView:view_controller_.tableView
      didSelectRowAtIndexPath:index_path];

  EXPECT_EQ(fake_delegate_.selectedItem, photo_item_);
}

// Tests that initializing without a subtitle sets a single `UILabel` with the
// header accessibility trait as the navigation item's `titleView`.
TEST_F(AutofillAiSourcesViewControllerTest, TestInitializationWithoutSubtitle) {
  AutofillAiSourcesViewController* sources_view_controller =
      [[AutofillAiSourcesViewController alloc] initWithSubtitle:nil
                                                         groups:groups_];
  [sources_view_controller loadViewIfNeeded];

  UILabel* title_label = base::apple::ObjCCastStrict<UILabel>(
      sources_view_controller.navigationItem.titleView);
  ASSERT_NE(title_label, nil);
  EXPECT_NSEQ(title_label.text,
              l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_TITLE));
  EXPECT_NE(title_label.accessibilityTraits & UIAccessibilityTraitHeader, 0u);
}
