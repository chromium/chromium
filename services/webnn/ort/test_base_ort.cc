// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/test_base_ort.h"

#include "services/webnn/ort/platform_functions_ort.h"

namespace webnn::ort {

void TestBaseOrt::SetUp() {
  // Skip tests if the loading platform functions fail.
  // In order to be able to run this test suite successfully, the developer
  // needs to place a copy of onnxruntime.dll which supports ORT_API_VERSION
  // defined in onnxruntime_c_api.h into Chromium module folder before.
  if (!PlatformFunctions::GetInstance()) {
    GTEST_SKIP() << "!PlatformFunctions::GetInstance()";
  }
}

}  // namespace webnn::ort
