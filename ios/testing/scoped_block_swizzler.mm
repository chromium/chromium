// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/scoped_block_swizzler.h"

#import "base/check.h"

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

ScopedBlockSwizzler::ScopedBlockSwizzler(Class target,
                                         SEL selector,
                                         id block,
                                         BOOL class_method) {
  if (class_method) {
    method_ = class_getClassMethod(target, selector);
  } else {
    method_ = class_getInstanceMethod(target, selector);
  }
  DCHECK(method_);

  IMP block_imp = imp_implementationWithBlock(block);
  original_imp_ = method_setImplementation(method_, block_imp);
}

ScopedBlockSwizzler::~ScopedBlockSwizzler() {
  IMP block_imp = method_setImplementation(method_, original_imp_);
  DCHECK(imp_removeBlock(block_imp));
}
