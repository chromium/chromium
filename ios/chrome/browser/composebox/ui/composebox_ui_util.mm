// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"

#import "build/build_config.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

UIImage* GetBananaIcon(CGFloat size) {
  CGFloat iconPadding = 4.0;
  CGSize imageSize = CGSizeMake(size + iconPadding, size + iconPadding);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:imageSize];
  UIImage* image = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        CGRect rect = CGRectMake(0, 0, imageSize.width, imageSize.height);
        UIFont* font = [UIFont systemFontOfSize:size];
        NSDictionary* attributes = @{
          NSFontAttributeName : font,
          NSForegroundColorAttributeName : UIColor.blackColor
        };
        [@"🍌" drawInRect:rect withAttributes:attributes];
      }];

  return image;
}

UIImage* ImageForIconResourceId(omnibox::IconResourceIds icon_id,
                                CGFloat point_size) {
  switch (icon_id) {
    case omnibox::IconResourceIds::GLOBE:
      return SymbolWithPointSize(SymbolGlobe, point_size);
    case omnibox::IconResourceIds::SEARCH:
      return SymbolWithPointSize(SymbolSearch, point_size);
    case omnibox::IconResourceIds::COLORED_SEARCH_LOUPE_WITH_SPARKLE:
    case omnibox::IconResourceIds::SEARCH_LOUPE_WITH_SPARKLE:
      return SymbolWithPointSize(SymbolMagnifyingglassSpark, point_size);
    case omnibox::IconResourceIds::AUTORENEW:
      return SymbolWithPointSize(SymbolArrowTrianglehead2ClockwiseRotate90,
                                 point_size);
    case omnibox::IconResourceIds::BOLT:
      return SymbolWithPointSize(SymbolBolt, point_size);
    case omnibox::IconResourceIds::ATTACH_FILE:
      return SymbolWithPointSize(SymbolPaperclip, point_size);
    case omnibox::IconResourceIds::ADD_PHOTO_ALTERNATE:
      return SymbolWithPointSize(SymbolPhotoBadgePlus, point_size);
    case omnibox::IconResourceIds::TRAVEL_EXPLORE:
      return SymbolWithPointSize(SymbolDeepSearch, point_size);
    case omnibox::IconResourceIds::TASK_SPARK:
      return SymbolWithPointSize(SymbolTextSpark, point_size);
    case omnibox::IconResourceIds::DRAFT_SPARK:
      return SymbolWithPointSize(SymbolDocumentBadgeSpark, point_size);
    case omnibox::IconResourceIds::TIMER:
      return SymbolWithPointSize(SymbolClock, point_size);
    case omnibox::IconResourceIds::LENS_CAMERA:
      return SymbolWithPointSize(SymbolCameraLens, point_size);
    case omnibox::IconResourceIds::BANANA:
      return GetBananaIcon(point_size);
    case omnibox::IconResourceIds::CHECK_SMALL:
      return SymbolWithPointSize(SymbolCheckmark, point_size);
    case omnibox::IconResourceIds::PHOTO_PRINTS:
      return SymbolWithPointSize(SymbolPhoto, point_size);
    case omnibox::IconResourceIds::DRIVE:
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      return SymbolWithPointSize(SymbolGoogleDrive, point_size);
#else
      return SymbolWithPointSize(SymbolFolder, point_size);
#endif
    case omnibox::IconResourceIds::PLACE_WHITE:
    default:
      return nil;
  }
}
