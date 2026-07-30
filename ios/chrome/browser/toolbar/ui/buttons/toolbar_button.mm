// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/ui_bundled/highlight_utils.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/blue_dot_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

constexpr CGFloat kDisabledOpacity = 0.4;

// Returns the tint color to be used in the normal mode.
UIColor* NormalTintColor() {
  if (IsNextOldDesignEnabled()) {
    return [UIColor colorNamed:kToolbarButtonColor];
  }
  return [UIColor colorNamed:kSolidBlackColor];
}

}  // namespace

@interface ToolbarButton ()

// The image from the imageLoader, if it has been loaded.
@property(nonatomic, strong, readonly) UIImage* image;

@end

@implementation ToolbarButton {
  ToolbarButtonImageLoader _imageLoader;
  UIView* _backgroundView;
  UIView* _blueDotView;
  UIView* _gradientView;
}

@synthesize image = _image;

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
                          incognito:(BOOL)incognito {
  if ((self = [super initWithFrame:CGRectMake(0, 0, kToolbarButtonSize,
                                              kToolbarButtonSize)])) {
    _imageLoader = [imageLoader copy];

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kToolbarButtonSize],
      [self.heightAnchor constraintEqualToConstant:kToolbarButtonSize],
    ]];

    if (!IsNextOldDesignEnabled()) {
      _backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
      _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
      _backgroundView.backgroundColor =
          ToolbarElementBackgroundColor(incognito);
      _backgroundView.userInteractionEnabled = NO;
      _backgroundView.clipsToBounds = YES;
      [self insertSubview:_backgroundView belowSubview:self.imageView];
      AddSameConstraints(self, _backgroundView);

      ConfigureCornerRadiusForToolbarButtonContainer(_backgroundView,
                                                     self.traitCollection);

      ConfigureShadowForToolbarElement(self);
    }

    self.tintColor = NormalTintColor();

    [self registerForTraitChanges:@[
      UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class
    ]
                       withAction:@selector(updateAppearance)];

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(userInterfaceStyleDidChange)];
  }
  return self;
}

#pragma mark - HighlightButton

- (NSArray<UIView*>*)highlightableViews {
  return @[ self.imageView ];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

#pragma mark - ContextMenuInteractionDelegate

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
    willDisplayMenuForConfiguration:(UIContextMenuConfiguration*)configuration
                           animator:
                               (id<UIContextMenuInteractionAnimating>)animator {
  if (IsPageActionMenuEnabled()) {
    [self.geminiHandler
        hideFloatyIfInvokedAnimated:YES
                         fromSource:gemini::FloatyUpdateSource::ContextMenu];
  }
  [super contextMenuInteraction:interaction
      willDisplayMenuForConfiguration:configuration
                             animator:animator];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  if (IsPageActionMenuEnabled()) {
    __weak __typeof(self) weakSelf = self;
    [animator addAnimations:^{
      [weakSelf.geminiHandler
          updateFloatyVisibilityIfEligibleAnimated:NO
                                        fromSource:gemini::FloatyUpdateSource::
                                                       ContextMenu];
    }];
  }
  [super contextMenuInteraction:interaction
        willEndForConfiguration:configuration
                       animator:animator];
}

#pragma mark - UIControl

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  if (enabled) {
    self.imageView.tintColor = NormalTintColor();
  } else {
    self.imageView.tintColor =
        [NormalTintColor() colorWithAlphaComponent:kDisabledOpacity];
  }
  [self updateAppearance];
}

#pragma mark - Properties

- (UIImage*)image {
  if (!_image) {
    _image = _imageLoader();
  }
  return _image;
}

- (void)setShadowAndBackgroundRemoved:(BOOL)shadowAndBackgroundRemoved {
  if (_shadowAndBackgroundRemoved == shadowAndBackgroundRemoved) {
    return;
  }
  _shadowAndBackgroundRemoved = shadowAndBackgroundRemoved;
  BOOL isDarkMode =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  if (_shadowAndBackgroundRemoved) {
    _backgroundView.backgroundColor =
        IsGlassToolbarEnabled() ? UIColor.clearColor
                                : ToolbarElementBackgroundColor(isDarkMode);
    self.layer.shadowColor = nil;
    self.layer.shadowOpacity = 0.0;
    self.layer.shadowOffset = CGSizeZero;
    self.layer.shadowRadius = 0;
  } else {
    _backgroundView.backgroundColor = ToolbarElementBackgroundColor(isDarkMode);
    ConfigureShadowForToolbarElement(self);
  }
}

- (void)setForceHidden:(BOOL)forceHidden {
  _forceHidden = forceHidden;
  [self updateAppearance];
}

- (void)setVisibilityMask:(ToolbarButtonVisibility)visibilityMask {
  _visibilityMask = visibilityMask;
  [self updateAppearance];
}

- (void)setIphHighlighted:(BOOL)iphHighlighted {
  if (_iphHighlighted == iphHighlighted) {
    return;
  }
  _iphHighlighted = iphHighlighted;
  [self updateHighlight];
}

- (void)setHasBlueDot:(BOOL)hasBlueDot {
  if (_hasBlueDot == hasBlueDot) {
    return;
  }
  _hasBlueDot = hasBlueDot;
  if (hasBlueDot && !_blueDotView) {
    // Do not add the blue dot to the background as the background will be
    // masked.
    _blueDotView = ConfigureAndAddBlueDotView(self);
  }
  _blueDotView.hidden = !hasBlueDot;
  if (hasBlueDot) {
    self.accessibilityValue = self.blueDotAccessibilityLabel;
  } else {
    self.accessibilityValue = nil;
  }
  [self updateMask];
  [self updateHighlight];
}

#pragma mark - Private

// Updates the highlight visibility.
- (void)updateHighlight {
  if (_iphHighlighted && !_hasBlueDot) {
    if (!_gradientView) {
      _gradientView = CreateIPHGradientView();
      if (_backgroundView) {
        [_backgroundView addSubview:_gradientView];
        AddSameConstraints(_backgroundView, _gradientView);
      } else {
        [self insertSubview:_gradientView belowSubview:self.imageView];
        AddSameConstraints(self, _gradientView);
      }
    }
    _gradientView.hidden = NO;
    ConfigureIPHImageStyleForImageView(self.imageView);
  } else {
    _gradientView.hidden = YES;
    RemoveIPHImageStyleFromImageView(self.imageView);
    self.imageView.tintColor = NormalTintColor();
  }
}

// Updates the mask on the background for the blue dot.
- (void)updateMask {
  if (_backgroundView) {
    UpdateBlueDotMaskForView(_backgroundView, _hasBlueDot);
  }
}

// Updates the image visibility based on the visibility of the button.
- (void)checkImageVisibility {
  if (!self.hidden && !self.currentImage) {
    [self setImage:self.image forState:UIControlStateNormal];
    [self updateHighlight];
  }
}

// Updates the appearance of this ToolbarButton.
- (void)updateAppearance {
  [self updateVisibility];
  [self updateShape];
}

// Helper for `-updateAppearance`. Updates the visibility of this button based
// on the current state and the visibility mask.
- (void)updateVisibility {
  if (self.forceHidden) {
    self.hidden = YES;
    return;
  }
  BOOL isCurrentRegularRegular = IsRegularXRegularSizeClass(self);
  BOOL isCurrentCompactHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  BOOL isCurrentWideLayout = isCurrentRegularRegular || isCurrentCompactHeight;

  switch (self.visibilityMask) {
    case ToolbarButtonVisibility::kAlways:
      break;
    case ToolbarButtonVisibility::kRegularRegular:
      self.hidden = !isCurrentRegularRegular;
      break;
    case ToolbarButtonVisibility::kWideLayout:
      self.hidden = !isCurrentWideLayout;
      break;
    case ToolbarButtonVisibility::kCompactHeight:
      self.hidden = !isCurrentCompactHeight;
      break;
    case ToolbarButtonVisibility::kWhenEnabled:
      self.hidden = !self.enabled;
      break;
  }
  [self checkImageVisibility];
}

// Helper for `-updateAppearance`. Updates the shape of this button based on the
// current size class of the UI. In windows with compact width, the
// ToolbarButton should be square. Otherwise, they should be circular.
- (void)updateShape {
  if (_backgroundView) {
    ConfigureCornerRadiusForToolbarButtonContainer(_backgroundView,
                                                   self.traitCollection);
    [self updateMask];
  }
}

// Handles user interface style trait collection changes.
- (void)userInterfaceStyleDidChange {
  if (!_shadowAndBackgroundRemoved) {
    ConfigureShadowForToolbarElement(self);
  }
}

@end
