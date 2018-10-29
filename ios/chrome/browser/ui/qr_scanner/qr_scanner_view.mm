// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/numerics/math_constants.h"
#include "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding for buttons in the QR scanner UI.
const CGFloat kButtonPadding = 16.0;

// Width and height of the QR scanner viewport.
const CGFloat kViewportSize_iPhone = 250.0;
const CGFloat kViewportSize_iPad = 300.0;

// Length of the viewport borders, starting from the corner.
const CGFloat kViewportBorderCornerWidth_iPhone = 25.0;
const CGFloat kViewportBorderCornerWidth_iPad = 30.0;

// Opacity of the preview overlay.
const CGFloat kPreviewOverlayOpacity = 0.5;

// Corner radius of the border around the viewport.
const CGFloat kViewportBorderCornerRadius = 2.0;
// Line width of the viewport border.
const CGFloat kViewportBorderLineWidth = 4.0;
// Shadow opacity of the viewport border.
const CGFloat kViewportBorderShadowOpacity = 1.0;
// Shadow radius of the viewport border.
const CGFloat kViewportBorderShadowRadius = 10.0;
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

// Returns a square of size |rectSize| centered inside |frameSize|.
CGRect CenteredRectForViewport(CGSize frameSize, CGFloat rectSize) {
  CGFloat rectX = AlignValueToPixel((frameSize.width - rectSize) / 2);
  CGFloat rectY = AlignValueToPixel((frameSize.height - rectSize) / 2);
  return CGRectMake(rectX, rectY, rectSize, rectSize);
}

// Returns the size of the viewport based on the device type.
CGFloat GetViewportSize() {
  return IsIPadIdiom() ? kViewportSize_iPad : kViewportSize_iPhone;
}

}  // namespace

// A subclass of UIView with the layerClass property set to
// AVCaptureVideoPreviewLayer. Contains the video preview for the QR scanner.
@interface VideoPreviewView : UIView

// Returns the VideoPreviewView's layer cast to AVCaptureVideoPreviewLayer.
- (AVCaptureVideoPreviewLayer*)previewLayer;

// Returns the rectangle in camera coordinates in which codes should be
// recognized.
- (CGRect)viewportRectOfInterest;

@end

@implementation VideoPreviewView

+ (Class)layerClass {
  return [AVCaptureVideoPreviewLayer class];
}

- (AVCaptureVideoPreviewLayer*)previewLayer {
  return base::mac::ObjCCastStrict<AVCaptureVideoPreviewLayer>([self layer]);
}

- (CGRect)viewportRectOfInterest {
  DCHECK(CGPointEqualToPoint(self.frame.origin, CGPointZero));
  CGRect viewportRect =
      CenteredRectForViewport(self.frame.size, GetViewportSize());
  AVCaptureVideoPreviewLayer* layer = [self previewLayer];
  // If the layer does not have a connection,
  // |metadataOutputRectOfInterestForRect:| does not return the right value.
  DCHECK(layer.connection);
  return [layer metadataOutputRectOfInterestForRect:viewportRect];
}

@end

// A subclass of UIView containing the preview overlay. It is responsible for
// redrawing the preview overlay and the viewport border every time the size
// of the preview changes. This UIView should always be square, with its width
// and height being the maximum of the width and height of its parent.
@interface PreviewOverlayView : UIView {
  // Creates a transparent preview overlay. The overlay is a sublayer of the
  // PreviewOverlayView's view to keep the opacity of the view's layer 1.0,
  // otherwise the viewport border would inherit the opacity of the overlay.
  CALayer* _previewOverlay;
  // A container for the viewport border to draw a shadow under the border.
  // Sublayer of PreviewOverlayView's layer.
  CALayer* _viewportBorderContainer;
  // The preview viewport border. Sublayer of |_viewportBorderContainer|.
  CAShapeLayer* _viewportBorder;
}

// Creates a square mask for the overlay to keep the viewport transparent.
- (CAShapeLayer*)getViewportMaskWithFrameSize:(CGSize)frameSize
                                 viewportSize:(CGFloat)viewportSize;
// Creates a mask to only draw the corners of the viewport border.
- (CAShapeLayer*)getViewportBorderMaskWithFrameSize:(CGSize)frameSize
                                       viewportSize:(CGFloat)viewportSize;

@end

@implementation PreviewOverlayView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }

  _previewOverlay = [[CALayer alloc] init];
  [_previewOverlay setBackgroundColor:[[UIColor blackColor] CGColor]];
  [_previewOverlay setOpacity:kPreviewOverlayOpacity];
  [[self layer] addSublayer:_previewOverlay];

  _viewportBorderContainer = [[CALayer alloc] init];
  [_viewportBorderContainer setShadowColor:[[UIColor blackColor] CGColor]];
  [_viewportBorderContainer setShadowOffset:CGSizeZero];
  [_viewportBorderContainer setShadowRadius:kViewportBorderShadowRadius];
  [_viewportBorderContainer setShadowOpacity:kViewportBorderShadowOpacity];
  [_viewportBorderContainer setShouldRasterize:YES];
  [_viewportBorderContainer
      setRasterizationScale:[[UIScreen mainScreen] scale]];

  _viewportBorder = [[CAShapeLayer alloc] init];
  [_viewportBorder setStrokeColor:[[UIColor whiteColor] CGColor]];
  [_viewportBorder setFillColor:nil];
  [_viewportBorder setOpacity:1.0];
  [_viewportBorder setLineWidth:kViewportBorderLineWidth];
  [_viewportBorderContainer addSublayer:_viewportBorder];

  [[self layer] addSublayer:_viewportBorderContainer];
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize frameSize = self.frame.size;
  CGFloat viewportSize = GetViewportSize();
  [_previewOverlay
      setFrame:CGRectMake(0, 0, frameSize.width, frameSize.height)];
  [_previewOverlay setMask:[self getViewportMaskWithFrameSize:frameSize
                                                 viewportSize:viewportSize]];

  CGRect borderRect = CenteredRectForViewport(
      frameSize, viewportSize + kViewportBorderLineWidth);
  UIBezierPath* borderPath =
      [UIBezierPath bezierPathWithRoundedRect:borderRect
                                 cornerRadius:kViewportBorderCornerRadius];

  [_viewportBorder setPath:[borderPath CGPath]];
  [_viewportBorder
      setMask:[self getViewportBorderMaskWithFrameSize:frameSize
                                          viewportSize:viewportSize]];
}

- (CAShapeLayer*)getViewportMaskWithFrameSize:(CGSize)frameSize
                                 viewportSize:(CGFloat)viewportSize {
  CGRect frameRect = CGRectMake(0, 0, frameSize.width, frameSize.height);
  CGRect viewportRect = CenteredRectForViewport(frameSize, viewportSize);
  UIBezierPath* maskPath = [UIBezierPath bezierPathWithRect:frameRect];
  [maskPath appendPath:[UIBezierPath bezierPathWithRect:viewportRect]];

  CAShapeLayer* mask = [[CAShapeLayer alloc] init];
  [mask setFillColor:[[UIColor blackColor] CGColor]];
  [mask setFillRule:kCAFillRuleEvenOdd];
  [mask setFrame:frameRect];
  [mask setPath:maskPath.CGPath];
  return mask;
}

- (CAShapeLayer*)getViewportBorderMaskWithFrameSize:(CGSize)frameSize
                                       viewportSize:(CGFloat)viewportSize {
  CGFloat viewportBorderCornerWidth = IsIPadIdiom()
                                          ? kViewportBorderCornerWidth_iPad
                                          : kViewportBorderCornerWidth_iPhone;
  CGRect maskRect = CenteredRectForViewport(
      frameSize, viewportSize - 2 * viewportBorderCornerWidth);
  CGFloat sizeX = maskRect.origin.x;
  CGFloat sizeY = maskRect.origin.y;
  CGFloat offsetX = sizeX + maskRect.size.width;
  CGFloat offsetY = sizeY + maskRect.size.height;

  UIBezierPath* path =
      [UIBezierPath bezierPathWithRect:CGRectMake(0, 0, sizeX, sizeY)];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(0, offsetY,
                                                               sizeX, sizeY)]];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(offsetY, 0,
                                                               sizeX, sizeY)]];
  [path appendPath:[UIBezierPath bezierPathWithRect:CGRectMake(offsetX, offsetY,
                                                               sizeX, sizeY)]];

  CAShapeLayer* mask = [[CAShapeLayer alloc] init];
  [mask setFillColor:[[UIColor blackColor] CGColor]];
  [mask setFrame:CGRectMake(0, 0, frameSize.width, frameSize.height)];
  [mask setPath:path.CGPath];
  return mask;
}

@end

@interface QRScannerView () {
  // A button to toggle the torch.
  MDCFlatButton* _torchButton;
  // A view containing the preview layer for camera input.
  VideoPreviewView* _previewView;
  // A transparent overlay on top of the preview layer.
  PreviewOverlayView* _previewOverlay;
  // The constraint specifying that the preview overlay should be square.
  NSLayoutConstraint* _overlaySquareConstraint;
  // The constraint relating the size of the |_previewOverlay| to the width of
  // the QRScannerView.
  NSLayoutConstraint* _overlayWidthConstraint;
  // The constraint relating the size of the |_previewOverlay| to the height of
  // te QRScannerView.
  NSLayoutConstraint* _overlayHeightConstraint;
}

// Creates an image with template rendering mode for use in icons.
- (UIImage*)templateImageWithName:(NSString*)name;
// Creates an icon for torch turned on.
- (UIImage*)torchOnIcon;
// Creates an icon for torch turned off.
- (UIImage*)torchOffIcon;

// Sets common configuration properties of a button in the QR scanner UI and
// adds it to self.view.
- (void)configureButton:(MDCFlatButton*)button
               withIcon:(UIImage*)icon
                 action:(SEL)action;
// Adds a close button.
- (void)addCloseButton;
// Adds a torch button and stores it in |_torchButton|.
- (void)addTorchButton;
// Adds a caption to the viewport.
- (void)addViewportCaptionLabel;
// Adds a preview view to |self| and configures its layout constraints.
- (void)setupPreviewView;
// Adds a transparent overlay with a viewport border to |self| and configures
// its layout constraints.
- (void)setupPreviewOverlayView;

@end

@implementation QRScannerView

@synthesize delegate = _delegate;

#pragma mark lifecycle

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<QRScannerViewDelegate>)delegate {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }
  DCHECK(delegate);
  _delegate = delegate;
  [self setupPreviewView];
  [self setupPreviewOverlayView];
  [self addCloseButton];
  [self addTorchButton];
  [self addViewportCaptionLabel];
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  NOTREACHED();
  return nil;
}

#pragma mark UIView

// TODO(crbug.com/633577): Replace the preview overlay with a UIView which is
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

#pragma mark public methods

- (AVCaptureVideoPreviewLayer*)getPreviewLayer {
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
        l10n_util::GetNSString(IDS_IOS_QR_SCANNER_TORCH_ON_ACCESSIBILITY_VALUE);
  } else {
    icon = [self torchOffIcon];
    accessibilityValue = l10n_util::GetNSString(
        IDS_IOS_QR_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE);
  }
  [_torchButton setImage:icon forState:UIControlStateNormal];
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
         fabs(fabs(atan2f(rotation.b, rotation.a)) - base::kPiFloat) < 0.001 ||
         fabs(fabs(atan2f(rotation.b, rotation.a)) - base::kPiFloat / 2) <
             0.001);
  rotation.a = round(rotation.a);
  rotation.b = round(rotation.b);
  rotation.c = round(rotation.c);
  rotation.d = round(rotation.d);
  [_previewView setTransform:rotation];
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

#pragma mark private methods

- (UIImage*)templateImageWithName:(NSString*)name {
  UIImage* image = [[UIImage imageNamed:name]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  DCHECK(image);
  return image;
}

- (UIImage*)torchOnIcon {
  UIImage* icon = [self templateImageWithName:@"qr_scanner_torch_on"];
  return icon;
}

- (UIImage*)torchOffIcon {
  UIImage* icon = [self templateImageWithName:@"qr_scanner_torch_off"];
  return icon;
}

- (void)configureButton:(MDCFlatButton*)button
               withIcon:(UIImage*)icon
                 action:(SEL)action {
  [button setTintColor:[UIColor whiteColor]];
  [button setImage:icon forState:UIControlStateNormal];
  [button setInkStyle:MDCInkStyleUnbounded];
  [button addTarget:_delegate
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:button];
}

- (void)addCloseButton {
  MDCFlatButton* closeButton = [[MDCFlatButton alloc] initWithFrame:CGRectZero];
  UIImage* closeIcon = [ChromeIcon closeIcon];
  UIImage* closeButtonIcon =
      [closeIcon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [closeButton setAccessibilityLabel:[closeIcon accessibilityLabel]];
  [closeButton setAccessibilityIdentifier:[closeIcon accessibilityIdentifier]];
  [self configureButton:closeButton
               withIcon:closeButtonIcon
                 action:@selector(dismissQRScannerView:)];

  // Constraints for closeButton.
  [closeButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[closeButton leadingAnchor] constraintEqualToAnchor:[self leadingAnchor]
                                                constant:kButtonPadding],
    [[closeButton bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]
                                               constant:-kButtonPadding]
  ]];
}

- (void)addTorchButton {
  DCHECK(!_torchButton);
  _torchButton = [[MDCFlatButton alloc] initWithFrame:CGRectZero];
  [_torchButton setEnabled:NO];
  [self configureButton:_torchButton
               withIcon:[self torchOffIcon]
                 action:@selector(toggleTorch:)];
  [_torchButton setAccessibilityIdentifier:@"qr_scanner_torch_button"];
  [_torchButton setAccessibilityLabel:
                    l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_BUTTON_ACCESSIBILITY_LABEL)];
  [_torchButton setAccessibilityValue:
                    l10n_util::GetNSString(
                        IDS_IOS_QR_SCANNER_TORCH_OFF_ACCESSIBILITY_VALUE)];

  // Constraints for _torchButton.
  [_torchButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[_torchButton trailingAnchor] constraintEqualToAnchor:[self trailingAnchor]
                                                  constant:-kButtonPadding],
    [[_torchButton bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]
                                                constant:-kButtonPadding]
  ]];
}

- (void)addViewportCaptionLabel {
  UILabel* viewportCaption = [[UILabel alloc] init];
  NSString* label = l10n_util::GetNSString(IDS_IOS_QR_SCANNER_VIEWPORT_CAPTION);
  [viewportCaption setText:label];
  [viewportCaption
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
  [viewportCaption setNumberOfLines:0];
  [viewportCaption setTextAlignment:NSTextAlignmentCenter];
  [viewportCaption setAccessibilityLabel:label];
  [viewportCaption setAccessibilityIdentifier:@"qr_scanner_viewport_caption"];
  [viewportCaption setTextColor:[UIColor whiteColor]];
  [viewportCaption.layer setShadowColor:[UIColor blackColor].CGColor];
  [viewportCaption.layer setShadowOffset:CGSizeZero];
  [viewportCaption.layer setShadowRadius:kViewportCaptionShadowRadius];
  [viewportCaption.layer setShadowOpacity:kViewportCaptionShadowOpacity];
  [viewportCaption.layer setMasksToBounds:NO];
  [viewportCaption.layer setShouldRasterize:YES];
  [self addSubview:viewportCaption];

  // Constraints for viewportCaption.
  [viewportCaption setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[viewportCaption topAnchor]
        constraintEqualToAnchor:[self centerYAnchor]
                       constant:GetViewportSize() / 2 +
                                kViewportCaptionVerticalPadding],
    [viewportCaption.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kViewportCaptionHorizontalPadding],
    [viewportCaption.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kViewportCaptionHorizontalPadding],
  ]];
}

- (void)setupPreviewView {
  DCHECK(!_previewView);
  _previewView = [[VideoPreviewView alloc] initWithFrame:self.frame];
  [self insertSubview:_previewView atIndex:0];
}

- (void)setupPreviewOverlayView {
  DCHECK(!_previewOverlay);
  _previewOverlay = [[PreviewOverlayView alloc] initWithFrame:CGRectZero];
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
  // height greater than or equal to the width and height of the QRScannerView.
  [_previewOverlay setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[_previewOverlay centerXAnchor]
        constraintEqualToAnchor:[self centerXAnchor]],
    [[_previewOverlay centerYAnchor]
        constraintEqualToAnchor:[self centerYAnchor]],
    _overlaySquareConstraint, _overlayWidthConstraint, _overlayHeightConstraint
  ]];
}

@end
