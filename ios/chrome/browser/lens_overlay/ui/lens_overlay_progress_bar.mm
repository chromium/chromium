// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_progress_bar.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LensOverlayProgressBar

- (NSString*)accessibilityValue {
  return l10n_util::GetNSStringF(
      IDS_IOS_PROGRESS_BAR_ACCESSIBILITY,
      base::SysNSStringToUTF16([super accessibilityValue]));
}

@end
