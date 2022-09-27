// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_perf_test_suite.h"

int main(int argc, char** argv) {
  return blink::BlinkPerfTestSuite(argc, argv).Run();
}
