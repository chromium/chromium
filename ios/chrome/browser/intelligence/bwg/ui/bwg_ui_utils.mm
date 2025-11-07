// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"

#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation BWGUIUtils

+ (UIImage*)brandedGeminiSymbolWithPointSize:(CGFloat)pointSize {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return CustomSymbolWithPointSize(kGeminiBrandedLogoImage, pointSize);
#else
  return DefaultSymbolWithPointSize(kGeminiNonBrandedLogoImage, pointSize);
#endif
}

+ (UIImage*)createGradientGeminiLogo:(CGFloat)pointSize {
  UITraitCollection* lightTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  NSArray<UIColor*>* colors = @[
    [[UIColor colorNamed:kBlue700Color]
        resolvedColorWithTraitCollection:lightTraitCollection],
    [[UIColor colorNamed:kBlue300Color]
        resolvedColorWithTraitCollection:lightTraitCollection]
  ];

  NSMutableArray<id>* gradientColorArray = [[NSMutableArray alloc] init];
  for (UIColor* color in colors) {
    [gradientColorArray addObject:static_cast<id>(color.CGColor)];
  }

  UIImage* geminiIcon = [BWGUIUtils brandedGeminiSymbolWithPointSize:pointSize];
  CGSize iconSize = [geminiIcon size];
  CGRect iconFrame = CGRectMake(0, 0, iconSize.width, iconSize.height);

  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.colors = gradientColorArray;
  gradientLayer.startPoint = CGPointMake(0, 0.5);
  gradientLayer.endPoint = CGPointMake(0.5, 0.0);
  gradientLayer.frame = iconFrame;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:iconSize];
  UIImage* gradientImage = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        CGContextClipToMask(rendererContext.CGContext, iconFrame,
                            geminiIcon.CGImage);
        [gradientLayer renderInContext:rendererContext.CGContext];
      }];

  return gradientImage;
}

@end
