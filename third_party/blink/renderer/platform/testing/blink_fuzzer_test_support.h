// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_BLINK_FUZZER_TEST_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_BLINK_FUZZER_TEST_SUPPORT_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Instantiating BlinkFuzzerTestSupport will spin up an environment similar to
// blink_unittests. It should be statically initialized and leaked in fuzzers.
class BlinkFuzzerTestSupport {
  STACK_ALLOCATED();

 public:
  // Use this constructor in LLVMFuzzerTestOneInput.
  BlinkFuzzerTestSupport();

  // Use this constructor in LLVMFuzzerInitialize only if argv is necessary.
  BlinkFuzzerTestSupport(int argc, char** argv);
  ~BlinkFuzzerTestSupport();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_BLINK_FUZZER_TEST_SUPPORT_H_
