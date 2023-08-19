// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/version_item.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
const CGFloat kVerticalSpacing = 16;
}  // namespace

#pragma mark - VersionItem

@implementation VersionItem

@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.accessibilityIdentifier = @"Version cell";
    self.cellClass = [VersionFooter class];
  }
  return self;
}

#pragma mark - TableViewHeaderFooterItem

- (void)configureHeaderFooterView:(VersionFooter*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];
  headerFooter.textLabel.text = self.text;
}

@end

#pragma mark - VersionFooter

@implementation VersionFooter

@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    self.accessibilityHint = l10n_util::GetNSString(IDS_IOS_COPY_VERSION_HINT);

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textLabel.numberOfLines = 0;
    _textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _textLabel.textAlignment = NSTextAlignmentCenter;
    [self.contentView addSubview:_textLabel];

    UIButton* button = [[UIButton alloc] init];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button addTarget:self
                  action:@selector(buttonTapped)
        forControlEvents:UIControlEventTouchUpInside];
    [self.contentView addSubview:button];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    AddSameConstraints(button, self.contentView);
    [NSLayoutConstraint activateConstraints:@[
      heightConstraint,
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalSpacing]
    ]];
  }
  return self;
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

#pragma mark - UITableViewHeaderFooterView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
}

#pragma mark - Private

// Callback for the button
- (void)buttonTapped {
  [self.delegate didTapVersionFooter:self];
}

@end
