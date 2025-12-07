// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_thumbnail_button.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"

namespace {
/// Corner radius of the thumbnail image.
const CGFloat kThumbnailImageCornerRadius = 12;
/// The duration of the transition for the thumbnail button.
const CGFloat kThumbnailButtonTransitionDuration = 0.25f;
}  // namespace

@implementation OmniboxThumbnailButton {
  /// Thumbnail image used in image search.
  UIImage* _thumbnailImage;

  /// Last view size. Used for resizing and reaplying the thumbnail image when
  /// the view frame changes.
  CGSize _lastSize;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = UIColor.clearColor;
    self.layer.cornerRadius = kThumbnailImageCornerRadius;
    self.userInteractionEnabled = YES;
    self.clipsToBounds = YES;
  }

  return self;
}

- (UIImage*)resizeImageToFillFrame:(UIImage*)thumbnailImage {
  if (!thumbnailImage) {
    return nil;
  }

  return ResizeImage(thumbnailImage,
                     CGSizeMake(self.frame.size.width, self.frame.size.height),
                     ProjectionMode::kAspectFill);
}

- (UIImage*)thumbnailImageWithDiscardIntentOverlay:(UIImage*)thumbnailImage {
  CGFloat imageWidth = self.frame.size.width;
  CGFloat imageHeight = self.frame.size.height;

  UIGraphicsImageRendererFormat* format =
      [[UIGraphicsImageRendererFormat alloc] init];
  format.opaque = YES;
  format.scale = 0.0;

  UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
      initWithSize:CGSizeMake(imageWidth, imageHeight)
            format:format];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    // Thumbnail image.
    [thumbnailImage drawAtPoint:CGPointZero];

    // Blue overlay.
    [[UIColor.systemBlueColor colorWithAlphaComponent:0.5] setFill];
    [context fillRect:renderer.format.bounds blendMode:kCGBlendModeNormal];

    // X Symbol.
    UIImage* xSymbolImage = SymbolWithPalette(
        DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize),
        @[ [UIColor whiteColor] ]);
    [xSymbolImage
        drawInRect:CGRectMake((imageWidth - xSymbolImage.size.width) / 2,
                              (imageHeight - xSymbolImage.size.height) / 2,
                              xSymbolImage.size.width,
                              xSymbolImage.size.height)];
  }];
}

- (BOOL)isHighlighted {
  // The highlighted state is not needed as the transition between normal and
  // selected is handled by a custom transition.
  return NO;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGSize frameSize = self.frame.size;
  if (frameSize.width == _lastSize.width &&
      frameSize.height == _lastSize.height) {
    return;
  }

  // If the frame's size or layout constraints are adjusted, the displayed
  // thumbnail image should be resized proportionally to avoid cropping.
  _lastSize = frameSize;
  UIImage* resizedImage = [self resizeImageToFillFrame:_thumbnailImage];
  UIImage* imageToSet =
      self.isSelected
          ? [self thumbnailImageWithDiscardIntentOverlay:resizedImage]
          : resizedImage;

  [self applyThumbnailImage:imageToSet animated:NO];
}

- (void)setThumbnailImage:(UIImage*)image {
  _thumbnailImage = image;
  UIImage* resizedImage = [self resizeImageToFillFrame:_thumbnailImage];
  [self applyThumbnailImage:resizedImage animated:NO];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];

  UIImage* resizedImage = [self resizeImageToFillFrame:_thumbnailImage];
  UIImage* imageToSet =
      selected ? [self thumbnailImageWithDiscardIntentOverlay:resizedImage]
               : resizedImage;

  [self applyThumbnailImage:imageToSet animated:YES];
}

- (void)applyThumbnailImage:(UIImage*)thumbnailImage animated:(BOOL)animated {
  if (!thumbnailImage) {
    return;
  }

  if (!animated) {
    [self setBackgroundImage:thumbnailImage forState:UIControlStateNormal];
    return;
  }

  [UIView transitionWithView:self
                    duration:kThumbnailButtonTransitionDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve |
                             UIViewAnimationOptionAllowAnimatedContent
                  animations:^{
                    [self setBackgroundImage:thumbnailImage
                                    forState:UIControlStateNormal];
                  }
                  completion:nil];
}

@end
