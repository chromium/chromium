// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/screenshot_protection/screenshot_protection_api.h"

namespace ios {
namespace provider {

void SetScreenshotProtection(UIView* view,
                             ScreenshotProtectionOptions options) {
  // The test provider does nothing by default to avoid side effects on the test
  // runner.
}

}  // namespace provider
}  // namespace ios
