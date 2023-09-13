// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {

// Favicon constants.
const CGFloat kFaviconSize = 24.0;
const CGFloat kFavIconCornerRadius = 5.0;
const CGFloat kFaviconBackgroundSize = 30.0;
const CGFloat kFavIconBackgroundCornerRadius = 7.0;
const CGFloat kFaviconContainerSize = 56.0;
const CGFloat kFavIconContainerCornerRadius = 12.0;

// Stacks constants.
const CGFloat kContainerStackSpacing = 14.0;
const CGFloat kLabelStackSpacing = 5.0;

}  // namespace

@implementation TabResumptionView {
  // Item used to configure the view.
  TabResumptionItem* _item;
  // The view container.
  UIStackView* _containerStackView;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (!_containerStackView) {
    [self createSubviews];
    [self addTapGestureRecognizer];
  }
}

#pragma mark - Private methods

// Creates all the subviews.
- (void)createSubviews {
  self.accessibilityIdentifier = kTabResumptionViewIdentifier;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  _containerStackView = [self configuredContainerStackView];

  UIView* faviconContainerView = [self configuredFaviconImageContainer];
  [_containerStackView addArrangedSubview:faviconContainerView];

  UIStackView* labelStackView = [self configuredLabelStackView];
  [_containerStackView addArrangedSubview:labelStackView];

  UILabel* sessionLabel;
  if (_item.itemType == TabResumptionItemType::kLastSyncedTab) {
    sessionLabel = [self configuredSessionLabel];
    [labelStackView addArrangedSubview:sessionLabel];
  }
  UILabel* tabTitleLabel = [self configuredTabTitleLabel];
  [labelStackView addArrangedSubview:tabTitleLabel];
  UILabel* hostnameAndSyncTimeLabel = [self configuredHostNameAndSyncTimeLabel];
  [labelStackView addArrangedSubview:hostnameAndSyncTimeLabel];

  if (_item.itemType == TabResumptionItemType::kLastSyncedTab) {
    self.accessibilityLabel = [NSString
        stringWithFormat:@"%@,%@,%@", sessionLabel.text, tabTitleLabel.text,
                         hostnameAndSyncTimeLabel.text];
  } else {
    self.accessibilityLabel =
        [NSString stringWithFormat:@"%@,%@", tabTitleLabel.text,
                                   hostnameAndSyncTimeLabel.text];
  }

  [self addSubview:_containerStackView];
  AddSameConstraints(_containerStackView, self);
}

// Adds a tap gesture recognizer to the view.
- (void)addTapGestureRecognizer {
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(tabResumptionItemTapped:)];
  [self addGestureRecognizer:tapRecognizer];
}

// Configures and returns the UIStackView that contains the faviconContainerView
// and the labelsStackView.
- (UIStackView*)configuredContainerStackView {
  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentCenter;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  return containerStack;
}

// Configures and returns the UIStackView that contains the different labels.
- (UIStackView*)configuredLabelStackView {
  UIStackView* labelStackView = [[UIStackView alloc] init];
  labelStackView.axis = UILayoutConstraintAxisVertical;
  labelStackView.spacing = kLabelStackSpacing;
  labelStackView.translatesAutoresizingMaskIntoConstraints = NO;
  return labelStackView;
}

// Configures and returns the leading UIView that contains the favicon image.
- (UIView*)configuredFaviconImageContainer {
  UIView* faviconContainerView = [[UIView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconContainerView.layer.cornerRadius = kFavIconContainerCornerRadius;
  faviconContainerView.backgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];

  UIView* faviconBackgroundView = [[UIView alloc] init];
  faviconBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBackgroundView.layer.cornerRadius = kFavIconBackgroundCornerRadius;
  faviconBackgroundView.backgroundColor = UIColor.whiteColor;
  [faviconContainerView addSubview:faviconBackgroundView];

  UIImageView* faviconImageView = [[UIImageView alloc] init];
  if (_item.faviconImage) {
    faviconImageView.image = _item.faviconImage;
  } else {
    // If the `faviconImage` property is nil, add a default symbol icon.
    faviconImageView.image =
        CustomSymbolWithPointSize(kRecentTabsSymbol, kFaviconSize);
    faviconImageView.backgroundColor = [UIColor colorNamed:kGreen500Color];
    faviconBackgroundView.backgroundColor = [UIColor colorNamed:kGreen500Color];
    faviconImageView.tintColor = UIColor.whiteColor;
  }
  faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconImageView.contentMode = UIViewContentModeScaleAspectFit;
  faviconImageView.clipsToBounds = YES;
  faviconImageView.layer.cornerRadius = kFavIconCornerRadius;
  [faviconBackgroundView addSubview:faviconImageView];

  [NSLayoutConstraint activateConstraints:@[
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconContainerView.heightAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconBackgroundView.widthAnchor
        constraintEqualToConstant:kFaviconBackgroundSize],
    [faviconBackgroundView.heightAnchor
        constraintEqualToConstant:kFaviconBackgroundSize],
    [faviconImageView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [faviconImageView.heightAnchor constraintEqualToConstant:kFaviconSize],
  ]];
  AddSameCenterConstraints(faviconBackgroundView, faviconContainerView);
  AddSameCenterConstraints(faviconImageView, faviconContainerView);

  return faviconContainerView;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredSessionLabel {
  NSString* sessionString =
      l10n_util::GetNSStringF(IDS_IOS_TAB_RESUMPTION_TILE_HOST_LABEL,
                              base::SysNSStringToUTF16(_item.sessionName));

  UILabel* label = [[UILabel alloc] init];
  label.text = sessionString.uppercaseString;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  label.numberOfLines = 1;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredTabTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.text = [_item.tabTitle length]
                   ? _item.tabTitle
                   : l10n_util::GetNSString(
                         IDS_IOS_TAB_RESUMPTION_TAB_TITLE_PLACEHOLDER);
  label.font = CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold);
  label.numberOfLines = 1;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  return label;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredHostNameAndSyncTimeLabel {
  NSString* hostnameAndSyncTimeString = [NSString
      stringWithFormat:@"%@ • %@", [self hostnameFromGURL:_item.tabURL],
                       [self lastSyncTimeStringFromTime:_item.syncedTime]];

  UILabel* label = [[UILabel alloc] init];
  label.text = hostnameAndSyncTimeString;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 1;
  label.lineBreakMode = NSLineBreakByTruncatingMiddle;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

// Returns the last sync string from the given `time`.
- (NSString*)lastSyncTimeStringFromTime:(base::Time)time {
  base::TimeDelta lastUsedDelta = base::Time::Now() - time;
  base::TimeDelta oneMinuteDelta = base::Minutes(1);

  // If the tab was synchronized within the last minute, returns 'just now'.
  if (lastUsedDelta < oneMinuteDelta) {
    return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_JUST_NOW);
  }

  // This will return something similar to "2 mins/hours ago".
  return base::SysUTF16ToNSString(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT, lastUsedDelta));
}

// Called when the view has been tapped.
- (void)tabResumptionItemTapped:(UIGestureRecognizer*)sender {
  [self.delegate tabResumptionViewTapped];
}

@end
