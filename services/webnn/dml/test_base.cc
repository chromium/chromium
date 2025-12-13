// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/test_base.h"

#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

void TestBase::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
}

}  // namespace webnn::dml
