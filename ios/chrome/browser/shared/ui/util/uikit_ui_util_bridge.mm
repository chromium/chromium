// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util_bridge.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@implementation UiKitUtils

+ (UIImage*)greyImage:(UIImage*)image {
  return GreyImage(image);
}

@end
