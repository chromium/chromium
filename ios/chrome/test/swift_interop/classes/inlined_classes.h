// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASSES_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASSES_H_

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

// Class will be imported as "RenamedClass" in swift due to apinotes.
class RenamedClassOriginal {
 public:
  RenamedClassOriginal() {}
  bool Check() const { return true; }
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLASSES_INLINED_CLASSES_H_
