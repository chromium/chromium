// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/ui_bundled/page_info_view_controller.h"

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
#import "ios/chrome/browser/page_info/ui_bundled/features.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_about_this_site_info.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_constants.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_history_mutator.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_delegate.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
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
  SectionIdentifierLastVisited,
};

typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierSecurity,
  ItemIdentifierPermissionsCamera,
  ItemIdentifierPermissionsMicrophone,
  ItemIdentifierAboutThisSite,
  ItemIdentifierLastVisited,
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
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.prompt = self.pageInfoSecurityDescription.siteURL;

  self.tableView.accessibilityIdentifier = kPageInfoViewAccessibilityIdentifier;
  self.navigationController.navigationBar.accessibilityIdentifier =
      kPageInfoViewNavigationBarAccessibilityIdentifier;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.pageInfoCommandsHandler
                           action:@selector(hidePageInfo)];
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kPageInfoTableViewSeparatorInsetWithIcon, 0, 0);

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

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // The Last Visited timestamp needs to be updated when the view is first
  // loaded and subsequencenly since there could have been deletions performed
  // on the Last Visited or History UI.
  [self.pageInfoHistoryMutator lastVisitedTimestampNeedsUpdate];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  UITableView* tableView = self.tableView;

  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:tableView
           cellProvider:^UITableViewCell*(UITableView* innerTableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:innerTableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  [TableViewCellContentConfiguration registerCellForTableView:tableView];
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(tableView);
  RegisterTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
      tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierSecurityContent) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierSecurity) ]
             intoSectionWithIdentifier:@(SectionIdentifierSecurityContent)];

  // Permissions section.
  for (NSNumber* permission in self.permissionsInfo.allKeys) {
    [self updateSnapshot:snapshot forPermission:permission];
  }

  if (IsAboutThisSiteFeatureEnabled()) {
    [self updateSnapshotForAboutThisSite:snapshot];
  }

  // Append cell for the Last Visited row only if the flag is enabled and the
  // Last Visited timestamp is available.
  if (_lastVisitedTimestamp) {
    [snapshot
        appendSectionsWithIdentifiers:@[ @(SectionIdentifierLastVisited) ]];
    [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierLastVisited) ]
               intoSectionWithIdentifier:@(SectionIdentifierLastVisited)];
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
      [self.pageInfoPresentationHandler showSecurityPage];
      break;
    case ItemIdentifierAboutThisSite:
      [self.pageInfoPresentationHandler
          showAboutThisSitePage:_aboutThisSiteInfo.moreAboutURL];
      break;
    case ItemIdentifierLastVisited:
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

  return ChromeTableViewHeightForHeaderInSection(sectionIdentifier);
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierSecurityContent:
    case SectionIdentifierAboutThisSite:
    case SectionIdentifierLastVisited:
      return nil;
    case SectionIdentifierPermissions: {
      TableViewAttributedStringHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<
              TableViewAttributedStringHeaderFooterView>(self.tableView);
      footer.attributedString = [self permissionFooterAttributedString];
      return footer;
    }
  }

  NOTREACHED();
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (URL.gurl == GURL(kPageInfoHelpCenterURL)) {
    [self.pageInfoPresentationHandler showSecurityHelpPage];
  }
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
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self.pageInfoCommandsHandler hidePageInfo];
}

#pragma mark - Private

// Returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierSecurity: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title =
          l10n_util::GetNSString(IDS_IOS_PAGE_INFO_CONNECTION);
      configuration.trailingText = self.pageInfoSecurityDescription.status;
      configuration.subtitleNumberOfLines =
          kAboutThisSiteDetailTextNumberOfLines;

      ColorfulSymbolContentConfiguration* iconConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      iconConfiguration.symbolImage =
          self.pageInfoSecurityDescription.iconImage;
      iconConfiguration.symbolBackgroundColor =
          self.pageInfoSecurityDescription.iconBackgroundColor;
      iconConfiguration.symbolTintColor = UIColor.whiteColor;

      configuration.leadingConfiguration = iconConfiguration;

      UITableViewCell* cell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      cell.contentConfiguration = configuration;
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

      return cell;
    }
    case ItemIdentifierPermissionsCamera: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title = l10n_util::GetNSString(IDS_IOS_PERMISSIONS_CAMERA);

      ColorfulSymbolContentConfiguration* symbolConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      symbolConfiguration.symbolImage =
          CustomSymbolWithPointSize(kCameraSymbol, kPageInfoSymbolPointSize);
      symbolConfiguration.symbolTintColor = UIColor.whiteColor;
      symbolConfiguration.symbolBackgroundColor =
          [UIColor colorNamed:kOrange500Color];

      configuration.leadingConfiguration = symbolConfiguration;

      BOOL permissionOn =
          self.permissionsInfo[@(web::PermissionCamera)].unsignedIntValue ==
          web::PermissionStateAllowed;

      SwitchContentConfiguration* switchConfiguration =
          [[SwitchContentConfiguration alloc] init];
      switchConfiguration.target = self;
      switchConfiguration.selector = @selector(permissionSwitchToggled:);
      switchConfiguration.tag = itemIdentifier;
      switchConfiguration.on = permissionOn;

      configuration.trailingConfiguration = switchConfiguration;

      UITableViewCell* cell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      cell.contentConfiguration = configuration;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;

      cell.accessibilityIdentifier =
          kPageInfoCameraSwitchAccessibilityIdentifier;

      return cell;
    }
    case ItemIdentifierPermissionsMicrophone: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title =
          l10n_util::GetNSString(IDS_IOS_PERMISSIONS_MICROPHONE);

      ColorfulSymbolContentConfiguration* symbolConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      symbolConfiguration.symbolImage = DefaultSymbolWithPointSize(
          kMicrophoneSymbol, kPageInfoSymbolPointSize);
      symbolConfiguration.symbolTintColor = UIColor.whiteColor;
      symbolConfiguration.symbolBackgroundColor =
          [UIColor colorNamed:kOrange500Color];

      configuration.leadingConfiguration = symbolConfiguration;

      BOOL permissionOn =
          self.permissionsInfo[@(web::PermissionMicrophone)].unsignedIntValue ==
          web::PermissionStateAllowed;

      SwitchContentConfiguration* switchConfiguration =
          [[SwitchContentConfiguration alloc] init];
      switchConfiguration.target = self;
      switchConfiguration.selector = @selector(permissionSwitchToggled:);
      switchConfiguration.tag = itemIdentifier;
      switchConfiguration.on = permissionOn;

      configuration.trailingConfiguration = switchConfiguration;

      UITableViewCell* cell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      cell.contentConfiguration = configuration;
      cell.selectionStyle = UITableViewCellSelectionStyleNone;

      cell.accessibilityIdentifier =
          kPageInfoMicrophoneSwitchAccessibilityIdentifier;

      return cell;
    }
    case ItemIdentifierAboutThisSite: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title =
          l10n_util::GetNSString(IDS_IOS_PAGE_INFO_ABOUT_THIS_PAGE);
      configuration.subtitle = _aboutThisSiteInfo.summary;
      configuration.subtitleNumberOfLines =
          kAboutThisSiteDetailTextNumberOfLines;

      ColorfulSymbolContentConfiguration* iconConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      UIImage* icon =
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
          CustomSymbolTemplateWithPointSize(kPageInsightsSymbol,
                                            kPageInfoSymbolPointSize);
#else
          DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                             kPageInfoSymbolPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS),
      iconConfiguration.symbolImage = icon;
      iconConfiguration.symbolBackgroundColor =
          [UIColor colorNamed:kPurple500Color];
      iconConfiguration.symbolTintColor = UIColor.whiteColor;

      configuration.leadingConfiguration = iconConfiguration;

      UITableViewCell* cell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      cell.contentConfiguration = configuration;

      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kExternalLinkSymbol)];
      cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
      return cell;
    }
    case ItemIdentifierLastVisited: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title = l10n_util::GetNSString(IDS_PAGE_INFO_HISTORY);
      configuration.subtitle = _lastVisitedTimestamp;
      configuration.subtitleNumberOfLines =
          kAboutThisSiteDetailTextNumberOfLines;

      ColorfulSymbolContentConfiguration* iconConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      iconConfiguration.symbolImage = DefaultSymbolTemplateWithPointSize(
          kClockSymbol, kPageInfoSymbolPointSize);
      iconConfiguration.symbolBackgroundColor =
          [UIColor colorNamed:kBlue500Color];
      iconConfiguration.symbolTintColor = UIColor.whiteColor;

      configuration.leadingConfiguration = iconConfiguration;

      UITableViewCell* cell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      cell.contentConfiguration = configuration;
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

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

- (void)setLastVisitedTimestamp:(std::optional<base::Time>)lastVisited {
  if (lastVisited.has_value()) {
    _lastVisitedTimestamp = base::SysUTF16ToNSString(
        page_info::PageInfoHistoryDataSource::FormatLastVisitedTimestamp(
            lastVisited.value()));
  } else {
    _lastVisitedTimestamp = nil;
  }

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];

  // The Last Visited timestamp can be available before the view is loaded. If
  // that occurs, just store the value of the timestamp and rely on `loadModel`
  // to add the Last Visited section and row to the snapshot.
  if (!_dataSource || !snapshot) {
    return;
  }

  // Update or remove the Last Visited section based on the timestamp.
  if (_lastVisitedTimestamp) {
    if ([snapshot indexOfSectionIdentifier:@(SectionIdentifierLastVisited)] ==
        NSNotFound) {
      // Add the Last Visited section.
      [snapshot
          appendSectionsWithIdentifiers:@[ @(SectionIdentifierLastVisited) ]];
      [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierLastVisited) ]
                 intoSectionWithIdentifier:@(SectionIdentifierLastVisited)];
    }
    // If the section already exists, no need to update the snapshot.
    // The timestamp change will be handled when the cell is configured.
  } else {
    // Remove the Last Visited section.
    [snapshot
        deleteSectionsWithIdentifiers:@[ @(SectionIdentifierLastVisited) ]];
  }

  // Update the UI.
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
