// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_setup_view_controller.h"

#import <MaterialComponents/MaterialShadowElevations.h>
#import <MaterialComponents/MaterialShadowLayer.h>

#import "remoting/ios/app/host_setup_header_view.h"
#import "remoting/ios/app/host_setup_view_cell.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kInstallationLink = @"remotedesktop.google.com/access";

static NSString* const kHostSetupViewCellIdentifierItem =
    @"HostSetupViewCellIdentifier";
static NSString* const kHeaderViewIdentifierItem =
    @"HostSetupHeaderViewIdentifier";

static const CGFloat kEstimatedRowHeight = 88.f;

@interface HostSetupViewController () {
  NSArray<NSString*>* _setupSteps;
}
@end

@implementation HostSetupViewController

@synthesize scrollViewDelegate = _scrollViewDelegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelection = NO;
  self.tableView.backgroundColor = UIColor.clearColor;
  self.tableView.estimatedRowHeight = kEstimatedRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;

  // Implement the header as a cell instead of a section header so that it
  // doesn't float on the top.
  [self.tableView registerClass:[HostSetupHeaderView class]
         forCellReuseIdentifier:kHeaderViewIdentifierItem];

  [self.tableView registerClass:[HostSetupViewCell class]
         forCellReuseIdentifier:kHostSetupViewCellIdentifierItem];

  _setupSteps = @[
    base::SysUTF8ToNSString(l10n_util::GetStringFUTF8(
        IDS_HOST_SETUP_STEP_1, base::SysNSStringToUTF16(kInstallationLink))),
    base::SysUTF8ToNSString(l10n_util::GetStringUTF8(IDS_HOST_SETUP_STEP_2)),
    base::SysUTF8ToNSString(l10n_util::GetStringUTF8(IDS_HOST_SETUP_STEP_3))
  ];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  // Number of steps + header.
  return _setupSteps.count + 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.item == 0) {
    // Header.
    return
        [tableView dequeueReusableCellWithIdentifier:kHeaderViewIdentifierItem
                                        forIndexPath:indexPath];
  }
  HostSetupViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:kHostSetupViewCellIdentifierItem
                           forIndexPath:indexPath];
  NSInteger stepIndex = indexPath.item - 1;
  NSString* contentText = _setupSteps[stepIndex];
  [cell setContentText:contentText number:stepIndex + 1];
  return cell;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [_scrollViewDelegate scrollViewDidScroll:scrollView];
}

@end
