// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_table_view_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation TabListFromAndroidTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [TabListFromAndroidTableViewCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TabListFromAndroidTableViewCell* cell =
      base::apple::ObjCCastStrict<TabListFromAndroidTableViewCell>(tableCell);
  cell.titleLabel.text = [self titleLabelText];
  cell.URLLabel.text = [self URLLabelText];
  cell.cellUniqueIdentifier = self.uniqueIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
}

- (NSString*)uniqueIdentifier {
  return self.URL ? base::SysUTF8ToNSString(self.URL.gurl.host()) : @"";
}

#pragma mark - Private

// Returns the text to use when configuring a TabListFromAndroidTableViewCell's
// title label.
- (NSString*)titleLabelText {
  if (self.title.length) {
    return self.title;
  }
  if (!self.URL) {
    return @"";
  }
  NSString* hostname = [self displayedURL];
  if (hostname.length) {
    return hostname;
  }
  // Backup in case host returns nothing (e.g. about:blank).
  return base::SysUTF8ToNSString(self.URL.gurl.spec());
}

// Returns the text to use when configuring a TabListFromAndroidTableViewCell's
// URL label.
- (NSString*)URLLabelText {
  return self.URL ? [self displayedURL] : @"";
}

// Returns a formatted URL text.
- (NSString*)displayedURL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              self.URL.gurl));
}

@end

@implementation TabListFromAndroidTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithStyle:style reuseIdentifier:reuseIdentifier])) {
    _faviconView = [[FaviconView alloc] init];
    _titleLabel = [[UILabel alloc] init];
    _URLLabel = [[UILabel alloc] init];
    self.isAccessibilityElement = YES;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Accessors

- (NSString*)accessibilityLabel {
  NSString* accessibilityLabel = self.titleLabel.text;
  accessibilityLabel = [NSString
      stringWithFormat:@"%@, %@", accessibilityLabel, self.URLLabel.text];
  return accessibilityLabel;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.titleLabel.text) {
    [userInputLabels addObject:self.titleLabel.text];
  }
  return userInputLabels;
}

- (NSString*)accessibilityIdentifier {
  return self.titleLabel.text;
}

#pragma mark - Private

// Creates and lays out the favicon, title, and URL subviews.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (self.contentView.subviews.count != 0) {
    return;
  }
  FaviconContainerView* faviconContainerView = [self faviconContainerView];
  UIStackView* verticalStack = [self stackView];

  UIView* contentView = self.contentView;
  contentView.frame = self.frame;
  [contentView addSubview:faviconContainerView];
  [contentView addSubview:verticalStack];

  NSLayoutConstraint* heightConstraint = [contentView.heightAnchor
      constraintGreaterThanOrEqualToConstant:kTabListFromAndroidCellHeight];
  // Don't set the priority to required to avoid clashing with the estimated
  // height.
  heightConstraint.priority = UILayoutPriorityRequired - 1;

  CGFloat faviconContainerSize = kBringAndroidTabsFaviconSize + 3;

  [NSLayoutConstraint activateConstraints:@[
    [faviconContainerView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [faviconContainerView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:faviconContainerSize],
    [faviconContainerView.heightAnchor
        constraintEqualToAnchor:faviconContainerView.widthAnchor],

    [_faviconView.widthAnchor
        constraintEqualToConstant:kBringAndroidTabsFaviconSize],
    [_faviconView.heightAnchor
        constraintEqualToAnchor:_faviconView.widthAnchor],
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:faviconContainerView.centerYAnchor],
    [_faviconView.centerXAnchor
        constraintEqualToAnchor:faviconContainerView.centerXAnchor],

    [verticalStack.leadingAnchor
        constraintEqualToAnchor:faviconContainerView.trailingAnchor
                       constant:kTableViewSubViewHorizontalSpacing],
    [verticalStack.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],
    [verticalStack.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [verticalStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [verticalStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                 constant:
                                     -kTableViewTwoLabelsCellVerticalSpacing],
    heightConstraint
  ]];
}

// Creates and returns the vertical stack view. The stack consists of a title
// and URL.
- (UIStackView*)stackView {
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.numberOfLines = 2;
  _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _URLLabel.adjustsFontForContentSizeCategory = YES;
  _URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _URLLabel ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  return verticalStack;
}

// Creates and returns the container for the favicon view.
- (FaviconContainerView*)faviconContainerView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  [faviconContainerView addSubview:_faviconView];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconView.contentMode = UIViewContentModeScaleAspectFill;
  _faviconView.clipsToBounds = YES;
  _faviconView.layer.masksToBounds = YES;
  return faviconContainerView;
}

@end
