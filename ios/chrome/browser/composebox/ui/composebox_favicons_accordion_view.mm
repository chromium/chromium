// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_favicons_accordion_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Maximum number of favicon icons displayed before showing a count badge.
const NSUInteger kMaxDisplayedFavicons = 3;

// Spacing between favicon views in the stack view. Negative value creates
// overlapping effect.
const CGFloat kFaviconStackSpacing = -4.0;

// Size (width and height) of each favicon icon view and badge.
const CGFloat kFaviconSize = 24.0;

// Corner radius for favicon icon views and badge.
const CGFloat kFaviconCornerRadius = 12.0;

// Border width for favicon icon views and badge.
const CGFloat kFaviconBorderWidth = 1.5;

// Font size for the badge label.
const CGFloat kBadgeFontSize = 10.0;

}  // namespace

@implementation ComposeboxFaviconsAccordionView {
  UIActivityIndicatorView* _activityIndicator;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.axis = UILayoutConstraintAxisHorizontal;
    self.alignment = UIStackViewAlignmentCenter;
    self.spacing = kFaviconStackSpacing;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    _activityIndicator = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    _activityIndicator.hidesWhenStopped = YES;
  }
  return self;
}

- (void)setIsLoading:(BOOL)isLoading {
  if (_isLoading == isLoading) {
    return;
  }
  _isLoading = isLoading;
  if (_isLoading) {
    for (UIView* view in self.arrangedSubviews) {
      [view removeFromSuperview];
    }
    [self addArrangedSubview:_activityIndicator];
    [_activityIndicator startAnimating];
  } else {
    [_activityIndicator stopAnimating];
    [_activityIndicator removeFromSuperview];
  }
}

- (void)updateWithImages:(NSArray<UIImage*>*)images {
  if (self.isLoading) {
    return;
  }

  for (UIView* view in self.arrangedSubviews) {
    [view removeFromSuperview];
  }

  for (NSUInteger i = 0; i < images.count; i++) {
    UIImage* image = [images objectAtIndex:i];

    if ((i < (kMaxDisplayedFavicons - 1)) ||
        images.count == kMaxDisplayedFavicons) {
      UIImageView* iconView = [self createTabIconViewWithImage:image];
      [self addArrangedSubview:iconView];
    } else {
      UILabel* badge = [self
          createTabBadgeViewWithCount:images.count - kMaxDisplayedFavicons + 1];
      [self addArrangedSubview:badge];
      break;
    }
  }
}

#pragma mark - Private

/// Creates a favicon icon view for a tab in the accordion stack.
- (UIImageView*)createTabIconViewWithImage:(UIImage*)image {
  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [iconView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [iconView.heightAnchor constraintEqualToConstant:kFaviconSize]
  ]];
  iconView.layer.cornerRadius = kFaviconCornerRadius;
  iconView.clipsToBounds = YES;
  iconView.layer.borderColor = [UIColor colorNamed:kBackgroundColor].CGColor;
  iconView.layer.borderWidth = kFaviconBorderWidth;
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  iconView.image =
      image ?: DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kFaviconSize);
  return iconView;
}

/// Creates a badge label view showing the remaining tabs count.
- (UILabel*)createTabBadgeViewWithCount:(NSUInteger)count {
  UILabel* badge = [[UILabel alloc] init];
  badge.text = [NSString stringWithFormat:@"+%lu", (unsigned long)count];
  badge.font = [UIFont systemFontOfSize:kBadgeFontSize
                                 weight:UIFontWeightMedium];
  badge.textAlignment = NSTextAlignmentCenter;
  badge.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  badge.textColor = [UIColor colorNamed:kTextPrimaryColor];
  badge.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [badge.widthAnchor constraintEqualToConstant:kFaviconSize],
    [badge.heightAnchor constraintEqualToConstant:kFaviconSize]
  ]];
  badge.layer.cornerRadius = kFaviconCornerRadius;
  badge.clipsToBounds = YES;
  badge.layer.borderColor = [UIColor colorNamed:kBackgroundColor].CGColor;
  badge.layer.borderWidth = kFaviconBorderWidth;
  return badge;
}

@end
