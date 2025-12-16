// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/vector.h"

IntVector GetFortyTwoVector() {
  return {42};
}

bool CheckFortyTwoInVector(const IntVector& input) {
  return input.size() == 1 && input[0] == 42;
}
