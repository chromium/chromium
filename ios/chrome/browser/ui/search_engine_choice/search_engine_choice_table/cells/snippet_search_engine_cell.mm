// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"

@implementation SnippetSearchEngineCell {
  // Container View for the faviconView.
  FaviconContainerView* _faviconContainerView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    UIView* contentView = self.contentView;

    _faviconContainerView = [[FaviconContainerView alloc] init];
    [_faviconContainerView
        setFaviconBackgroundColor:[UIColor colorNamed:kBackgroundColor]];
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_faviconContainerView];
    _nameLabel = [[UILabel alloc] init];
    _nameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _nameLabel.adjustsFontForContentSizeCategory = YES;
    _snippetLabel = [[UILabel alloc] init];
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _snippetLabel.adjustsFontForContentSizeCategory = YES;
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.hidden = YES;

    // Horizontal view holds vertical stack view and metadata views.
    UIView* horizontalView = [[UIView alloc] init];
    horizontalView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:horizontalView];

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _nameLabel, _snippetLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [horizontalView addSubview:verticalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [horizontalView.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [horizontalView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [verticalStack.topAnchor
          constraintEqualToAnchor:horizontalView.topAnchor],
      [verticalStack.bottomAnchor
          constraintEqualToAnchor:horizontalView.bottomAnchor],
      [verticalStack.leadingAnchor
          constraintEqualToAnchor:horizontalView.leadingAnchor],
      [horizontalView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
      [horizontalView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:
                                       -kTableViewTwoLabelsCellVerticalSpacing],
      heightConstraint
    ]];
  }
  return self;
}

- (void)configureUILayout {
  if ([self.snippetLabel.text length]) {
    self.snippetLabel.hidden = NO;
  } else {
    self.snippetLabel.hidden = YES;
  }
}

#pragma mark - Properties

- (FaviconView*)faviconView {
  return _faviconContainerView.faviconView;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.faviconView configureWithAttributes:nil];
  self.snippetLabel.hidden = YES;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  NSString* accessibilityLabel = self.nameLabel.text;
  if (self.snippetLabel.text.length > 0) {
    accessibilityLabel = [NSString
        stringWithFormat:@"%@. %@", accessibilityLabel, self.snippetLabel.text];
  }
  return accessibilityLabel;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  CHECK(self.nameLabel.text);
  return @[ self.nameLabel.text ];
}

- (NSString*)accessibilityIdentifier {
  return self.nameLabel.text;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end
