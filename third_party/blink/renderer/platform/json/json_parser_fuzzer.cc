// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/json/json_parser.h"

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;
  blink::JSONCommentState comment_state =
      blink::JSONCommentState::kAllowedButAbsent;
  blink::ParseJSON(WTF::String(data, size), comment_state, 500);
  return 0;
}
