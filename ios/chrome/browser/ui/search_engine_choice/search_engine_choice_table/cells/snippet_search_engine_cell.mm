// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
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
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [_faviconContainerView
        setFaviconBackgroundColor:[UIColor colorNamed:kBackgroundColor]];
    [contentView addSubview:_faviconContainerView];

    // Add name label and snippet label.
    UIView* nameSnippetLabelContainer = [[UIView alloc] init];
    nameSnippetLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:nameSnippetLabelContainer];
    _nameLabel = [[UILabel alloc] init];
    _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _nameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _nameLabel.adjustsFontForContentSizeCategory = YES;
    [nameSnippetLabelContainer addSubview:_nameLabel];
    _snippetLabel = [[UILabel alloc] init];
    _snippetLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _snippetLabel.adjustsFontForContentSizeCategory = YES;
    _snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _snippetLabel.numberOfLines = 1;
    [nameSnippetLabelContainer addSubview:_snippetLabel];

    NSLayoutConstraint* heightConstraint = [_snippetLabel.bottomAnchor
        constraintEqualToAnchor:nameSnippetLabelContainer.bottomAnchor
                       constant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [nameSnippetLabelContainer.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
      [nameSnippetLabelContainer.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:
                                       -kTableViewTwoLabelsCellVerticalSpacing],
      [nameSnippetLabelContainer.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor],
      [nameSnippetLabelContainer.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [_nameLabel.leadingAnchor
          constraintEqualToAnchor:nameSnippetLabelContainer.leadingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [_nameLabel.trailingAnchor
          constraintEqualToAnchor:nameSnippetLabelContainer.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_snippetLabel.leadingAnchor
          constraintEqualToAnchor:nameSnippetLabelContainer.leadingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [_snippetLabel.trailingAnchor
          constraintEqualToAnchor:nameSnippetLabelContainer.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_nameLabel.topAnchor
          constraintEqualToAnchor:nameSnippetLabelContainer.topAnchor
                         constant:0],
      [_nameLabel.bottomAnchor constraintEqualToAnchor:_snippetLabel.topAnchor
                                              constant:0],
      heightConstraint,
    ]];
  }
  return self;
}

#pragma mark - Properties

- (FaviconView*)faviconView {
  return _faviconContainerView.faviconView;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.faviconView configureWithAttributes:nil];
  self.snippetLabel.numberOfLines = 1;
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
  return
      [NSString stringWithFormat:@"%@%@", kSnippetSearchEngineIdentifierPrefix,
                                 self.nameLabel.text];
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end
