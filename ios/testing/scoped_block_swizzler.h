// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef IOS_TESTING_SCOPED_BLOCK_SWIZZLER_H_
#define IOS_TESTING_SCOPED_BLOCK_SWIZZLER_H_

#include <objc/runtime.h>

// Helper class that replaces a method implementation with a given block.
// ScopedBlockSwizzler automatically swizzles when it is constructed and
// reinstalls the original method implementation when it goes out of scope.
class ScopedBlockSwizzler {
 public:
  // Constructs a new ScopedBlockSwizzler object and replaces the implementation
  // of |selector| on the |target| class with the given |block|.
  // ScopedBlockSwizzler first tries to swizzle a class method; if one is not
  // found, it tries to swizzle an instance method.  It is an error to pass a
  // |selector| that does not exist on the |target| class.
  ScopedBlockSwizzler(Class target, SEL selector, id block);

  // Constructs a new ScopedBlockSwizzler object and replaces the implementation
  // of |selector| on the |target| class with the given |block|.
  // |class_method| specifies if the swizzled method is a class method (YES) or
  // an instance method (NO). This can be use to disambiguate if the class
  // contains both method types with the same selector.
  ScopedBlockSwizzler(Class target, SEL selector, id block, BOOL class_method);

  ScopedBlockSwizzler(const ScopedBlockSwizzler&) = delete;
  ScopedBlockSwizzler& operator=(const ScopedBlockSwizzler&) = delete;

  // Destroys the ScopedBlockSwizzler object, removing the swizzled method and
  // reinstalling the original method implementation.
  virtual ~ScopedBlockSwizzler();

 private:
  // The method that is to be swizzled.  Can be either a class method or an
  // instance method.
  Method method_;

  // The original implementation of the swizzled method, saved so that it can be
  // reinstalled when this object goes out of scope.
  IMP original_imp_;
};

#endif  // IOS_TESTING_SCOPED_BLOCK_SWIZZLER_H_
