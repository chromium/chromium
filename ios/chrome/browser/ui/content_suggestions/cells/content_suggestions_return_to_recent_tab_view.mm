// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"

#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kContentViewCornerRadius = 12.0f;
const CGFloat kContentViewBorderWidth = 1.0f;
const CGFloat kIconCornerRadius = 4.0f;
const CGFloat kContentViewSubviewSpacing = 12.0f;
const CGFloat kIconWidth = 32.0f;
}

@implementation ContentSuggestionsReturnToRecentTabView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = IsMagicStackEnabled()
                                  ? kHomeModuleContainerCornerRadius
                                  : kContentViewCornerRadius;
    self.layer.masksToBounds = YES;
    if (IsMagicStackEnabled()) {
      self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    } else {
      [self.layer
          setBorderColor:[UIColor colorNamed:kTertiaryBackgroundColor].CGColor];
      [self.layer setBorderWidth:kContentViewBorderWidth];
    }

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.isAccessibilityElement = NO;
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.backgroundColor = UIColor.clearColor;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitleLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.backgroundColor = UIColor.clearColor;

    UIStackView* textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    textStackView.axis = UILayoutConstraintAxisVertical;
    [self addSubview:textStackView];

    _iconImageView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kGlobeAmericasSymbol,
                                                 kIconWidth)];
    _iconImageView.tintColor = [UIColor colorNamed:kGrey400Color];
    _iconImageView.layer.cornerRadius = kIconCornerRadius;
    _iconImageView.layer.masksToBounds = YES;
    [self addSubview:_iconImageView];

    UIImageView* disclosureImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
    [disclosureImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    [self addSubview:disclosureImageView];

    UIStackView* horizontalStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _iconImageView, textStackView, disclosureImageView
        ]];
    horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
    horizontalStackView.alignment = UIStackViewAlignmentCenter;
    horizontalStackView.spacing = kContentViewSubviewSpacing;
    [self addSubview:horizontalStackView];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconWidth],
      [_iconImageView.heightAnchor
          constraintEqualToAnchor:_iconImageView.widthAnchor],
      [horizontalStackView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [horizontalStackView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [horizontalStackView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kContentViewSubviewSpacing],
      [horizontalStackView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kContentViewSubviewSpacing],
    ]];
  }
  return self;
}

- (instancetype)initWithConfiguration:
    (ContentSuggestionsReturnToRecentTabItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    self.titleLabel.text = config.title;
    self.subtitleLabel.text = config.subtitle;
    self.isAccessibilityElement = YES;
    self.accessibilityLabel = config.title;
    self.iconImageView.image = config.icon;
    if (!config.icon) {
      self.iconImageView.hidden = YES;
    }
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    // CGColors are static RGB, so the border color needs to be reset.
    [self.layer
        setBorderColor:[UIColor colorNamed:kTertiaryBackgroundColor].CGColor];
  }
}

@end
