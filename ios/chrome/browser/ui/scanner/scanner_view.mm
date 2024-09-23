// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/scanner_view.h"

#import <numbers>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/numerics/math_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/scanner/preview_overlay_view.h"
#import "ios/chrome/browser/ui/scanner/video_preview_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Padding of the viewport caption, below the viewport.
const CGFloat kViewportCaptionVerticalPadding = 14.0;
// Padding of the viewport caption from the edges of the superview.
const CGFloat kViewportCaptionHorizontalPadding = 31.0;
// Shadow opacity of the viewport caption.
const CGFloat kViewportCaptionShadowOpacity = 1.0;
// Shadow radius of the viewport caption.
const CGFloat kViewportCaptionShadowRadius = 5.0;

// Duration of the flash animation played when a code is scanned.
const CGFloat kFlashDuration = 0.5;

}  // namespace

@interface ScannerView () {
  // A scrollview containing the viewport's caption.
  UIScrollView* _captionContainer;
  // A button to toggle the torch.
  UIBarButtonItem* _torchButton;
  // A view containing the preview layer for camera input.
  VideoPreviewView* _previewView;
  // A transparent overlay on top of the preview layer.
  PreviewOverlayView* _previewOverlay;
  // The constraint specifying that the preview overlay should be square.
  NSLayoutConstraint* _overlaySquareConstraint;
  // The constraint relating the size of the `_previewOverlay` to the width of
  // the ScannerView.
  NSLayoutConstraint* _overlayWidthConstraint;
  // The constraint relating the size of the `_previewOverlay` to the height of
  // te ScannerView.
  NSLayoutConstraint* _overlayHeightConstraint;
}

@end

@implementation ScannerView

#pragma mark - lifecycle

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<ScannerViewDelegate>)delegate {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }
  DCHECK(delegate);
  _delegate = delegate;
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(maybeHideCaptions)];
  }

  return self;
}

#pragma mark - UIView

// TODO(crbug.com/40478852): Replace the preview overlay with a UIView which is
// not resized.
- (void)layoutSubviews {
  [super layoutSubviews];
  [self setBackgroundColor:[UIColor blackColor]];
  if (CGRectEqualToRect([_previewView bounds], CGRectZero)) {
    [_previewView setBounds:self.bounds];
  }
  [_previewView setCenter:CGPointMake(CGRectGetMidX(self.bounds),
                                      CGRectGetMidY(self.bounds))];
}

- (void)willMoveToSuperview:(UIView*)superview {
  // Set up subviews if they don't already exist.
  if (superview && self.subviews.count == 0) {
    [self setupPreviewView];
    [self setupPreviewOverlayView];
    [self addSubviews];
  }
}

#pragma mark - public methods

- (AVCaptureVideoPreviewLayer*)previewLayer {
  return [_previewView previewLayer];
}

- (void)enableTorchButton:(BOOL)torchIsAvailable {
  [_torchButton setEnabled:torchIsAvailable];
  if (!torchIsAvailable) {
    [self setTorchButtonTo:NO];
  }
}

- (void)setTorchButtonTo:(BOOL)torchIsOn {
  DCHECK(_torchButton);
  UIImage* icon = nil;
  NSString* accessibilityValue = nil;
  if (torchIsOn) {
    icon = [self torchOnIcon];
    accessibilityValue =
        l10n_util::GetNSString(IDS_IOS_SCANNER_TORCH_ON_ACCESSIBILITY_VALUE);
  } else {
    icon = [self torchOffIcon];
    accessibilityValue =
        l10n_util::GetNSString(IDS_IOS_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE);
  }
  [_torchButton setImage:icon];
  [_torchButton setAccessibilityValue:accessibilityValue];
}

- (void)resetPreviewFrame:(CGSize)size {
  [_previewView setTransform:CGAffineTransformIdentity];
  [_previewView setFrame:CGRectMake(0, 0, size.width, size.height)];
}

- (void)rotatePreviewByAngle:(CGFloat)angle {
  [_previewView
      setTransform:CGAffineTransformRotate([_previewView transform], angle)];
}

- (void)finishPreviewRotation {
  CGAffineTransform rotation = [_previewView transform];
  // Check that the current transform is either an identity or a 90, -90, or 180
  // degree rotation.
  DCHECK(fabs(atan2f(rotation.b, rotation.a)) < 0.001 ||
         fabs(fabs(atan2f(rotation.b, rotation.a)) -
              std::numbers::pi_v<float>) < 0.001 ||
         fabs(fabs(atan2f(rotation.b, rotation.a)) -
              std::numbers::pi_v<float> / 2) < 0.001);
  rotation.a = round(rotation.a);
  rotation.b = round(rotation.b);
  rotation.c = round(rotation.c);
  rotation.d = round(rotation.d);
  [_previewView setTransform:rotation];
}

- (CGRect)viewportRegionOfInterest {
  return [_previewView viewportRegionOfInterest];
}

- (CGRect)viewportRectOfInterest {
  return [_previewView viewportRectOfInterest];
}

- (void)animateScanningResultWithCompletion:(void (^)(void))completion {
  UIView* whiteView = [[UIView alloc] init];
  whiteView.frame = self.bounds;
  [self addSubview:whiteView];
  whiteView.backgroundColor = [UIColor whiteColor];
  [UIView animateWithDuration:kFlashDuration
      animations:^{
        whiteView.alpha = 0.0;
      }
      completion:^void(BOOL finished) {
        [whiteView removeFromSuperview];
        if (completion) {
          completion();
        }
      }];
}

- (CGSize)viewportSize {
  return self.window.frame.size;
}

- (NSString*)caption {
  return @"";
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self maybeHideCaptions];
}
#endif

#pragma mark - private methods

// Creates an image with template rendering mode for use in icons.
- (UIImage*)templateImageWithName:(NSString*)name {
  UIImage* image = [[UIImage imageNamed:name]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  DCHECK(image);
  return image;
}

// Creates an icon for torch turned on.
- (UIImage*)torchOnIcon {
  UIImage* icon = [self templateImageWithName:@"scanner_torch_on"];
  return icon;
}

// Creates an icon for torch turned off.
- (UIImage*)torchOffIcon {
  UIImage* icon = [self templateImageWithName:@"scanner_torch_off"];
  return icon;
}

// Adds the subviews.
- (void)addSubviews {
  UIBarButtonItem* close =
      [[UIBarButtonItem alloc] initWithImage:[ChromeIcon closeIcon]
                                       style:UIBarButtonItemStylePlain
                                      target:_delegate
                                      action:@selector(dismissScannerView:)];
  close.accessibilityLabel = [[ChromeIcon closeIcon] accessibilityLabel];
  UIBarButtonItem* spacer = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  _torchButton =
      [[UIBarButtonItem alloc] initWithImage:[self torchOffIcon]
                                       style:UIBarButtonItemStylePlain
                                      target:_delegate
                                      action:@selector(toggleTorch:)];
  _torchButton.enabled = NO;
  _torchButton.accessibilityIdentifier = @"scanner_torch_button";
  _torchButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL);
  _torchButton.accessibilityValue =
      l10n_util::GetNSString(IDS_IOS_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE);
  UIToolbar* toolbar = [[UIToolbar alloc] init];
  toolbar.items = @[ close, spacer, _torchButton ];
  toolbar.tintColor = UIColor.whiteColor;
  [toolbar setBackgroundImage:[[UIImage alloc] init]
           forToolbarPosition:UIToolbarPositionAny
                   barMetrics:UIBarMetricsDefault];
  [toolbar setShadowImage:[[UIImage alloc] init]
       forToolbarPosition:UIBarPositionAny];

  [toolbar setBackgroundColor:[UIColor clearColor]];
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:toolbar];

  AddSameConstraintsToSides(self, toolbar,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  [toolbar.bottomAnchor
      constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor]
      .active = YES;

  UILabel* viewportCaption = [[UILabel alloc] init];
  NSString* label = [self caption];
  [viewportCaption setText:label];
  [viewportCaption
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
  [viewportCaption setAdjustsFontForContentSizeCategory:YES];
  [viewportCaption setNumberOfLines:0];
  [viewportCaption setTextAlignment:NSTextAlignmentCenter];
  [viewportCaption setAccessibilityLabel:label];
  [viewportCaption setAccessibilityIdentifier:@"scanner_viewport_caption"];
  [viewportCaption setTextColor:[UIColor whiteColor]];
  [viewportCaption.layer setShadowColor:[UIColor blackColor].CGColor];
  [viewportCaption.layer setShadowOffset:CGSizeZero];
  [viewportCaption.layer setShadowRadius:kViewportCaptionShadowRadius];
  [viewportCaption.layer setShadowOpacity:kViewportCaptionShadowOpacity];
  [viewportCaption.layer setMasksToBounds:NO];
  [viewportCaption.layer setShouldRasterize:YES];

  _captionContainer = [[UIScrollView alloc] init];
  _captionContainer.showsVerticalScrollIndicator = NO;
  [self addSubview:_captionContainer];
  [_captionContainer addSubview:viewportCaption];

  // Constraints for viewportCaption.
  _captionContainer.translatesAutoresizingMaskIntoConstraints = NO;
  viewportCaption.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_captionContainer.topAnchor
        constraintEqualToAnchor:self.centerYAnchor
                       constant:self.viewportSize.height / 2 +
                                kViewportCaptionVerticalPadding],
    [_captionContainer.bottomAnchor constraintEqualToAnchor:toolbar.topAnchor],
    [_captionContainer.centerXAnchor
        constraintEqualToAnchor:_previewOverlay.centerXAnchor],
    [_captionContainer.contentLayoutGuide.widthAnchor
        constraintEqualToAnchor:_captionContainer.widthAnchor],
    [_captionContainer.widthAnchor
        constraintLessThanOrEqualToAnchor:self.widthAnchor
                                 constant:-2 *
                                          kViewportCaptionHorizontalPadding],
    [_captionContainer.widthAnchor
        constraintLessThanOrEqualToAnchor:self.heightAnchor
                                 constant:-2 *
                                          kViewportCaptionHorizontalPadding],
  ]];
  AddSameConstraints(_captionContainer, viewportCaption);

  // There is no space for at least 1 line of text in compact height.
  _captionContainer.hidden =
      UIUserInterfaceSizeClassCompact == self.traitCollection.verticalSizeClass;
}

// Adds a preview view to `self` and configures its layout constraints.
- (void)setupPreviewView {
  DCHECK(!_previewView);
  _previewView = [[VideoPreviewView alloc] initWithFrame:self.frame
                                            viewportSize:[self viewportSize]];
  [self insertSubview:_previewView atIndex:0];
}

// Adds a transparent overlay with a viewport border to `self` and configures
// its layout constraints.
- (void)setupPreviewOverlayView {
  DCHECK(!_previewOverlay);
  _previewOverlay =
      [[PreviewOverlayView alloc] initWithFrame:CGRectZero
                                   viewportSize:[self viewportSize]];
  [self addSubview:_previewOverlay];

  // Add a multiplier of sqrt(2) to the width and height constraints to make
  // sure that the overlay covers the whole screen during rotation.
  _overlayWidthConstraint =
      [NSLayoutConstraint constraintWithItem:_previewOverlay
                                   attribute:NSLayoutAttributeWidth
                                   relatedBy:NSLayoutRelationGreaterThanOrEqual
                                      toItem:self
                                   attribute:NSLayoutAttributeWidth
                                  multiplier:sqrt(2)
                                    constant:0.0];

  _overlayHeightConstraint =
      [NSLayoutConstraint constraintWithItem:_previewOverlay
                                   attribute:NSLayoutAttributeHeight
                                   relatedBy:NSLayoutRelationGreaterThanOrEqual
                                      toItem:self
                                   attribute:NSLayoutAttributeHeight
                                  multiplier:sqrt(2)
                                    constant:0.0];

  _overlaySquareConstraint = [[_previewOverlay heightAnchor]
      constraintEqualToAnchor:[_previewOverlay widthAnchor]];

  // Constrains the preview overlay to be square, centered, with both width and
  // height greater than or equal to the width and height of the ScannerView.
  [_previewOverlay setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[_previewOverlay centerXAnchor]
        constraintEqualToAnchor:[self centerXAnchor]],
    [[_previewOverlay centerYAnchor]
        constraintEqualToAnchor:[self centerYAnchor]],
    _overlaySquareConstraint, _overlayWidthConstraint, _overlayHeightConstraint
  ]];
}

// Hides the UIScrollView that contains the caption if the UI's vertical size is
// compact.
- (void)maybeHideCaptions {
  _captionContainer.hidden =
      UIUserInterfaceSizeClassCompact == self.traitCollection.verticalSizeClass;
}

@end
