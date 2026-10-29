// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_section_header_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Top padding above the first section header label.
constexpr CGFloat kFirstSectionHeaderTopPadding = 8.0;

// Top padding above subsequent section header labels.
constexpr CGFloat kSectionHeaderTopPadding = 16.0;

// Bottom padding between a section header label and its grouped card.
constexpr CGFloat kSectionHeaderBottomPadding = 6.0;

}  // namespace

@implementation AutofillAiSourcesSectionHeaderView {
  UILabel* _titleLabel;
  NSLayoutConstraint* _topConstraint;
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _titleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.numberOfLines = 0;
    _titleLabel.accessibilityTraits = UIAccessibilityTraitHeader;
    [self.contentView addSubview:_titleLabel];

    // Use `UILayoutPriorityDefaultHigh` on padding constraints so temporary
    // zero-height frames assigned by `UITableView` before layout do not log
    // unsatisfiable constraint warnings.
    _topConstraint = [_titleLabel.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kSectionHeaderTopPadding];
    _topConstraint.priority = UILayoutPriorityDefaultHigh;

    NSLayoutConstraint* bottomConstraint = [_titleLabel.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kSectionHeaderBottomPadding];
    bottomConstraint.priority = UILayoutPriorityDefaultHigh;

    NSLayoutConstraint* leadingConstraint = [_titleLabel.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    leadingConstraint.priority = UILayoutPriorityDefaultHigh;

    NSLayoutConstraint* trailingConstraint = [_titleLabel.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing];
    trailingConstraint.priority = UILayoutPriorityDefaultHigh;

    [NSLayoutConstraint activateConstraints:@[
      _topConstraint,
      bottomConstraint,
      leadingConstraint,
      trailingConstraint,
    ]];
  }
  return self;
}

#pragma mark - UITableViewHeaderFooterView

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  self.firstSection = NO;
}

#pragma mark - Public

- (void)setFirstSection:(BOOL)firstSection {
  if (_firstSection == firstSection) {
    return;
  }
  _firstSection = firstSection;
  _topConstraint.constant =
      _firstSection ? kFirstSectionHeaderTopPadding : kSectionHeaderTopPadding;
}

- (void)configureWithTitle:(NSString*)title {
  _titleLabel.text = title;
}

@end
