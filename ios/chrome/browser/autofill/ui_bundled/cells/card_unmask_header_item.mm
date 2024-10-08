// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/cells/card_unmask_header_item.h"

#import "base/apple/foundation_util.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between elements.
const CGFloat kUISpacing = 5;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google pay badge.
const CGFloat kGooglePayBadgeHeight = 16;
#endif

}  // namespace

@implementation CardUnmaskHeaderItem {
}

- (instancetype)initWithType:(NSInteger)type
                   titleText:(NSString*)titleText
            instructionsText:(NSString*)instructionsText {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CardUnmaskHeaderView class];
    _titleText = titleText;
    _instructionsText = instructionsText;
  }
  return self;
}

- (NSString*)accessibilityLabels {
  return [NSString stringWithFormat:@"%@\n%@", _titleText, _instructionsText];
}

#pragma mark - TableViewHeaderFooterItem

- (void)configureHeaderFooterView:(CardUnmaskHeaderView*)cardUnmaskHeaderView
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:cardUnmaskHeaderView withStyler:styler];
  cardUnmaskHeaderView.titleLabel.text = _titleText;
  cardUnmaskHeaderView.instructionsLabel.text = _instructionsText;
}

@end

@implementation CardUnmaskHeaderView {
  // View holding the google pay badge.
  UIImageView* _googlePayBadgeImageView;
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _instructionsLabel = [self createInstructionsLabel];
    [self.contentView addSubview:_instructionsLabel];

    _googlePayBadgeImageView = [self createGooglePayBadge];
    [self.contentView addSubview:_googlePayBadgeImageView];

    // Badge image aspect ratio (width / height).
    // Used to set the badge view size keeping its image aspect ratio without
    // any transparency in the view.
    CGFloat badgeAspectRatio = _googlePayBadgeImageView.image.size.width /
                               _googlePayBadgeImageView.image.size.height;

    _titleLabel = [self createTitleLabel];
    [self.contentView addSubview:_titleLabel];

    [NSLayoutConstraint activateConstraints:@[
      // Google Pay Badge
      [_googlePayBadgeImageView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewImagePadding],
      [_googlePayBadgeImageView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_googlePayBadgeImageView.heightAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_googlePayBadgeImageView.widthAnchor
          constraintEqualToAnchor:_googlePayBadgeImageView.heightAnchor
                       multiplier:badgeAspectRatio],

      // Title label
      [_titleLabel.topAnchor
          constraintEqualToAnchor:_googlePayBadgeImageView.bottomAnchor
                         constant:kTableViewImagePadding],
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:HorizontalPadding()],
      [_titleLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-HorizontalPadding()],

      // Instructions label
      [_instructionsLabel.topAnchor
          constraintEqualToAnchor:_titleLabel.bottomAnchor
                         constant:kUISpacing],
      [_instructionsLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:HorizontalPadding()],
      [_instructionsLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-HorizontalPadding()],
      [_instructionsLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
    ]];

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:TraitCollectionSetForTraits(
                                        @[ UITraitUserInterfaceStyle.self ])
                         withAction:@selector(userInterfaceStyleDidChange)];
    }
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  _instructionsLabel.text = nil;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 17, *)) {
    return;
  }
  // Dark/Light mode change ocurred.
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self userInterfaceStyleDidChange];
  }
}
#endif

#pragma mark - Private
// Returns a new UILabel to be used as the view's title label.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textAlignment = NSTextAlignmentCenter;
  return label;
}

// Returns a new UILabel to be used as the view's instruction label.
- (UILabel*)createInstructionsLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textAlignment = NSTextAlignmentCenter;
  return label;
}

// Returns a new UIImageView with the google pay badge as image.
- (UIImageView*)createGooglePayBadge {
  UIImageView* googlePayBadge = [[UIImageView alloc] init];
  googlePayBadge.translatesAutoresizingMaskIntoConstraints = NO;
  googlePayBadge.contentMode = UIViewContentModeScaleAspectFit;
  googlePayBadge.image = [self googlePayBadgeImage];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  googlePayBadge.isAccessibilityElement = YES;
  googlePayBadge.accessibilityLabel =
      l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME);
#endif
  return googlePayBadge;
}

// Returns the google pay badge image corresponding to the current
// UIUserInterfaceStyle (light/dark mode).
- (UIImage*)googlePayBadgeImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGooglePaySymbol, kGooglePayBadgeHeight));
#else
  return NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#endif
}

// Updates the view after a change in light/dark mode.
- (void)userInterfaceStyleDidChange {
  // Update google pay image to the asset corresponding to light/dark mode.
  _googlePayBadgeImageView.image = [self googlePayBadgeImage];
}
@end
