// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ObjCInteropTestCase : XCTestCase
@end

@implementation ObjCInteropTestCase

- (void)testEmpty {
  LOG(INFO) << "This is a dependency on //base";
}

@end
