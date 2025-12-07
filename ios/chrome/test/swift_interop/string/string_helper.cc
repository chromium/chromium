// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/string_helper.h"

std::string addStringFromCxx(std::string a) {
  return a + " string added in C++!";
}
