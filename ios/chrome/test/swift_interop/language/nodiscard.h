// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_

[[nodiscard]] int NoDiscardAdd(int x, int y) {
  return x + y;
}

class NoDiscardMultiply {
 public:
  NoDiscardMultiply() {}
  ~NoDiscardMultiply() {}

  [[nodiscard]] int Multiply(int x, int y) { return x * y; }

  int Divide(int x, int y) { return x / y; }
};

struct [[nodiscard]] NoDiscardError {
 public:
  NoDiscardError(int value) : value_(value) {}
  int value_;
};

NoDiscardError NoDiscardReturnError(int x, int y) {
  auto z = x + y;
  NoDiscardError e(z);
  return e;
}

void NoDiscardTestReturnError() {
  // NoDiscardError is declared nodiscard, so ignoring the return value of
  // NoDiscardReturnError() should be a warning, but isn't.
  // Filed as: https://github.com/apple/swift/issues/59002
  NoDiscardReturnError(5, 5);
}

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_
