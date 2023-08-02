// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/widget_kit_extension/crash_helper.h"

#import "ios/chrome/common/crash_report/crash_helper.h"

@implementation CrashHelper

+ (void)configure {
  if (self == [CrashHelper self]) {
    crash_helper::common::StartCrashpad();
  }
}

@end
