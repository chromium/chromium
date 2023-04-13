// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_test.h"

// Note that this uses the direct runtime interface to the autorelease pool.
// https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
// This is so this can work when compiled for ARC.

extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

PlatformTest::PlatformTest() : autorelease_pool_(objc_autoreleasePoolPush()) {}

PlatformTest::~PlatformTest() {
  objc_autoreleasePoolPop(autorelease_pool_);
}
