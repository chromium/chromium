// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_activity_indicator_header_footer_item.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation TableViewActivityIndicatorHeaderFooterItem
@synthesize subtitleText = _subtitleText;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewActivityIndicatorHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];
  TableViewActivityIndicatorHeaderFooterView* header =
      base::apple::ObjCCastStrict<TableViewActivityIndicatorHeaderFooterView>(
          headerFooter);
  header.titleLabel.text = self.text;
  header.subtitleLabel.text = self.subtitleText;
  // Use colors from styler if available.
  if (styler.tableViewBackgroundColor) {
    header.contentView.backgroundColor = styler.tableViewBackgroundColor;
  }
}

@end

#pragma mark - TableViewActivityIndicatorHeaderFooterView

@implementation TableViewActivityIndicatorHeaderFooterView
@synthesize subtitleLabel = _subtitleLabel;
@synthesize titleLabel = titleLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    self.accessibilityIdentifier =
        kTableViewActivityIndicatorHeaderFooterViewId;

    // Labels, set font sizes using dynamic type.
    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [self.titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    self.subtitleLabel = [[UILabel alloc] init];
    self.subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [self.subtitleLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ self.titleLabel, self.subtitleLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;

    // Activity Indicator.
    MDCActivityIndicator* activityIndicator =
        [[MDCActivityIndicator alloc] init];
    activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
    [activityIndicator startAnimating];
    [activityIndicator
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Horizontal StackView.
    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, activityIndicator ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.alignment = UIStackViewAlignmentCenter;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:horizontalStack];

    // Lower the padding constraints priority. UITableView might try to set
    // the header view height/width to 0 breaking the constraints. See
    // https://crbug.com/854117 for more information.
    NSLayoutConstraint* topAnchorConstraint = [horizontalStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:kTableViewVerticalSpacing];
    topAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* bottomAnchorConstraint = [horizontalStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                 constant:-kTableViewVerticalSpacing];
    bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* leadingAnchorConstraint = [horizontalStack.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:HorizontalPadding()];
    leadingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* trailingAnchorConstraint =
        [horizontalStack.trailingAnchor
            constraintEqualToAnchor:self.contentView.trailingAnchor
                           constant:-HorizontalPadding()];
    trailingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      topAnchorConstraint, bottomAnchorConstraint, leadingAnchorConstraint,
      trailingAnchorConstraint,
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor]
    ]];
  }
  return self;
}

@end
