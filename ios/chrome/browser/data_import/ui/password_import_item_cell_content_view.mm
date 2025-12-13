// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/password_import_item_cell_content_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {
/// Spacing between the URL and username label.
const CGFloat kURLAndUsernameSpace = 8.0;
/// Spacing between the two lines in password details.
const CGFloat kUsernameAndDetailSpace = 5.0;
}  // namespace

@implementation PasswordImportItemCellContentView {
  /// Container for the favicon.
  FaviconContainerView* _faviconContainerView;
  /// Main label, displaying the URL.
  UILabel* _URLLabel;
  /// Label showing the user name.
  UILabel* _usernameLabel;
  /// Label with more details.
  UILabel* _detailLabel;
}

- (instancetype)initWithConfiguration:
    (PasswordImportItemCellContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    /// View elements.
    _faviconContainerView = [[FaviconContainerView alloc] init];
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _URLLabel = [[UILabel alloc] init];
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.numberOfLines = 2;
    _URLLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _usernameLabel = [[UILabel alloc] init];
    _usernameLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _usernameLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _usernameLabel.adjustsFontForContentSizeCategory = YES;
    _usernameLabel.numberOfLines = 2;
    _usernameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailLabel = [[UILabel alloc] init];
    _detailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailLabel.adjustsFontForContentSizeCategory = YES;
    _detailLabel.numberOfLines = 2;
    _detailLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_faviconContainerView];
    [self addSubview:_URLLabel];
    [self addSubview:_usernameLabel];
    [self addSubview:_detailLabel];

    /// Positioning.
    UILayoutGuide* labelsLayoutGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:labelsLayoutGuide];
    /// Minimize height.
    NSLayoutConstraint* heightConstraint = [self.heightAnchor
        constraintEqualToConstant:kChromeTableViewCellHeight];
    heightConstraint.priority = UILayoutPriorityDefaultLow;
    [NSLayoutConstraint activateConstraints:@[
      /// X-axis anchors.
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [labelsLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [self.trailingAnchor
          constraintEqualToAnchor:labelsLayoutGuide.trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [labelsLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_URLLabel.leadingAnchor],
      [labelsLayoutGuide.trailingAnchor
          constraintGreaterThanOrEqualToAnchor:_URLLabel.trailingAnchor],
      [labelsLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_usernameLabel.leadingAnchor],
      [labelsLayoutGuide.trailingAnchor
          constraintGreaterThanOrEqualToAnchor:_usernameLabel.trailingAnchor],
      [labelsLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailLabel.leadingAnchor],
      [labelsLayoutGuide.trailingAnchor
          constraintGreaterThanOrEqualToAnchor:_detailLabel.trailingAnchor],
      /// Y-axis anchors.
      [_faviconContainerView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:
                                          kTableViewOneLabelCellVerticalSpacing],
      [_faviconContainerView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                   constant:
                                       kTableViewOneLabelCellVerticalSpacing],
      [labelsLayoutGuide.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:
                                          kTableViewOneLabelCellVerticalSpacing],
      [labelsLayoutGuide.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                   constant:
                                       kTableViewOneLabelCellVerticalSpacing],
      [self.centerYAnchor
          constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
      [self.centerYAnchor
          constraintEqualToAnchor:labelsLayoutGuide.centerYAnchor],
      [_URLLabel.topAnchor constraintEqualToAnchor:labelsLayoutGuide.topAnchor],
      [_usernameLabel.topAnchor constraintEqualToAnchor:_URLLabel.bottomAnchor
                                               constant:kURLAndUsernameSpace],
      [_detailLabel.topAnchor
          constraintEqualToAnchor:_usernameLabel.bottomAnchor
                         constant:kUsernameAndDetailSpace],
      [labelsLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailLabel.bottomAnchor],
      heightConstraint
    ]];
    self.configuration = configuration;
  }
  return self;
}

- (void)setConfiguration:
    (PasswordImportItemCellContentConfiguration*)configuration {
  _configuration = [configuration copy];
  _URLLabel.text = configuration.URL;
  _usernameLabel.text = configuration.username;
  _detailLabel.text = configuration.message;
  if (configuration.isMessageHighlighted) {
    _detailLabel.textColor = [UIColor colorNamed:kRed600Color];
  }
  [_faviconContainerView.faviconView
      configureWithAttributes:configuration.faviconAttributes];
}

@end
