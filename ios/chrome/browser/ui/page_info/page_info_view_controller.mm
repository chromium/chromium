// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/page_info/core/page_info_history_data_source.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_delegate.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_info.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSecurityContent,
  SectionIdentifierPermissions,
  SectionIdentifierAboutThisSite,
  SectionIdentifierLastVisited
};

typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierSecurity,
  ItemIdentifierPermissionsCamera,
  ItemIdentifierPermissionsMicrophone,
  ItemIdentifierAboutThisSite,
  ItemIdentifierLastVisited
};

// The minimum scale factor of the title label showing the URL.
const float kTitleLabelMinimumScaleFactor = 0.7f;

// The maximum number of lines we should show for a page's description in the
// AboutThisSite section.
const NSInteger kAboutThisSiteDetailTextNumberOfLines = 2;

}  // namespace

@interface PageInfoViewController () <TableViewLinkHeaderFooterItemDelegate>

// The page info security description.
@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

// The list of permissions info used to create switches.
@property(nonatomic, copy)
    NSMutableDictionary<NSNumber*, NSNumber*>* permissionsInfo;

@end

@implementation PageInfoViewController {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  PageInfoAboutThisSiteInfo* _aboutThisSiteInfo;
  NSString* _lastVisitedTimestamp;
}

#pragma mark - UIViewController

- (instancetype)initWithSiteSecurityDescription:
    (PageInfoSiteSecurityDescription*)siteSecurityDescription {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _pageInfoSecurityDescription = siteSecurityDescription;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);
  if (IsRevampPageInfoIosEnabled()) {
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    self.navigationItem.prompt = self.pageInfoSecurityDescription.siteURL;
  } else {
    self.navigationItem.titleView =
        [self titleViewLabelForURL:self.pageInfoSecurityDescription.siteURL];
  }

  self.tableView.accessibilityIdentifier = kPageInfoViewAccessibilityIdentifier;
  self.navigationController.navigationBar.accessibilityIdentifier =
      kPageInfoViewNavigationBarAccessibilityIdentifier;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.pageInfoCommandsHandler
                           action:@selector(hidePageInfo)];
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.tableView.separatorInset = UIEdgeInsetsMake(
      0,
      IsRevampPageInfoIosEnabled() ? kPageInfoTableViewSeparatorInsetWithIcon
                                   : kPageInfoTableViewSeparatorInset,
      0, 0);
  if (!IsRevampPageInfoIosEnabled()) {
    self.tableView.allowsSelection = NO;
  }

  if (self.pageInfoSecurityDescription.isEmpty) {
    [self addEmptyTableViewWithMessage:self.pageInfoSecurityDescription.message
                                 image:nil];
    self.tableView.alwaysBounceVertical = NO;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    return;
  }

  // Update the security information if it was fetched while the page was being
  // loaded. If page info is opened while the page is being loaded, its
  // certificates might not have been loaded yet and so the page is shown as
  // insecure. The next time the user opens page info, we should update the
  // security information when the page is fully loaded so it's up to date.
  if (self.pageInfoSecurityDescription.isPageLoading) {
    _pageInfoSecurityDescription =
        [self.pageInfoPresentationHandler updatedSiteSecurityDescription];
  }

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);
  RegisterTableViewCell<TableViewSwitchCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
      self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierSecurityContent) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierSecurity) ]];

  // Permissions section.
  for (NSNumber* permission in self.permissionsInfo.allKeys) {
    [self updateSnapshot:snapshot forPermission:permission];
  }

  if (IsAboutThisSiteFeatureEnabled()) {
    [self updateSnapshotForAboutThisSite:snapshot];
  }

  // Append cell for the Last Visited row only if the `kPageInfoLastVisitedIOS`
  // flag is enabled and the Last Visited timestamp is available.
  if (IsPageInfoLastVisitedIOSEnabled() && _lastVisitedTimestamp) {
    [snapshot
        appendSectionsWithIdentifiers:@[ @(SectionIdentifierLastVisited) ]];
    [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierLastVisited) ]];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(self.pageInfoPresentationHandler);
  ItemIdentifier itemType = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  switch (itemType) {
    case ItemIdentifierSecurity:
      if (IsRevampPageInfoIosEnabled()) {
        [self.pageInfoPresentationHandler showSecurityPage];
      }
      break;
    case ItemIdentifierAboutThisSite:
      if (IsRevampPageInfoIosEnabled()) {
        [self.pageInfoPresentationHandler
            showAboutThisSitePage:_aboutThisSiteInfo.moreAboutURL];
      }
      break;
    case ItemIdentifierLastVisited:
      CHECK(IsPageInfoLastVisitedIOSEnabled());
      [self.pageInfoPresentationHandler showLastVisitedPage];
      break;
    case ItemIdentifierPermissionsCamera:
    case ItemIdentifierPermissionsMicrophone:
      break;
  }

  // Deselect the row so the UI seems responsive when the action triggered by
  // the selection (e.g. opening a new tab, opening a subpage) takes a bit
  // longer to happen.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);

  if (IsRevampPageInfoIosEnabled()) {
    return ChromeTableViewHeightForHeaderInSection(sectionIdentifier);
  }

  return sectionIdentifier == SectionIdentifierSecurityContent
             ? kPageInfoPaddingFirstSectionHeader
             : UITableViewAutomaticDimension;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierSecurityContent:
    case SectionIdentifierAboutThisSite:
    case SectionIdentifierLastVisited:
      return nil;
    case SectionIdentifierPermissions: {
      if (IsRevampPageInfoIosEnabled()) {
        return nil;
      }

      TableViewTextHeaderFooterView* header =
          DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
              self.tableView);
      header.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER);
      [header setSubtitle:nil];
      return header;
    }
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierSecurityContent: {
      if (IsRevampPageInfoIosEnabled()) {
        // Don't show the security footer in the revamp UI.
        return nil;
      }

      TableViewLinkHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(
              self.tableView);
      footer.urls =
          @[ [[CrURL alloc] initWithGURL:GURL(kPageInfoHelpCenterURL)] ];
      [footer setText:self.pageInfoSecurityDescription.message
            withColor:[UIColor colorNamed:kTextSecondaryColor]];
      footer.delegate = self;
      footer.accessibilityIdentifier =
          kPageInfoSecurityFooterAccessibilityIdentifier;
      return footer;
    }
    case SectionIdentifierPermissions: {
      TableViewAttributedStringHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<
              TableViewAttributedStringHeaderFooterView>(self.tableView);
      footer.attributedString = [self permissionFooterAttributedString];
      return footer;
    }
    default:
      return nil;
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  DCHECK(URL.gurl == GURL(kPageInfoHelpCenterURL));
  [self.pageInfoPresentationHandler showSecurityHelpPage];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.pageInfoCommandsHandler hidePageInfo];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.pageInfoCommandsHandler hidePageInfo];
}

#pragma mark - Private

// Returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierSecurity: {
      TableViewDetailIconCell* cell =
          DequeueTableViewCell<TableViewDetailIconCell>(tableView);
      cell.textLabel.text = l10n_util::GetNSString(
          IsRevampPageInfoIosEnabled() ? IDS_IOS_PAGE_INFO_CONNECTION
                                       : IDS_IOS_PAGE_INFO_SITE_SECURITY);
      cell.detailText = self.pageInfoSecurityDescription.status;
      [cell setIconImage:self.pageInfoSecurityDescription.iconImage
                tintColor:UIColor.whiteColor
          backgroundColor:self.pageInfoSecurityDescription.iconBackgroundColor
             cornerRadius:kColorfulBackgroundSymbolCornerRadius];

      if (IsRevampPageInfoIosEnabled()) {
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      }

      return cell;
    }
    case ItemIdentifierPermissionsCamera: {
      TableViewSwitchCell* cell =
          DequeueTableViewCell<TableViewSwitchCell>(tableView);
      BOOL permissionOn =
          self.permissionsInfo[@(web::PermissionCamera)].unsignedIntValue ==
          web::PermissionStateAllowed;
      NSString* title = l10n_util::GetNSString(IDS_IOS_PERMISSIONS_CAMERA);
      [cell configureCellWithTitle:title
                          subtitle:nil
                     switchEnabled:YES
                                on:permissionOn];
      cell.accessibilityIdentifier =
          kPageInfoCameraSwitchAccessibilityIdentifier;
      cell.switchView.tag = itemIdentifier;
      [cell.switchView addTarget:self
                          action:@selector(permissionSwitchToggled:)
                forControlEvents:UIControlEventValueChanged];

      if (IsRevampPageInfoIosEnabled()) {
        [cell setIconImage:CustomSymbolWithPointSize(kCameraSymbol,
                                                     kPageInfoSymbolPointSize)
                  tintColor:UIColor.whiteColor
            backgroundColor:[UIColor colorNamed:kOrange500Color]
               cornerRadius:kColorfulBackgroundSymbolCornerRadius
                borderWidth:0];
      }

      return cell;
    }
    case ItemIdentifierPermissionsMicrophone: {
      TableViewSwitchCell* cell =
          DequeueTableViewCell<TableViewSwitchCell>(tableView);
      BOOL permissionOn =
          self.permissionsInfo[@(web::PermissionMicrophone)].unsignedIntValue ==
          web::PermissionStateAllowed;
      NSString* title = l10n_util::GetNSString(IDS_IOS_PERMISSIONS_MICROPHONE);
      [cell configureCellWithTitle:title
                          subtitle:nil
                     switchEnabled:YES
                                on:permissionOn];
      cell.accessibilityIdentifier =
          kPageInfoMicrophoneSwitchAccessibilityIdentifier;
      cell.switchView.tag = itemIdentifier;
      [cell.switchView addTarget:self
                          action:@selector(permissionSwitchToggled:)
                forControlEvents:UIControlEventValueChanged];

      if (IsRevampPageInfoIosEnabled()) {
        [cell setIconImage:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                      kPageInfoSymbolPointSize)
                  tintColor:UIColor.whiteColor
            backgroundColor:[UIColor colorNamed:kOrange500Color]
               cornerRadius:kColorfulBackgroundSymbolCornerRadius
                borderWidth:0];
      }

      return cell;
    }
    case ItemIdentifierAboutThisSite: {
      TableViewDetailIconCell* cell =
          DequeueTableViewCell<TableViewDetailIconCell>(tableView);
      cell.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_PAGE_INFO_ABOUT_THIS_PAGE);
      cell.detailText = _aboutThisSiteInfo.summary;
      cell.detailTextNumberOfLines = kAboutThisSiteDetailTextNumberOfLines;
      cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;

      UIImage* icon =
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
          CustomSymbolTemplateWithPointSize(kPageInsightsSymbol,
                                            kPageInfoSymbolPointSize);
#else
          DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                             kPageInfoSymbolPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS),

      [cell setIconImage:icon
                tintColor:UIColor.whiteColor
          backgroundColor:[UIColor colorNamed:kPurple500Color]
             cornerRadius:kColorfulBackgroundSymbolCornerRadius];

      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kExternalLinkSymbol)];
      cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
      return cell;
    }
    case ItemIdentifierLastVisited: {
      TableViewDetailIconCell* cell =
          DequeueTableViewCell<TableViewDetailIconCell>(tableView);
      cell.textLabel.text = l10n_util::GetNSString(IDS_PAGE_INFO_HISTORY);

      cell.detailText = _lastVisitedTimestamp;
      cell.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
      [cell setIconImage:DefaultSymbolTemplateWithPointSize(
                             kClockSymbol, kPageInfoSymbolPointSize)
                tintColor:UIColor.whiteColor
          backgroundColor:[UIColor colorNamed:kBlue500Color]
             cornerRadius:kColorfulBackgroundSymbolCornerRadius];

      if (IsRevampPageInfoIosEnabled()) {
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      }

      return cell;
    }
  }
}

// Returns the attributed string for the permission footer.
- (NSAttributedString*)permissionFooterAttributedString {
  NSString* description = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_INFOBAR_MODAL_DESCRIPTION,
      base::SysNSStringToUTF16(self.pageInfoSecurityDescription.siteURL));
  NSMutableAttributedString* descriptionAttributedString =
      [[NSMutableAttributedString alloc]
          initWithAttributedString:PutBoldPartInString(
                                       description, UIFontTextStyleFootnote)];
  [descriptionAttributedString
      addAttributes:@{
        NSForegroundColorAttributeName :
            [UIColor colorNamed:kTextSecondaryColor]
      }
              range:NSMakeRange(0, descriptionAttributedString.length)];
  return descriptionAttributedString;
}

// Returns an UILabel for the navigationItem titleView for `siteURL`.
- (UILabel*)titleViewLabelForURL:(NSString*)siteURL {
  UILabel* labelURL = [[UILabel alloc] init];
  labelURL.lineBreakMode = NSLineBreakByTruncatingHead;
  labelURL.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  labelURL.text = siteURL;
  labelURL.adjustsFontSizeToFitWidth = YES;
  labelURL.minimumScaleFactor = kTitleLabelMinimumScaleFactor;
  return labelURL;
}

// Updates `snapshot` to reflect the changes to AboutThisSite info.
- (void)updateSnapshotForAboutThisSite:
    (NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>*)snapshot {
  CHECK(IsAboutThisSiteFeatureEnabled());
  if (!_aboutThisSiteInfo || !self.pageInfoSecurityDescription.secure) {
    return;
  }

  NSInteger sectionIndex =
      [snapshot indexOfSectionIdentifier:@(SectionIdentifierPermissions)];
  SectionIdentifier afterSectionWithIdentifier =
      (sectionIndex == NSNotFound) ? SectionIdentifierSecurityContent
                                   : SectionIdentifierPermissions;
  [snapshot insertSectionsWithIdentifiers:@[ @(SectionIdentifierAboutThisSite) ]
               afterSectionWithIdentifier:@(afterSectionWithIdentifier)];

  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierAboutThisSite) ]
             intoSectionWithIdentifier:@(SectionIdentifierAboutThisSite)];
}

// Updates `snapshot` to reflect the changes done to `permissions`.
- (void)updateSnapshot:
            (NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>*)snapshot
         forPermission:(NSNumber*)permission {
  web::PermissionState state = static_cast<web::PermissionState>(
      self.permissionsInfo[permission].unsignedIntegerValue);
  ItemIdentifier itemIdentifier;
  switch (static_cast<web::Permission>(permission.unsignedIntValue)) {
    case web::PermissionCamera:
      itemIdentifier = ItemIdentifierPermissionsCamera;
      break;
    case web::PermissionMicrophone:
      itemIdentifier = ItemIdentifierPermissionsMicrophone;
      break;
  }
  [self updateSnapshot:snapshot forPermissionState:state toItem:itemIdentifier];
}

// Invoked when a permission switch is toggled.
- (void)permissionSwitchToggled:(UISwitch*)sender {
  web::Permission permission;
  switch (sender.tag) {
    case ItemIdentifierPermissionsCamera:
      permission = web::PermissionCamera;
      break;
    case ItemIdentifierPermissionsMicrophone:
      permission = web::PermissionMicrophone;
      break;
  }
  PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
  permissionsDescription.permission = permission;
  permissionsDescription.state =
      sender.isOn ? web::PermissionStateAllowed : web::PermissionStateBlocked;
  [self.permissionsDelegate updateStateForPermission:permissionsDescription];
}

// Updates the `snapshot` (including adds/removes section) to reflect changes to
// `itemIdentifier`'s `state`.
- (void)updateSnapshot:
            (NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>*)snapshot
    forPermissionState:(web::PermissionState)state
                toItem:(ItemIdentifier)itemIdentifier {
  NSInteger sectionIndex =
      [snapshot indexOfSectionIdentifier:@(SectionIdentifierPermissions)];
  if (sectionIndex == NSNotFound) {
    [snapshot
        insertSectionsWithIdentifiers:@[ @(SectionIdentifierPermissions) ]
           afterSectionWithIdentifier:@(SectionIdentifierSecurityContent)];
  }

  BOOL itemVisible = state != web::PermissionStateNotAccessible;
  if (itemVisible) {
    if ([_dataSource indexPathForItemIdentifier:@(itemIdentifier)]) {
      [snapshot reconfigureItemsWithIdentifiers:@[ @(itemIdentifier) ]];
    } else {
      if (itemIdentifier == ItemIdentifierPermissionsCamera &&
          [_dataSource indexPathForItemIdentifier:
                           @(ItemIdentifierPermissionsMicrophone)]) {
        [snapshot
            insertItemsWithIdentifiers:@[ @(itemIdentifier) ]
              beforeItemWithIdentifier:@(ItemIdentifierPermissionsMicrophone)];
      } else {
        [snapshot appendItemsWithIdentifiers:@[ @(itemIdentifier) ]
                   intoSectionWithIdentifier:@(SectionIdentifierPermissions)];
      }
    }
  } else {
    [snapshot deleteItemsWithIdentifiers:@[ @(itemIdentifier) ]];
  }

  if ([snapshot numberOfItemsInSection:@(SectionIdentifierPermissions)] == 0) {
    [snapshot
        deleteSectionsWithIdentifiers:@[ @(SectionIdentifierPermissions) ]];
  }
}

#pragma mark - PageInfoAboutThisSiteConsumer

- (void)setAboutThisSiteSection:(PageInfoAboutThisSiteInfo*)info {
  CHECK(IsAboutThisSiteFeatureEnabled());
  _aboutThisSiteInfo = info;
}

#pragma mark - PermissionsConsumer

- (void)setPermissionsInfo:
    (NSDictionary<NSNumber*, NSNumber*>*)permissionsInfo {
  _permissionsInfo = [permissionsInfo mutableCopy];
}

- (void)permissionStateChanged:(PermissionInfo*)permissionInfo {
  self.permissionsInfo[@(permissionInfo.permission)] = @(permissionInfo.state);

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [self updateSnapshot:snapshot forPermission:@(permissionInfo.permission)];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - PageInfoHistoryConsumer

- (void)setLastVisitedTimestamp:(base::Time)lastVisited {
  CHECK(IsPageInfoLastVisitedIOSEnabled());
  std::string timestamp = base::UTF16ToUTF8(
      page_info::PageInfoHistoryDataSource::FormatLastVisitedTimestamp(
          lastVisited));
  _lastVisitedTimestamp = [NSString stringWithUTF8String:timestamp.c_str()];

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];

  // It was observed that the Last Visited timestamp is usually available before
  // the view is displayed. If that is the case, then we can just store the
  // value of the timestamp and rely on `loadModel` to display the Last Visited
  // row.
  if (!_dataSource || !snapshot) {
    return;
  }

  // Append cell for the Last Visited row.
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierLastVisited) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierLastVisited) ]];

  // Update the UI.
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
