// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_test_case_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/showcase/core/app_delegate.h"

@implementation ShowcaseTestCaseAppInterface

+ (void)setupUI {
  AppDelegate* delegate = base::apple::ObjCCastStrict<AppDelegate>(
      [UIApplication sharedApplication].delegate);
  [delegate setupUI];
}

@end
