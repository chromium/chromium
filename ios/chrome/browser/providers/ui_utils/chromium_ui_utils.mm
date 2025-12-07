// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>

#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

namespace ios {
namespace provider {

void InitializeUI() {
  // Chromium does not have global UI state.
}

UIImageView* CreateAnimatedImageView() {
  return [[UIImageView alloc] init];
}

UIImage* CreateAnimatedImageFromData(NSData* data) {
  return nil;
}

void HideModalViewStack() {
  // Chromium provider does not present modals.
}

void LogIfModalViewsArePresented() {
  // Chromium provider does not present modals.
}

}  // namespace provider
}  // namespace ios
