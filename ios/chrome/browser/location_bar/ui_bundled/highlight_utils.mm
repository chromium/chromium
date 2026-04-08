// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/highlight_utils.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// The tint color for the image in IPH.
UIColor* IPHHighlightedTintColor() {
  return [UIColor whiteColor];
}

// Applies a shadow to an image view for IPH.
void SetShadowForIPHImage(UIImageView* imageView) {
  CALayer* layer = imageView.layer;
  layer.shadowColor = [UIColor blackColor].CGColor;
  layer.shadowOpacity = 0.2;
  layer.shadowRadius = 1.5;
  layer.shadowOffset = CGSizeMake(0, 1);
}

// Removes the shadow from an image view for IPH.
void RemoveShadowFromIPHImage(UIImageView* imageView) {
  CALayer* layer = imageView.layer;
  layer.shadowColor = nil;
  layer.shadowOpacity = 0;
  layer.shadowRadius = 0;
  layer.shadowOffset = CGSizeZero;
}
}  // namespace

UIView* CreateIPHGradientView() {
  UIView* view = [[GradientView alloc]
      initWithStartColor:[UIColor colorNamed:kStaticBlue400Color]
                endColor:[UIColor colorNamed:kStaticBlue600Color]
              startPoint:CGPointMake(1, 0)
                endPoint:CGPointMake(0, 1)
            gradientType:GradientLayerType::kLinear];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

void ConfigureIPHImageStyleForImageView(UIImageView* imageView) {
  if (!imageView) {
    return;
  }
  imageView.tintColor = IPHHighlightedTintColor();
  SetShadowForIPHImage(imageView);
}

void ConfigureIPHImageStyleForButton(UIButton* button) {
  if (!button) {
    return;
  }
  button.tintColor = IPHHighlightedTintColor();
  SetShadowForIPHImage(button.imageView);
}

void RemoveIPHImageStyleFromImageView(UIImageView* imageView) {
  if (!imageView) {
    return;
  }
  imageView.tintColor = nil;
  RemoveShadowFromIPHImage(imageView);
}

void RemoveIPHImageStyleFromButton(UIButton* button) {
  if (!button) {
    return;
  }
  button.tintColor = nil;
  RemoveShadowFromIPHImage(button.imageView);
}
