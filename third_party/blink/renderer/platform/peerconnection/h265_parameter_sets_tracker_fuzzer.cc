// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/h265_parameter_sets_tracker.h"

#include <stdint.h>

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;
  blink::H265ParameterSetsTracker h265_parameter_sets_tracker;
  h265_parameter_sets_tracker.MaybeFixBitstream(
      rtc::ArrayView<const uint8_t>(data, size));
  return 0;
}
