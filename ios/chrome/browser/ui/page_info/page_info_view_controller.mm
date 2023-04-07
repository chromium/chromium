// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/crurl.h"
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
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#import "ios/chrome/browser/ui/permissions/permissions_constants.h"
#import "ios/chrome/browser/ui/permissions/permissions_delegate.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSecurityContent = kSectionIdentifierEnumZero,
  SectionIdentifierPermissions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSecurityHeader = kItemTypeEnumZero,
  ItemTypeSecurityDescription,
  ItemTypePermissionsHeader,
  ItemTypePermissionsCamera,
  ItemTypePermissionsMicrophone,
  ItemTypePermissionsDescription,
};

// The vertical padding between the navigation bar and the Security header.
float kPaddingSecurityHeader = 28.0f;
// The minimum scale factor of the title label showing the URL.
float kTitleLabelMinimumScaleFactor = 0.7f;

}  // namespace

@interface PageInfoViewController () <TableViewLinkHeaderFooterItemDelegate>

// The page info security description.
@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

// The list of permissions info used to create switches.
@property(nonatomic, copy) NSArray<PermissionInfo*>* permissionsInfo;

@end

@implementation PageInfoViewController

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

  self.navigationItem.titleView =
      [self titleViewLabelForURL:self.pageInfoSecurityDescription.siteURL];
  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);
  self.tableView.accessibilityIdentifier = kPageInfoViewAccessibilityIdentifier;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.pageInfoCommandsHandler
                           action:@selector(hidePageInfo)];
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);
  self.tableView.allowsSelection = NO;

  if (self.pageInfoSecurityDescription.isEmpty) {
    [self addEmptyTableViewWithMessage:self.pageInfoSecurityDescription.message
                                 image:nil];
    self.tableView.alwaysBounceVertical = NO;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    return;
  }

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierSecurityContent];

  // Site Security section.
  TableViewDetailIconItem* securityHeader =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSecurityHeader];
  securityHeader.text = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);
  securityHeader.detailText = self.pageInfoSecurityDescription.status;
  securityHeader.iconImage = self.pageInfoSecurityDescription.iconImage;
  securityHeader.iconTintColor = UIColor.whiteColor;
  securityHeader.iconBackgroundColor =
      self.pageInfoSecurityDescription.iconBackgroundColor;
  securityHeader.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  [self.tableViewModel addItem:securityHeader
       toSectionWithIdentifier:SectionIdentifierSecurityContent];

  TableViewLinkHeaderFooterItem* securityDescription =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeSecurityDescription];
  securityDescription.text = self.pageInfoSecurityDescription.message;
  securityDescription.urls =
      @[ [[CrURL alloc] initWithGURL:GURL(kPageInfoHelpCenterURL)] ];
  [self.tableViewModel setFooter:securityDescription
        forSectionWithIdentifier:SectionIdentifierSecurityContent];

  // Permissions section.
  if (@available(iOS 15.0, *)) {
    if ([self.permissionsInfo count]) {
      [self loadPermissionsModel];
    }
  }
}

// Loads the "Permissions" section in this table view.
- (void)loadPermissionsModel API_AVAILABLE(ios(15.0)) {
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierPermissions];

  TableViewTextHeaderFooterItem* permissionsHeaderItem =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:ItemTypePermissionsHeader];
  permissionsHeaderItem.text =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_PERMISSIONS_HEADER);
  [self.tableViewModel setHeader:permissionsHeaderItem
        forSectionWithIdentifier:SectionIdentifierPermissions];

  for (id permission in self.permissionsInfo) {
    [self updateSwitchForPermission:permission tableViewLoaded:NO];
  }

  TableViewAttributedStringHeaderFooterItem* permissionsDescription =
      [[TableViewAttributedStringHeaderFooterItem alloc]
          initWithType:ItemTypePermissionsDescription];
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
  permissionsDescription.attributedString = descriptionAttributedString;
  [self.tableViewModel setFooter:permissionsDescription
        forSectionWithIdentifier:SectionIdentifierPermissions];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return section == SectionIdentifierSecurityContent
             ? kPaddingSecurityHeader
             : UITableViewAutomaticDimension;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierSecurityContent: {
      TableViewLinkHeaderFooterView* linkView =
          base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
  }
  return view;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType =
      (ItemType)[self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePermissionsCamera:
    case ItemTypePermissionsMicrophone: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      switchCell.switchView.tag = itemType;
      [switchCell.switchView addTarget:self
                                action:@selector(permissionSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSecurityHeader:
    case ItemTypeSecurityDescription:
    case ItemTypePermissionsHeader:
    case ItemTypePermissionsDescription: {
      // Not handled.
      break;
    }
  }
  return cell;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  DCHECK(URL.gurl == GURL(kPageInfoHelpCenterURL));
  [self.pageInfoCommandsHandler showSecurityHelpPage];
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

// Returns the navigationItem titleView for `siteURL`.
- (UILabel*)titleViewLabelForURL:(NSString*)siteURL {
  UILabel* labelURL = [[UILabel alloc] init];
  labelURL.lineBreakMode = NSLineBreakByTruncatingHead;
  labelURL.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  labelURL.text = siteURL;
  labelURL.adjustsFontSizeToFitWidth = YES;
  labelURL.minimumScaleFactor = kTitleLabelMinimumScaleFactor;
  return labelURL;
}

// Updates the switch of the given permission.
- (void)updateSwitchForPermission:(PermissionInfo*)permissionInfo
                  tableViewLoaded:(BOOL)tableViewLoaded {
  switch (permissionInfo.permission) {
    case web::PermissionCamera:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_CAMERA)
                                    toItem:ItemTypePermissionsCamera
                           tableViewLoaded:tableViewLoaded];
      break;
    case web::PermissionMicrophone:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_MICROPHONE)
                                    toItem:ItemTypePermissionsMicrophone
                           tableViewLoaded:tableViewLoaded];
      break;
  }
}

// Invoked when a permission switch is toggled.
- (void)permissionSwitchToggled:(UISwitch*)sender {
  if (@available(iOS 15.0, *)) {
    web::Permission permission;
    switch (sender.tag) {
      case ItemTypePermissionsCamera:
        permission = web::PermissionCamera;
        break;
      case ItemTypePermissionsMicrophone:
        permission = web::PermissionMicrophone;
        break;
      case ItemTypePermissionsDescription:
        NOTREACHED();
        return;
    }
    PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
    permissionsDescription.permission = permission;
    permissionsDescription.state =
        sender.isOn ? web::PermissionStateAllowed : web::PermissionStateBlocked;
    [self.permissionsDelegate updateStateForPermission:permissionsDescription];
  }
}

// Adds or removes a switch depending on the value of the PermissionState.
- (void)updateSwitchForPermissionState:(web::PermissionState)state
                             withLabel:(NSString*)label
                                toItem:(ItemType)itemType
                       tableViewLoaded:(BOOL)tableViewLoaded {
  if ([self.tableViewModel hasItemForItemType:itemType
                            sectionIdentifier:SectionIdentifierPermissions]) {
    NSIndexPath* index = [self.tableViewModel indexPathForItemType:itemType];

    // Remove the switch item if the permission is not accessible.
    if (state == web::PermissionStateNotAccessible) {
      [self removeFromModelItemAtIndexPaths:@[ index ]];
      [self.tableView deleteRowsAtIndexPaths:@[ index ]
                            withRowAnimation:UITableViewRowAnimationAutomatic];
    } else {
      TableViewSwitchItem* currentItem =
          base::mac::ObjCCastStrict<TableViewSwitchItem>(
              [self.tableViewModel itemAtIndexPath:index]);
      TableViewSwitchCell* currentCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(
              [self.tableView cellForRowAtIndexPath:index]);
      currentItem.on = state == web::PermissionStateAllowed;
      // Reload the switch cell if its value is outdated.
      if (currentItem.isOn != currentCell.switchView.isOn) {
        [self.tableView
            reloadRowsAtIndexPaths:@[ index ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
    }
    return;
  }

  // Don't add a switch item if the permission is not accessible.
  if (state == web::PermissionStateNotAccessible) {
    return;
  }

  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:itemType];
  switchItem.text = label;
  switchItem.on = state == web::PermissionStateAllowed;
  switchItem.accessibilityIdentifier =
      itemType == ItemTypePermissionsCamera
          ? kPageInfoCameraSwitchAccessibilityIdentifier
          : kPageInfoMicrophoneSwitchAccessibilityIdentifier;

  // If ItemTypePermissionsMicrophone is already added, insert the
  // ItemTypePermissionsCamera before the ItemTypePermissionsMicrophone.
  if (itemType == ItemTypePermissionsCamera &&
      [self.tableViewModel hasItemForItemType:ItemTypePermissionsMicrophone
                            sectionIdentifier:SectionIdentifierPermissions]) {
    NSIndexPath* index = [self.tableViewModel
        indexPathForItemType:ItemTypePermissionsMicrophone];
    [self.tableViewModel insertItem:switchItem
            inSectionWithIdentifier:SectionIdentifierPermissions
                            atIndex:index.row];
  } else {
    [self.tableViewModel addItem:switchItem
         toSectionWithIdentifier:SectionIdentifierPermissions];
  }

  if (tableViewLoaded) {
    NSIndexPath* index = [self.tableViewModel indexPathForItemType:itemType];
    [self.tableView insertRowsAtIndexPaths:@[ index ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

#pragma mark - PermissionsConsumer

- (void)setPermissionsInfo:(NSArray<PermissionInfo*>*)permissionsInfo {
  _permissionsInfo = permissionsInfo;
}

- (void)permissionStateChanged:(PermissionInfo*)permissionInfo {
  if (@available(iOS 15.0, *)) {
    // Add the Permissions section if it doesn't exist.
    if (![self.tableViewModel
            hasSectionForSectionIdentifier:SectionIdentifierPermissions]) {
      [self loadPermissionsModel];
      NSUInteger index = [self.tableViewModel
          sectionForSectionIdentifier:SectionIdentifierPermissions];
      [self.tableView insertSections:[NSIndexSet indexSetWithIndex:index]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    }

    [self updateSwitchForPermission:permissionInfo tableViewLoaded:YES];
  }
}

@end
