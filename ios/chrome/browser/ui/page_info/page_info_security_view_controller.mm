// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_security_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"

@implementation PageInfoSecurityViewController {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  PageInfoSiteSecurityDescription* _pageInfoSecurityDescription;
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

  self.tableView.accessibilityIdentifier =
      kPageInfoSecurityViewAccessibilityIdentifier;

  // TODO(crbug.com/1512580): Add contents to the security subpage.
}

@end
