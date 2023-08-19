// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"

#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

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
