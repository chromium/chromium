// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/drop_and_navigate_interaction.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using DropAndNavigateTest = PlatformTest;

TEST_F(DropAndNavigateTest, Instantiation) {
  if (@available(iOS 11, *)) {
    DropAndNavigateInteraction* interaction =
        [[DropAndNavigateInteraction alloc] initWithDelegate:nil];
    DCHECK(interaction.delegate);
  }
}

}  // namespace
