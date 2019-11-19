// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_H_
#define IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"

// Helper class that wraps ScopedBlockSwizzler for use in EG1 and EG2 tests.
class EarlGreyScopedBlockSwizzler {
 public:
  // Constructs a new ScopedBlockSwizzler via the
  // EarlGreyScopedBlockSwizzlerAppInterface interface.
  EarlGreyScopedBlockSwizzler(NSString* target, NSString* selector, id block);

  // Destroys the ScopedBlockSwizzler object via the
  // EarlGreyScopedBlockSwizzlerAppInterface interface.
  virtual ~EarlGreyScopedBlockSwizzler();

 private:
  // id used to track creation and destruction of swizzled block.
  int unique_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(EarlGreyScopedBlockSwizzler);
};

#endif  // IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_H_
