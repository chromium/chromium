// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

#import <CoreGraphics/CoreGraphics.h>

namespace ios {
namespace provider {

void InitializeUI() {
  // Tests do not have global UI state.
}

id<LogoVendor> CreateLogoVendor(Browser* browser, web::WebState* web_state) {
  // Tests do not use LogoVendor.
  return nil;
}

void HideModalViewStack() {
  // Test provider does not present modals.
}

void LogIfModalViewsArePresented() {
  // Test provider does not present modals.
}

}  // namespace provider
}  // namespace ios
