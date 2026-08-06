// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Helper to create a CAGradientLayer for a given size, start/end points, and
// color palette.
CAGradientLayer* CreateGradientLayer(CGSize size,
                                     CGPoint startPoint,
                                     CGPoint endPoint,
                                     NSArray<UIColor*>* colors) {
  NSMutableArray<id>* gradientColorArray =
      [NSMutableArray arrayWithCapacity:colors.count];
  for (UIColor* color in colors) {
    [gradientColorArray addObject:static_cast<id>(color.CGColor)];
  }

  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.colors = gradientColorArray;
  gradientLayer.startPoint = startPoint;
  gradientLayer.endPoint = endPoint;
  gradientLayer.frame = CGRectMake(0, 0, size.width, size.height);
  return gradientLayer;
}

// Returns standard Gemini blue gradient colors.
NSArray<UIColor*>* GeminiBlueGradientColors() {
  UITraitCollection* lightTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  return @[
    [[UIColor colorNamed:kBlue700Color]
        resolvedColorWithTraitCollection:lightTraitCollection],
    [[UIColor colorNamed:kBlue300Color]
        resolvedColorWithTraitCollection:lightTraitCollection]
  ];
}

// Helper to create the standard Gemini blue gradient layer for a given size and
// start/end points.
CAGradientLayer* CreateGeminiBlueGradientLayer(CGSize size,
                                               CGPoint startPoint,
                                               CGPoint endPoint) {
  return CreateGradientLayer(size, startPoint, endPoint,
                             GeminiBlueGradientColors());
}

}  // namespace

@implementation GeminiUIUtils

+ (UIImage*)brandedGeminiSymbolWithPointSize:(CGFloat)pointSize {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SymbolWithPointSize(SymbolGeminiBrandedLogo, pointSize);
#else
  return SymbolWithPointSize(SymbolGeminiNonBrandedLogo, pointSize);
#endif
}

+ (UIImage*)createGradientGeminiLogo:(CGFloat)pointSize {
  UIImage* geminiIcon =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:pointSize];
  CGSize iconSize = [geminiIcon size];
  CGRect iconFrame = CGRectMake(0, 0, iconSize.width, iconSize.height);

  CAGradientLayer* gradientLayer = CreateGeminiBlueGradientLayer(
      iconSize, CGPointMake(0, 0.5), CGPointMake(0.5, 0.0));

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

+ (NSAttributedString*)attributedStringByReplacingWord:(NSString*)targetWord
                                                inText:(NSString*)text
                                                  font:(UIFont*)font
                                                colors:
                                                    (NSArray<UIColor*>*)colors {
  CHECK(text);
  CHECK(targetWord);
  CHECK(font);
  CHECK(colors);
  CHECK_GE(colors.count, 2u);
  CHECK_GT(targetWord.length, 0u);
  CHECK_GE(text.length, targetWord.length);

  NSDictionary* attributes = @{
    NSFontAttributeName : font,
  };

  NSDictionary* textAttributes = @{NSFontAttributeName : font};
  CGSize textSize = [targetWord sizeWithAttributes:textAttributes];

  // Create a gradient layer based on the exact size of the text with the given
  // font.
  CAGradientLayer* gradientLayer = CreateGradientLayer(
      textSize, CGPointMake(0.0, 0.5), CGPointMake(1.0, 0.5), colors);

  // Create a stencil mask for the target word using the font and text size.
  UIGraphicsImageRenderer* maskRenderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:textSize];
  UIImage* textMaskImage = [maskRenderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        [targetWord drawAtPoint:CGPointZero withAttributes:textAttributes];
      }];

  // Apply the gradient to the mask to create the gradient text effect.
  UIGraphicsImageRenderer* gradientRenderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:textSize];
  UIImage* gradientImage = [gradientRenderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        CGContextTranslateCTM(rendererContext.CGContext, 0, textSize.height);
        CGContextScaleCTM(rendererContext.CGContext, 1.0, -1.0);
        CGContextClipToMask(rendererContext.CGContext,
                            CGRectMake(0, 0, textSize.width, textSize.height),
                            textMaskImage.CGImage);
        [gradientLayer renderInContext:rendererContext.CGContext];
      }];

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:attributes];
  NSRange range = [attributedString.string rangeOfString:targetWord];
  // Replace target word in text with the gradient image.
  if (range.location != NSNotFound) {
    NSTextAttachment* textAttachment = [[NSTextAttachment alloc] init];
    textAttachment.image = gradientImage;
    textAttachment.bounds =
        CGRectMake(0, font.descender, textSize.width, textSize.height);
    NSAttributedString* attachmentString =
        [NSAttributedString attributedStringWithAttachment:textAttachment];
    [attributedString replaceCharactersInRange:range
                          withAttributedString:attachmentString];
  }
  return attributedString;
}

+ (NSAttributedString*)
    attributedStringWithGradientGeminiForTitle:(NSString*)title
                                          font:(UIFont*)font {
  if (!title || !font) {
    return nil;
  }
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  NSString* gradientSubstring =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_GRADIENT_TEXT);
  return [self attributedStringByReplacingWord:gradientSubstring
                                        inText:title
                                          font:font
                                        colors:GeminiBlueGradientColors()];
#else
  NSDictionary* attributes = @{
    NSFontAttributeName : font,
  };
  return [[NSAttributedString alloc] initWithString:title
                                         attributes:attributes];
#endif
}

+ (CGFloat)contentHeightForView:(UIView*)targetView
             withContainerWidth:(CGFloat)containerWidth {
  if (!targetView) {
    return 0;
  }
  if (containerWidth <= 0) {
    return
        [targetView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
            .height;
  }
  CGSize targetSize =
      CGSizeMake(containerWidth, UILayoutFittingCompressedSize.height);
  return
      [targetView systemLayoutSizeFittingSize:targetSize
                withHorizontalFittingPriority:UILayoutPriorityRequired
                      verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
}

@end
