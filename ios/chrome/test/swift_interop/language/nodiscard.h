// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_

[[nodiscard]] int NoDiscardAdd(int x, int y);

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

NoDiscardError NoDiscardReturnError(int x, int y);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_LANGUAGE_NODISCARD_H_
