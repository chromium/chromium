// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_SCOPED_METHOD_SWIZZLER_H_
#define IOS_TESTING_SCOPED_METHOD_SWIZZLER_H_

#include <objc/runtime.h>

class ScopedMethodSwizzler {
 public:
  // Constructs a new ScopedMethodSwizzler object and replaces the
  // implementation of |selector_to_replace| on the |target| class with the
  // given |replacing_selector|. ScopedMethodSwizzler swizzles instance methods.
  // |selector_to_replace| has to be implemented on the class.
  ScopedMethodSwizzler(Class target,
                       SEL selector_to_replace,
                       SEL replacing_selector);

  ScopedMethodSwizzler(const ScopedMethodSwizzler&) = delete;
  ScopedMethodSwizzler& operator=(const ScopedMethodSwizzler&) = delete;

  // Destroys the ScopedMethodSwizzler object, removing the swizzled method and
  // reinstalling the original method implementation.
  virtual ~ScopedMethodSwizzler();

 private:
  // The method that is to be swizzled.
  Method original_method_;

  // The method that replaces the original method.
  Method replacing_method_;
};

#endif  // IOS_TESTING_SCOPED_METHOD_SWIZZLER_H_
