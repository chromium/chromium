// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/test_base_ort.h"

#include "services/webnn/ort/platform_functions_ort.h"

namespace webnn::ort {

void TestBaseOrt::SetUp() {
  if (!PlatformFunctions::EnsureInitialized()) {
    GTEST_SKIP() << "Failed to initialize ORT platform functions.";
  }
}

}  // namespace webnn::ort
