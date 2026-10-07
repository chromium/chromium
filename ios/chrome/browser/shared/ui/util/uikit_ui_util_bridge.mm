// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util_bridge.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@implementation UiKitUtils

+ (UIImage*)greyImage:(UIImage*)image {
  return GreyImage(image);
}

+ (size_t)memoryFootprintForImage:(UIImage*)image {
  return MemoryFootprintForImage(image);
}

+ (BOOL)isChromeNextIaEnabled {
  return IsChromeNextIaEnabled();
}

@end
