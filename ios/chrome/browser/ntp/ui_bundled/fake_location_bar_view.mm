// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Fakebox highlight animation duration.
constexpr CGFloat kFakeboxHighlightDuration = 0.4;

// Fakebox highlight background alpha.
constexpr CGFloat kFakeboxHighlightAlpha = 0.06;

// Returns the top color of the Fakebox's gradient background.
UIColor* FakeboxTopColor() {
  return UIAccessibilityIsReduceTransparencyEnabled()
             ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
             : [UIColor colorNamed:@"fake_omnibox_top_gradient_color"];
}

// Returns the bottom color of the Fakebox's gradient background.
UIColor* FakeboxBottomColor() {
  return UIAccessibilityIsReduceTransparencyEnabled()
             ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
             : [UIColor colorNamed:@"fake_omnibox_bottom_gradient_color"];
}

}  // namespace

@implementation FakeLocationBarView {
  GradientView* _fakeLocationBarGradientView;
  UIVisualEffectView* _fakeLocationBarBlurEffectView;
  UIView* _fakeLocationBarHighlightView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.clipsToBounds = YES;

    _fakeLocationBarGradientView =
        [[GradientView alloc] initWithTopColor:FakeboxTopColor()
                                   bottomColor:FakeboxBottomColor()];
    _fakeLocationBarGradientView.userInteractionEnabled = NO;
    _fakeLocationBarGradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_fakeLocationBarGradientView];
    AddSameConstraints(self, _fakeLocationBarGradientView);

    if (IsNTPBackgroundCustomizationEnabled()) {
      UIVisualEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
      _fakeLocationBarBlurEffectView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      _fakeLocationBarBlurEffectView.userInteractionEnabled = NO;
      _fakeLocationBarBlurEffectView.translatesAutoresizingMaskIntoConstraints =
          NO;
      [self addSubview:_fakeLocationBarBlurEffectView];
      AddSameConstraints(self, _fakeLocationBarBlurEffectView);
    }

    _fakeLocationBarHighlightView = [[UIView alloc] init];
    _fakeLocationBarHighlightView.userInteractionEnabled = NO;
    _fakeLocationBarHighlightView.backgroundColor = UIColor.clearColor;
    _fakeLocationBarHighlightView.translatesAutoresizingMaskIntoConstraints =
        NO;
    [self addSubview:_fakeLocationBarHighlightView];
    AddSameConstraints(self, _fakeLocationBarHighlightView);

    // Make sure the correct background is visible.
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self applyBackgroundTheme];
    } else {
      _fakeLocationBarGradientView.hidden = NO;
      _fakeLocationBarBlurEffectView.hidden = YES;
    }
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [UIView animateWithDuration:kFakeboxHighlightDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     CGFloat alpha = highlighted ? kFakeboxHighlightAlpha : 0;
                     self->_fakeLocationBarHighlightView.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

- (void)updateColorsWithProgress:(CGFloat)progress
                    colorPalette:(NewTabPageColorPalette*)colorPalette {
  UIColor* pinnedColor = [UIColor colorNamed:kTextfieldBackgroundColor];

  // Use a quadratic curve interpolation.
  progress = progress * progress;
  [_fakeLocationBarGradientView
      setStartColor:BlendColors(colorPalette ? colorPalette.omniboxColor
                                             : FakeboxTopColor(),
                                pinnedColor, progress)
           endColor:BlendColors(colorPalette ? colorPalette.omniboxColor
                                             : FakeboxBottomColor(),
                                pinnedColor, progress)];
}

- (void)applyBackgroundTheme {
  BOOL hasBlurredBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];

  if (hasBlurredBackground) {
    _fakeLocationBarGradientView.hidden = YES;
    _fakeLocationBarBlurEffectView.hidden = NO;
  } else {
    _fakeLocationBarGradientView.hidden = NO;
    _fakeLocationBarBlurEffectView.hidden = YES;
  }
}

@end
