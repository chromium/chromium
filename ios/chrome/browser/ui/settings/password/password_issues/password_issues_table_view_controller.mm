// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"

#import <UIKit/UIKit.h>
#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue_content_item.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::WarningType;

namespace {

// Vertical spacing between password issue items.
constexpr CGFloat kVerticalSpacingBetweenItems = 8;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierDismissedCredentialsButton,
  // Identifier of the section containing the first password issue when Password
  // Checkup is enabled. Subsequent password issues use incremental section
  // identifiers, as each issue goes in a separate section. To avoid section
  // identifiers collisions, `SectionIdentifierFirstPasswordIssue` should be
  // the last value in `SectionIdentifier` and any new identifiers must be
  // defined above it.
  SectionIdentifierFirstPasswordIssue,
  // Do not add more values here. See comment above.
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypePassword,        // This is a repeated item type.
  ItemTypePasswordHeader,  // This is a repeated item type.
  ItemTypeChangePassword,  // This is a repeated item type.
  ItemTypeDismissedCredentialsButton,
};

}  // namespace

@interface PasswordIssuesTableViewController () {
  // Text of the header displayed on top of the page.
  NSString* _headerText;
  // URL of link in the page header. Nullable.
  CrURL* _headerURL;
  // Insecure password issues displayed in the tableView.
  // Reused password issues are displayed in groups of same-password credentials
  // with a text header on top of the first password issue in the group. All
  // other types of issues are displayed in the same group without header.
  NSArray<PasswordIssueGroup*>* _passwordGroups;
  // Number in the button for presenting dismissed compromised
  // credential warnings. When zero, no button is displayed.
  NSInteger _dismissedWarningsCount;
  // Type of insecure credentials displayed in the page.
  WarningType _warningType;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation PasswordIssuesTableViewController

- (instancetype)initWithWarningType:(WarningType)warningType {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _warningType = warningType;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordIssuesTableViewID;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presenter dismissPasswordIssuesTableViewController];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  TableViewLinkHeaderFooterItem* headerItem = [self headerItem];

  if (headerItem) {
    [model addSectionWithIdentifier:SectionIdentifierHeader];
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierHeader];
  }

  // Add password issues to their own separate sections.
  __block NSInteger nextPasswordIssueSectionIdentifier =
      SectionIdentifierFirstPasswordIssue;
  for (PasswordIssueGroup* issueGroup in _passwordGroups) {
    [issueGroup.passwordIssues
        enumerateObjectsUsingBlock:^(PasswordIssue* passwordIssue,
                                     NSUInteger index, BOOL* stop) {
          // Create section for next password issue.
          [model addSectionWithIdentifier:nextPasswordIssueSectionIdentifier];

          // Add header on top of first issue if the issue group has a header.
          if (index == 0 && issueGroup.headerText) {
            [model setHeader:[self passwordIssueGroupHeaderItemWithText:
                                       issueGroup.headerText]
                forSectionWithIdentifier:nextPasswordIssueSectionIdentifier];
          }

          // Add password issue.
          [model addItem:[self passwordIssueItem:passwordIssue]
              toSectionWithIdentifier:nextPasswordIssueSectionIdentifier];

          if (passwordIssue.changePasswordURL.has_value()) {
            // Add change password button below password issue.
            [model addItem:[self changePasswordItem]
                toSectionWithIdentifier:nextPasswordIssueSectionIdentifier];
          }

          // Increment section identifier for next password issue.
          nextPasswordIssueSectionIdentifier++;
        }];
  }

  TableViewMultiDetailTextItem* dismissedWarningsItem =
      [self dismissedWarningsItem];
  if (dismissedWarningsItem) {
    [model
        addSectionWithIdentifier:SectionIdentifierDismissedCredentialsButton];
    [model addItem:dismissedWarningsItem
        toSectionWithIdentifier:SectionIdentifierDismissedCredentialsButton];
  }
}

#pragma mark - Items

- (TableViewLinkHeaderFooterItem*)headerItem {
  if (!_headerText) {
    return nil;
  }

  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = _headerText;

  if (_headerURL) {
    headerItem.urls = @[ _headerURL ];
  }

  return headerItem;
}

- (PasswordIssueContentItem*)passwordIssueItem:(PasswordIssue*)password {
  PasswordIssueContentItem* passwordItem =
      [[PasswordIssueContentItem alloc] initWithType:ItemTypePassword];
  passwordItem.password = password;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

// Creates a header for displaying on top of a group of password issues.
- (TableViewLinkHeaderFooterItem*)passwordIssueGroupHeaderItemWithText:
    (NSString*)headerText {
  TableViewLinkHeaderFooterItem* groupHeaderItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypePasswordHeader];
  groupHeaderItem.text = headerText;
  return groupHeaderItem;
}

// Creates an item acting as a button for changing an insecure password in its
// corresponding website.
- (TableViewTextItem*)changePasswordItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeChangePassword];
  item.text = l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

// Creates the item acting as a button for presenting dismissed compromised
// credential warnings. Returns nil when `_dismissedWarningsCount` is zero.
- (TableViewMultiDetailTextItem*)dismissedWarningsItem {
  // The button is not visible either because there aren't dismissed compromised
  // credentials or because the view controller is not showing compromised
  // credentials.
  if (_dismissedWarningsCount == 0) {
    return nil;
  }

  TableViewMultiDetailTextItem* dismissedWarningsItem =
      [[TableViewMultiDetailTextItem alloc]
          initWithType:ItemTypeDismissedCredentialsButton];
  dismissedWarningsItem.text = l10n_util::GetNSString(
      IDS_IOS_COMPROMISED_PASSWORD_ISSUES_DISMISSED_WARNINGS_BUTTON_TITLE);
  dismissedWarningsItem.trailingDetailText =
      [@(_dismissedWarningsCount) stringValue];
  dismissedWarningsItem.accessibilityTraits = UIAccessibilityTraitButton;
  dismissedWarningsItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  dismissedWarningsItem.accessibilityIdentifier = kDismissedWarningsCellID;
  return dismissedWarningsItem;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewModel* model = self.tableViewModel;
  ItemType itemType =
      static_cast<ItemType>([model itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeHeader:
    case ItemTypePasswordHeader:
      break;
    case ItemTypePassword: {
      PasswordIssueContentItem* passwordIssue =
          base::apple::ObjCCastStrict<PasswordIssueContentItem>(
              [model itemAtIndexPath:indexPath]);
      base::RecordAction(
          base::UserMetricsAction("MobilePasswordIssuesOpenPasswordDetails"));
      [self.presenter presentPasswordIssueDetails:passwordIssue.password];
      break;
    }
    case ItemTypeDismissedCredentialsButton:
      password_manager::LogOpenPasswordIssuesList(
          WarningType::kDismissedWarningsWarning);
      [self.presenter presentDismissedCompromisedCredentials];
      break;

    case ItemTypeChangePassword:
      password_manager::LogChangePasswordOnWebsite(_warningType);
      CrURL* changePasswordURL =
          [self changePasswordURLForPasswordInSection:indexPath.section];
      [self.presenter dismissAndOpenURL:changePasswordURL];
      break;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypePassword: {
      TableViewURLCell* urlCell =
          base::apple::ObjCCastStrict<TableViewURLCell>(cell);
      urlCell.textLabel.lineBreakMode = NSLineBreakByTruncatingHead;
      // Load the favicon from cache.
      [self loadFaviconAtIndexPath:indexPath forCell:cell];
      break;
    }
  }
  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];

  if (section == 0 && [self.tableViewModel headerForSectionIndex:0]) {
    // Attach self as delegate to handle clicks in page header.
    TableViewLinkHeaderFooterView* headerView =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    headerView.delegate = self;
  }

  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  TableViewModel* model = self.tableViewModel;
  // Calculate the actual height of the footer view if there's one.
  if ([model footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }

  NSInteger sectionIdentifier =
      [model sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierHeader:
      // Add an empty footer of 8pt height so added up to the table view header
      // bottom padding (8pt) and the first section's header (either an empty
      // 8pt header or an actual header that has a 8pt top padding) achieves the
      // desired 24pt spacing between the table view header and the next element
      // below it.
      return kVerticalSpacingBetweenItems;

    case SectionIdentifierDismissedCredentialsButton:
      // Spacing between dismiss button and the bottom of the scrollable area.
      return kVerticalSpacingBetweenItems;

    default:
      // Handle password issue sections.
      // All other sections should be handled by now.
      CHECK_GE(sectionIdentifier, SectionIdentifierFirstPasswordIssue);

      if (section + 1 < model.numberOfSections) {
        // When the next section doesn't have a header, the desired spacing is
        // achieved via an empty header. If there's a header, it includes an 8pt
        // top padding, so we add a 16pt empty footer to achieve the desired
        // spacing of 24pt.
        return [model headerForSectionIndex:section + 1]
                   ? kVerticalSpacingBetweenItems * 2
                   : 0;
      }

      // Vertical spacing between the last item and its container.
      return kVerticalSpacingBetweenItems;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  TableViewModel* model = self.tableViewModel;

  // Calculate the actual height of the header view if there's one.
  if ([model headerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }

  NSInteger sectionIdentifier =
      [model sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierHeader:
      // This section always has a header.
      NOTREACHED();

    case SectionIdentifierDismissedCredentialsButton:
      // Spacing to last password issue.
      return 3 * kVerticalSpacingBetweenItems;

    default:
      // Handle password issue sections.
      // All other sections should be handled by now.
      CHECK_GE(sectionIdentifier, SectionIdentifierFirstPasswordIssue);
      // Add an empty header to achieve the desired spacing to the element
      // above.
      return kVerticalSpacingBetweenItems;
  }
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  TableViewURLItem* URLItem =
      base::apple::ObjCCastStrict<TableViewURLItem>(item);
  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);

  NSString* itemIdentifier = URLItem.uniqueIdentifier;
  [self.imageDataSource
      faviconForPageURL:URLItem.URL
             completion:^(FaviconAttributes* attributes) {
               // Only set favicon if the cell hasn't been reused.
               if ([URLCell.cellUniqueIdentifier
                       isEqualToString:itemIdentifier]) {
                 DCHECK(attributes);
                 [URLCell.faviconView configureWithAttributes:attributes];
               }
             }];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordIssuesSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordIssuesSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

#pragma mark - PasswordIssuesConsumer

- (void)setPasswordIssues:(NSArray<PasswordIssueGroup*>*)passwordGroups
    dismissedWarningsCount:(NSInteger)dismissedWarnings {
  _passwordGroups = passwordGroups;
  _dismissedWarningsCount = dismissedWarnings;
  [self reloadData];

  // User removed/resolved all issues, dismiss the vc and go back to the
  // previous screen.
  if (passwordGroups.count == 0 && dismissedWarnings == 0) {
    [self.presenter dismissAfterAllIssuesGone];
  }
}

- (void)setNavigationBarTitle:(NSString*)title {
  self.title = title;
}

- (void)setHeader:(NSString*)text URL:(CrURL*)URL {
  _headerText = text;
  _headerURL = URL;

  [self reloadData];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.presenter dismissAndOpenURL:URL];
}

#pragma mark - Private

// Helper for getting the url for changing the password of the password issue
// item in the given tableView section.
- (CrURL*)changePasswordURLForPasswordInSection:(NSInteger)section {
  PasswordIssueContentItem* passwordIssueItem =
      base::apple::ObjCCastStrict<PasswordIssueContentItem>([self.tableViewModel
          itemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:section]]);

  CHECK(passwordIssueItem.password.changePasswordURL.has_value());
  return passwordIssueItem.password.changePasswordURL.value();
}

@end
