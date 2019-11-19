// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/scoped_block_swizzler.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ScopedBlockSwizzler::ScopedBlockSwizzler(Class target, SEL selector, id block) {
  method_ = class_getInstanceMethod(target, selector);
  if (!method_) {
    // Try swizzling a class method instead.
    method_ = class_getClassMethod(target, selector);
  }
  DCHECK(method_);

  IMP block_imp = imp_implementationWithBlock(block);
  original_imp_ = method_setImplementation(method_, block_imp);
}

ScopedBlockSwizzler::~ScopedBlockSwizzler() {
  IMP block_imp = method_setImplementation(method_, original_imp_);
  DCHECK(imp_removeBlock(block_imp));
}
