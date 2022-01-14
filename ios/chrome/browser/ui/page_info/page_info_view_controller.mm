// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/net/crurl.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller_permissions_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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

@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

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
                           target:self.handler
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
  securityHeader.iconImageName = self.pageInfoSecurityDescription.iconImageName;
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
    if ([self.permissionsDelegate shouldShowPermissionsSection]) {
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

  [self addSwitchIfExistForPermission:web::Permission::CAMERA
                            withLabel:l10n_util::GetNSString(
                                          IDS_IOS_PERMISSIONS_CAMERA)
                               toItem:ItemTypePermissionsCamera];
  [self addSwitchIfExistForPermission:web::Permission::MICROPHONE
                            withLabel:l10n_util::GetNSString(
                                          IDS_IOS_PERMISSIONS_MICROPHONE)
                               toItem:ItemTypePermissionsMicrophone];

  TableViewLinkHeaderFooterItem* permissionsDescription =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypePermissionsDescription];
  permissionsDescription.text = l10n_util::GetNSStringF(
      IDS_IOS_PAGE_INFO_PERMISSIONS_DESCRIPTION,
      base::SysNSStringToUTF16(self.pageInfoSecurityDescription.siteURL));
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
      [self.tableViewModel sectionIdentifierForSection:section];
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
  [self.handler showSecurityHelpPage];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.handler hidePageInfo];
}

#pragma mark - Private

// Returns the navigationItem titleView for |siteURL|.
- (UILabel*)titleViewLabelForURL:(NSString*)siteURL {
  UILabel* labelURL = [[UILabel alloc] init];
  labelURL.lineBreakMode = NSLineBreakByTruncatingHead;
  labelURL.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  labelURL.text = siteURL;
  labelURL.adjustsFontSizeToFitWidth = YES;
  labelURL.minimumScaleFactor = kTitleLabelMinimumScaleFactor;
  return labelURL;
}

// Invoked when a permission switch is toggled.
- (void)permissionSwitchToggled:(UISwitch*)sender API_AVAILABLE(ios(15.0)) {
  web::Permission permission;
  switch (sender.tag) {
    case ItemTypePermissionsCamera:
      permission = web::Permission::CAMERA;
      break;
    case ItemTypePermissionsMicrophone:
      permission = web::Permission::MICROPHONE;
      break;
    case ItemTypeSecurityHeader:
    case ItemTypeSecurityDescription:
    case ItemTypePermissionsHeader:
    case ItemTypePermissionsDescription: {
      NOTREACHED();
      return;
    }
  }
  [self.permissionsDelegate toggleStateForPermission:permission];
}

// If |permission| is accessible, add a switch for it to the Permissions section
// of the table.
- (void)addSwitchIfExistForPermission:(web::Permission)permission
                            withLabel:(NSString*)label
                               toItem:(ItemType)item API_AVAILABLE(ios(15.0)) {
  if ([self.permissionsDelegate isPermissionAccessible:permission]) {
    TableViewSwitchItem* switchItem =
        [[TableViewSwitchItem alloc] initWithType:item];
    switchItem.text = label;
    switchItem.on =
        [self.permissionsDelegate stateForAccessiblePermission:permission];
    [self.tableViewModel addItem:switchItem
         toSectionWithIdentifier:SectionIdentifierPermissions];
  }
}

@end
