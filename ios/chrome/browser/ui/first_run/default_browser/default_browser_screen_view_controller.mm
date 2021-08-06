// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"

#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view.h"
#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view_cell.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Reuse ID for registering cell class in table views.
constexpr NSString* kReuseID = @"InstructionTableCell";

NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";

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
  self.titleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE);

  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_PRIMARY_ACTION);

  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);

  self.defaultBrowserSteps = @[
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)
  ];

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

  NSString* text = [self.defaultBrowserSteps objectAtIndex:indexPath.row];
  NSAttributedString* attributedString = [self putBoldPartInString:text];

  [cell configureCellText:attributedString withStepNumber:indexPath.row + 1];

  return cell;
}

#pragma mark - Private

// Parses a string with an embedded bold part inside, delineated by
// "BEGIN_BOLD" and "END_BOLD". Returns an attributed string with bold part.
- (NSAttributedString*)putBoldPartInString:(NSString*)string {
  StringWithTag parsedString =
      ParseStringWithTag(string, kBeginBoldTag, kEndBoldTag);

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];

  UIFontDescriptor* defaultDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];

  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];

  [attributedString addAttribute:NSFontAttributeName
                           value:[UIFont fontWithDescriptor:defaultDescriptor
                                                       size:0.0]
                           range:NSMakeRange(0, parsedString.string.length)];

  [attributedString addAttribute:NSFontAttributeName
                           value:[UIFont fontWithDescriptor:boldDescriptor
                                                       size:0.0]
                           range:parsedString.range];

  return attributedString;
}

@end
