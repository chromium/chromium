// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/widget_kit_extension/crash_helper.h"

#import "ios/chrome/common/crash_report/crash_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CrashHelper

+ (void)configure {
  if (self == [CrashHelper self]) {
    crash_helper::common::StartCrashpad();
  }
}

@end
