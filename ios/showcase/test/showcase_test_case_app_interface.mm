// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_test_case_app_interface.h"

#import "base/mac/foundation_util.h"
#import "ios/showcase/core/app_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowcaseTestCaseAppInterface

+ (void)setupUI {
  AppDelegate* delegate = base::mac::ObjCCastStrict<AppDelegate>(
      [UIApplication sharedApplication].delegate);
  [delegate setupUI];
}

@end
