// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view.h"
#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Reuse ID for registering cell class in table views.
constexpr NSString* kReuseID = @"InstructionTableCell";

}  // namespace

@interface DefaultBrowserScreenViewController () <UITableViewDataSource,
                                                  UITableViewDelegate>

// Instruction list to set the default browser.
@property(nonatomic, strong) NSArray* defaultBrowserSteps;

@end

@implementation DefaultBrowserScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerImage = [UIImage imageNamed:@"default_browser_screen_banner"];
  self.titleText = @"Use Chrome by default";
  self.subtitleText = @"You can now use Chrome anytime you tap links in "
                      @"messages, documents, and other apps.";

  self.primaryActionString = @"Make Default in Settings...";
  self.secondaryActionString = @"No, Thanks";

  self.defaultBrowserSteps =
      @[ @"Open Settings", @"Tap Default Browser App", @"Select Chrome" ];

  UITableView* tableView =
      [[InstructionTableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStylePlain];
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.dataSource = self;
  tableView.delegate = self;

  [tableView registerClass:[InstructionTableViewCell class]
      forCellReuseIdentifier:kReuseID];

  [self.specificContentView addSubview:tableView];

  [NSLayoutConstraint activateConstraints:@[
    [tableView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [tableView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [tableView.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [tableView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.defaultBrowserSteps.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  InstructionTableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kReuseID];

  [cell configureCellText:[self.defaultBrowserSteps objectAtIndex:indexPath.row]
           withStepNumber:indexPath.row + 1];

  return cell;
}

@end
