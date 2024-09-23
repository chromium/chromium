// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/uma/uma_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, UMASectionIdentifier) {
  // Main section.
  UMAMainSectionIdentifier = kSectionIdentifierEnumZero,
};

// List of items.
typedef NS_ENUM(NSInteger, UMAItemType) {
  // Checkbox item to enable/disable the UMA metrics.
  UMAItemTypeCheckbox = kItemTypeEnumZero,
  // Footer with the description about UMA metrics.
  UMAItemTypeFooter,
};

// Prefix used for bullet points in the UMA descriptions. This prefix is used to
// compute how many pixels the paragraph lines needs to be aligned with the
// first line. It is impportant all translations have the exact same prefix.
NSString* const kBulletPrefix = @"  • ";
// Begin and end tags to find bullet points paragraphs.
NSString* const kBegingIndentString = @"BEGIN_INDENT";
NSString* const kEndIndentString = @"END_INDENT";

// Parses a string with an embedded bold part inside, delineated by
// "BEGIN_INDENT" and "END_INDENT". Returns an attributed string with the right
// paragraph style.
NSMutableAttributedString* AddIndentAttributes(NSString* string,
                                               CGFloat indentSize) {
  StringWithTags parsedString =
      ParseStringWithTags(string, kBegingIndentString, kEndIndentString);
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.headIndent = indentSize;
  for (std::vector<NSRange>::iterator it = parsedString.ranges.begin();
       it != parsedString.ranges.end(); ++it) {
    [attributedString addAttribute:NSParagraphStyleAttributeName
                             value:paragraphStyle
                             range:*it];
  }
  return attributedString;
}

}  // namespace

@implementation UMATableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_UMA_DIALOG_TITLE);
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonAction:)];
  self.navigationItem.rightBarButtonItem = doneButton;
  [self loadModel];
}

#pragma mark - Actions

- (void)doneButtonAction:(id)sender {
  __weak __typeof(self) weakSelf = self;
  [self dismissViewControllerAnimated:YES
                           completion:^() {
                             [weakSelf.presentationDelegate
                                 UMATableViewControllerDidDismiss:self];
                           }];
}

- (void)switchAction:(UISwitch*)sender {
  TableViewModel* model = self.tableViewModel;
  NSIndexPath* indexPath = [model indexPathForItemType:sender.tag];
  DCHECK(indexPath);
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:indexPath]);
  DCHECK(switchItem);
  self.UMAReportingUserChoice = sender.isOn;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    switchCell.switchView.tag = item.type;
  }
  return cell;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  // Adds the section.
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:UMAMainSectionIdentifier];

  // Adds switch item.
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:UMAItemTypeCheckbox];
  switchItem.on = self.UMAReportingUserChoice;
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_UMA_DIALOG_CHECKBOX);
  switchItem.accessibilityIdentifier =
      kImproveChromeItemAccessibilityIdentifier;
  [model addItem:switchItem toSectionWithIdentifier:UMAMainSectionIdentifier];

  // Adds the footer.
  NSMutableDictionary* regularAttributes = [NSMutableDictionary dictionary];
  [regularAttributes
      setObject:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
         forKey:NSFontAttributeName];
  [regularAttributes setObject:[UIColor colorNamed:kTextSecondaryColor]
                        forKey:NSForegroundColorAttributeName];
  CGSize indentSize = [kBulletPrefix sizeWithAttributes:regularAttributes];
  NSMutableAttributedString* attributedString = AddIndentAttributes(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_UMA_DIALOG_EXPLANATION_NO_SYNC),
      indentSize.width);
  [attributedString
      addAttributes:regularAttributes
              range:NSMakeRange(0, attributedString.string.length)];
  TableViewAttributedStringHeaderFooterItem* headerItem =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:UMAItemTypeFooter];
  headerItem.attributedString = attributedString;
  [model setFooter:headerItem
      forSectionWithIdentifier:UMAMainSectionIdentifier];
}

@end
