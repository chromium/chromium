// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/perf_test_suite.h"
#include "third_party/blink/renderer/controller/tests/blink_test_suite.h"

int main(int argc, char** argv) {
  BlinkUnitTestSuite<base::PerfTestSuite> test_suite(argc, argv);
  return test_suite.Run();
}
