// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_test.h"

#import <Foundation/Foundation.h>

PlatformTest::PlatformTest()
    : pool_([[NSAutoreleasePool alloc] init]) {
}

PlatformTest::~PlatformTest() {
  [pool_ release];
}
