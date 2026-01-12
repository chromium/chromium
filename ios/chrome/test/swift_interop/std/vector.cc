// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/std/vector.h"

SWIFT_DEFINE_MOVE_ONLY_INTEROP_WRAPPER(CxxStringVector, StringVector)

IntVector GetFortyTwoVector() {
  return {42};
}

bool CheckFortyTwoInVector(const IntVector& input) {
  return input.size() == 1 && input[0] == 42;
}

CxxStringVector GetStringVector() {
  std::vector<std::unique_ptr<std::string>> v;
  v.push_back(std::make_unique<std::string>("a"));
  v.push_back(std::make_unique<std::string>("b"));
  v.push_back(std::make_unique<std::string>("c"));
  return v;
}
