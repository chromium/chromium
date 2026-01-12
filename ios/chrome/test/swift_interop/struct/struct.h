// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_STRUCT_STRUCT_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_STRUCT_STRUCT_H_

#include <string>

// Boolean, but for foo.
struct Foolean {
  Foolean() {}

  bool value;
  std::string description;

  static std::string GetDescriptionForValue(bool value) {
    return std::string(value ? "true" : "false");
  }
};

inline bool IsFooleanValid(Foolean foo) {
  return foo.description == Foolean::GetDescriptionForValue(foo.value);
}

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_STRUCT_STRUCT_H_
