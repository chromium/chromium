// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_POINTER_RETURNER_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_POINTER_RETURNER_H_

class PointerReturner {
 public:
  PointerReturner();

  ~PointerReturner();

  bool Valid();
  int* IntegerPointer();
  PointerReturner* ObjectPointer();

 private:
  int integer_;
  PointerReturner* child_;
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_POINTER_RETURNER_H_
