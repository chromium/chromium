// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/test_base_ort.h"

#include "base/win/windows_version.h"
#include "services/webnn/ort/platform_functions_ort.h"

namespace webnn::ort {

void TestBaseOrt::SetUp() {
  if (base::win::GetVersion() < base::win::Version::WIN11_24H2) {
    GTEST_SKIP() << "The Windows version is too old.";
  }

  if (!PlatformFunctions::GetInstance()) {
    GTEST_SKIP() << "Failed to initialize ORT platform functions.";
  }
}

}  // namespace webnn::ort
