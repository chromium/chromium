// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASS_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASS_H_

class InlinedClass {
 private:
  int x_;

 public:
  InlinedClass() { x_ = 0; }
  int AddTo(int delta) {
    x_ += delta;
    return x_;
  }
};

class ComposedClass {
 private:
  InlinedClass first_;

 public:
  int Increment(int delta) {
    int result = first_.AddTo(delta);
    return result;
  }
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASS_H_
