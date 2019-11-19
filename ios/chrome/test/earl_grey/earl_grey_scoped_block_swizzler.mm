// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"

#include "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(EarlGreyScopedBlockSwizzlerAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

EarlGreyScopedBlockSwizzler::EarlGreyScopedBlockSwizzler(NSString* target,
                                                         NSString* selector,
                                                         id block)
    : unique_id_([EarlGreyScopedBlockSwizzlerAppInterface
          createScopedBlockSwizzlerForTarget:target
                                withSelector:selector
                                   withBlock:block]) {}

EarlGreyScopedBlockSwizzler::~EarlGreyScopedBlockSwizzler() {
  [EarlGreyScopedBlockSwizzlerAppInterface
      deleteScopedBlockSwizzlerForID:unique_id_];
}
