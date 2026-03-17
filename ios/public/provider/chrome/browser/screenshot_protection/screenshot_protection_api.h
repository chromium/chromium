// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SCREENSHOT_PROTECTION_SCREENSHOT_PROTECTION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SCREENSHOT_PROTECTION_SCREENSHOT_PROTECTION_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Options for content obfuscation.
struct ScreenshotProtectionOptions {
  // If true, the view's content will be hidden in system screenshots.
  bool obfuscate_screenshots = false;

  // If true, the view's content will be hidden during system
  // screen capturing
  bool obfuscate_screen_recordings = false;
};

// Applies protection to `view` based on the provided `options`.
void SetScreenshotProtection(UIView* view, ScreenshotProtectionOptions options);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SCREENSHOT_PROTECTION_SCREENSHOT_PROTECTION_API_H_
