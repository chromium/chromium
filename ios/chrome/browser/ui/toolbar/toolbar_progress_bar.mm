// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarProgressBar

- (NSString*)accessibilityValue {
  return l10n_util::GetNSStringF(
      IDS_IOS_PROGRESS_BAR_ACCESSIBILITY,
      base::SysNSStringToUTF16([super accessibilityValue]));
}

@end
