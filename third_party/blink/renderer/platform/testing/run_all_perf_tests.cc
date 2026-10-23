// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_perf_test_suite.h"

#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"

int main(int argc, char** argv) {
  blink::BlinkPerfTestSuite suite(argc, argv);
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
  return suite.Run();
}
