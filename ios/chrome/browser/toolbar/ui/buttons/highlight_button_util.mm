// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/highlight_button_util.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

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
  imageView.tintColor = [UIColor whiteColor];
  imageView.layer.shadowColor = [UIColor blackColor].CGColor;
  imageView.layer.shadowOpacity = 0.2;
  imageView.layer.shadowRadius = 1.5;
  imageView.layer.shadowOffset = CGSizeMake(0, 1);
}

void RemoveIPHImageStyleFromImageView(UIImageView* imageView) {
  if (!imageView) {
    return;
  }
  imageView.tintColor = [UIColor colorNamed:kSolidBlackColor];
  imageView.layer.shadowColor = nil;
  imageView.layer.shadowOpacity = 0;
  imageView.layer.shadowRadius = 0;
  imageView.layer.shadowOffset = CGSizeZero;
}
