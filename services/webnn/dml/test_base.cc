// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/test_base.h"

#include "base/command_line.h"
#include "ui/gl/gl_switches.h"

namespace webnn::dml {

bool UseGPUInTests() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseGpuInTests);
}

void TestBase::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
}

}  // namespace webnn::dml
