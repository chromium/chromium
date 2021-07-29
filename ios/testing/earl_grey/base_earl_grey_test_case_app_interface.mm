// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BaseEarlGreyTestCaseAppInterface

+ (void)logMessage:(NSString*)message {
  DLOG(WARNING) << base::SysNSStringToUTF8(message);
}

@end
