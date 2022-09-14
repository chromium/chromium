// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


@implementation TableViewTextHeaderFooterItem
@synthesize subtitleText = _subtitleText;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextHeaderFooterView class];
    self.accessibilityTraits |= UIAccessibilityTraitHeader;
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];
  TableViewTextHeaderFooterView* header =
      base::mac::ObjCCastStrict<TableViewTextHeaderFooterView>(headerFooter);
  header.textLabel.text = self.text;
  header.subtitleLabel.text = self.subtitleText;
  header.accessibilityLabel = self.text;
  header.isAccessibilityElement = YES;
}

@end

#pragma mark - TableViewTextHeaderFooter

@implementation TableViewTextHeaderFooterView
@synthesize subtitleLabel = _subtitleLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Labels, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _subtitleLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

    // Container View.
    UIView* containerView = [[UIView alloc] init];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Add subviews to View Hierarchy.
    [containerView addSubview:verticalStack];
    [self.contentView addSubview:containerView];

    // Lower the padding constraints priority. UITableView might try to set
    // the header view height/width to 0 breaking the constraints. See
    // https://crbug.com/854117 for more information.
    NSLayoutConstraint* heightConstraint =
        [self.contentView.heightAnchor constraintGreaterThanOrEqualToConstant:
                                           kTableViewHeaderFooterViewHeight];
    // Set this constraint to UILayoutPriorityDefaultHigh + 1 in order to guard
    // against some elements that might UILayoutPriorityDefaultHigh to expand
    // beyond the margins.
    heightConstraint.priority = UILayoutPriorityDefaultHigh + 1;
    NSLayoutConstraint* topAnchorConstraint = [containerView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kTableViewVerticalSpacing];
    topAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* bottomAnchorConstraint = [containerView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kTableViewVerticalSpacing];
    bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* leadingAnchorConstraint = [containerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:HorizontalPadding()];
    leadingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* trailingAnchorConstraint = [containerView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-HorizontalPadding()];
    trailingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Container Constraints.
      heightConstraint,
      topAnchorConstraint,
      bottomAnchorConstraint,
      leadingAnchorConstraint,
      trailingAnchorConstraint,
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Vertical StackView Constraints.
      [verticalStack.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [verticalStack.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [verticalStack.bottomAnchor
          constraintEqualToAnchor:containerView.bottomAnchor],
      [verticalStack.trailingAnchor
          constraintEqualToAnchor:containerView.trailingAnchor],
    ]];
  }
  return self;
}

@end
