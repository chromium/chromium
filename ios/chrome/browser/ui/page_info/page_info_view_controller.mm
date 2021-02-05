// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSecurityContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSecurityHeader = kItemTypeEnumZero,
  ItemTypeSecurityDescription,
};

// The vertical padding between the navigation bar and the Security header.
float kPaddingSecurityHeader = 28.0f;

}  // namespace

@interface PageInfoViewController () <TableViewLinkHeaderFooterItemDelegate,
                                      TableViewTextLinkCellDelegate>

@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

@end

@implementation PageInfoViewController

#pragma mark - UIViewController

- (instancetype)initWithSiteSecurityDescription:
    (PageInfoSiteSecurityDescription*)siteSecurityDescription {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? ChromeTableViewStyle()
                               : UITableViewStylePlain;
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

  TableViewDetailIconItem* securityHeader =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSecurityHeader];
  securityHeader.text = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);
  securityHeader.detailText = self.pageInfoSecurityDescription.status;
  securityHeader.iconImageName = self.pageInfoSecurityDescription.iconImageName;
  [self.tableViewModel addItem:securityHeader
       toSectionWithIdentifier:SectionIdentifierSecurityContent];

  if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
    TableViewLinkHeaderFooterItem* securityDescription =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:ItemTypeSecurityDescription];
    securityDescription.text = self.pageInfoSecurityDescription.message;
    securityDescription.linkURL = GURL(kPageInfoHelpCenterURL);
    [self.tableViewModel setFooter:securityDescription
          forSectionWithIdentifier:SectionIdentifierSecurityContent];
  } else {
    TableViewTextLinkItem* securityDescription = [[TableViewTextLinkItem alloc]
        initWithType:ItemTypeSecurityDescription];
    securityDescription.text = self.pageInfoSecurityDescription.message;
    securityDescription.linkURL = GURL(kPageInfoHelpCenterURL);
    [self.tableViewModel addItem:securityDescription
         toSectionWithIdentifier:SectionIdentifierSecurityContent];
  }
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kPaddingSecurityHeader;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == ItemTypeSecurityDescription) {
    DCHECK(!base::FeatureList::IsEnabled(kSettingsRefresh));
    TableViewTextLinkCell* tableViewTextLinkCell =
        base::mac::ObjCCastStrict<TableViewTextLinkCell>(cellToReturn);
    tableViewTextLinkCell.delegate = self;
  }

  return cellToReturn;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierSecurityContent: {
      if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
        TableViewLinkHeaderFooterView* linkView =
            base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
        linkView.delegate = self;
      }
    } break;
    default:
      NOTREACHED();
      break;
  }
  return view;
}

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL {
  DCHECK(!base::FeatureList::IsEnabled(kSettingsRefresh));
  DCHECK(URL == GURL(kPageInfoHelpCenterURL));
  [self.handler showSecurityHelpPage];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  DCHECK(base::FeatureList::IsEnabled(kSettingsRefresh));
  DCHECK(URL == GURL(kPageInfoHelpCenterURL));
  [self.handler showSecurityHelpPage];
}

#pragma mark - Private

// Returns the navigationItem titleView for |siteURL|.
- (UILabel*)titleViewLabelForURL:(NSString*)siteURL {
  UILabel* labelURL = [[UILabel alloc] init];
  labelURL.lineBreakMode = NSLineBreakByTruncatingHead;
  labelURL.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  labelURL.text = siteURL;
  labelURL.adjustsFontSizeToFitWidth = YES;
  labelURL.minimumScaleFactor = 0.7;
  return labelURL;
}

@end
