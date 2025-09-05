// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_background_view.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation MagicStackModuleBackgroundView {
  UIView* _backgroundColorView;
  UIVisualEffectView* _backgroundBlurView;

  BOOL _faded;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:
                @[ NewTabPageTrait.class, NewTabPageImageBackgroundTrait.class ]
                         withAction:@selector(updateBackground)];
    }
    [self updateBackground];
  }
  return self;
}

- (void)setBackgroundColor:(UIColor*)color {
  [super setBackgroundColor:color];
}

#pragma mark - Private

- (UIVisualEffectView*)backgroundBlurView {
  if (_backgroundBlurView) {
    return _backgroundBlurView;
  }

  _backgroundBlurView = [[UIVisualEffectView alloc] initWithEffect:nil];
  _backgroundBlurView.translatesAutoresizingMaskIntoConstraints = NO;
  return _backgroundBlurView;
}

- (UIView*)backgroundColorView {
  if (_backgroundColorView) {
    return _backgroundColorView;
  }
  _backgroundColorView = [[UIView alloc] init];
  _backgroundColorView.translatesAutoresizingMaskIntoConstraints = NO;
  return _backgroundColorView;
}

- (void)updateBackground {
  // If the background is an image, the modules use a blurred background.
  BOOL hasBlurredBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];
  if (hasBlurredBackground) {
    UIView* backgroundBlurView = [self backgroundBlurView];
    if (!backgroundBlurView.superview) {
      [self addSubview:backgroundBlurView];
      AddSameConstraints(self, backgroundBlurView);
    }
    if (_backgroundColorView) {
      [_backgroundColorView removeFromSuperview];
      _backgroundColorView = nil;
    }
    [self updateFadedState];
    return;
  }

  UIView* backgroundColorView = [self backgroundColorView];
  if (!backgroundColorView.superview) {
    [self addSubview:backgroundColorView];
    AddSameConstraints(self, backgroundColorView);
  }
  if (_backgroundBlurView) {
    [_backgroundBlurView removeFromSuperview];
    _backgroundBlurView = nil;
  }

  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  backgroundColorView.backgroundColor =
      colorPalette ? colorPalette.secondaryCellColor
                   : [UIColor colorNamed:kBackgroundColor];

  [self updateFadedState];
}

- (void)fadeIn {
  _faded = NO;
  [self updateFadedState];
}

- (void)fadeOut {
  _faded = YES;
  [self updateFadedState];
}

// Updates the currently active view to the correct faded state.
- (void)updateFadedState {
  if (_backgroundBlurView) {
    _backgroundBlurView.effect =
        _faded ? nil
               : [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  }

  if (_backgroundColorView) {
    _backgroundColorView.alpha = _faded ? 0 : 1;
  }
}

@end
