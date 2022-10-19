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

  // As of Swift 5.8, C++ methods that return pointers are not imported by
  // default as they are an unsafe projection into the state of the object.
  // Using clang hints, the APIs can be annotated to import anyway. These hints
  // are backwards compatible to previous language versions.
  __attribute__((swift_attr("import_unsafe"))) int* IntegerPointer();
  __attribute__((swift_attr("import_unsafe"))) PointerReturner* ObjectPointer();

 private:
  int integer_;
  PointerReturner* child_;
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_POINTER_RETURNER_H_
