// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "cvc_header_item.h"

#import "base/mac/foundation_util.h"
#import "build/branding_buildflags.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing between elements.
const CGFloat kUISpacing = 5;
// Height of the Google pay badge.
const CGFloat kGooglePayBadgeHeight = 16;
}  // namespace

@implementation CVCHeaderItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CVCHeaderView class];
  }
  return self;
}

#pragma mark - TableViewHeaderFooterItem

- (void)configureHeaderFooterView:(CVCHeaderView*)cvcHeaderView
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:cvcHeaderView withStyler:styler];

  cvcHeaderView.instructionsLabel.text = self.instructionsText;
}

@end

@implementation CVCHeaderView {
  // View holding the google pay badge.
  UIImageView* _googlePayBadgeImageView;
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _instructionsLabel = [self createInstructionsLabel];
    [self.contentView addSubview:_instructionsLabel];

    _googlePayBadgeImageView = [self createGooglePayBadge];
    _googlePayBadgeImageView.image = [self googlePayBadgeImage];
    [self.contentView addSubview:_googlePayBadgeImageView];

    // Badge image aspect ratio (width / height).
    // Used to set the badge view size keeping its image aspect ratio without
    // any transparency in the view.
    CGFloat badgeAspectRatio = _googlePayBadgeImageView.image.size.width /
                               _googlePayBadgeImageView.image.size.height;

    [NSLayoutConstraint activateConstraints:@[
      // Instructions label
      [_instructionsLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewVerticalSpacing],
      [_instructionsLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:HorizontalPadding()],
      [_instructionsLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-HorizontalPadding()],
      // Google Pay Badge
      [_googlePayBadgeImageView.topAnchor
          constraintEqualToAnchor:_instructionsLabel.bottomAnchor
                         constant:kUISpacing],
      [_googlePayBadgeImageView.leadingAnchor
          constraintEqualToAnchor:_instructionsLabel.leadingAnchor],
      [_googlePayBadgeImageView.heightAnchor
          constraintEqualToConstant:kGooglePayBadgeHeight],
      [_googlePayBadgeImageView.widthAnchor
          constraintEqualToAnchor:_googlePayBadgeImageView.heightAnchor
                       multiplier:badgeAspectRatio],
      [_googlePayBadgeImageView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewVerticalSpacing],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _instructionsLabel.text = nil;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Dark/Light mode change ocurred.
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self userInterfaceStyleDidChange];
  }
}

#pragma mark - Private
// Returns a new UILabel to be used as the view's instruction label.
- (UILabel*)createInstructionsLabel {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

// Returns a new UIImageView with the google pay badge as image.
- (UIImageView*)createGooglePayBadge {
  UIImageView* googlePayBadge = [[UIImageView alloc] init];
  googlePayBadge.translatesAutoresizingMaskIntoConstraints = NO;
  googlePayBadge.contentMode = UIViewContentModeScaleAspectFit;

  return googlePayBadge;
}

// Returns the google pay badge image corresponding to the current
// UIUserInterfaceStyle (light/dark mode).
- (UIImage*)googlePayBadgeImage {
  // IDR_AUTOFILL_GOOGLE_PAY_DARK only exists in official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
             ? NativeImage(IDR_AUTOFILL_GOOGLE_PAY_DARK)
             : NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
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
