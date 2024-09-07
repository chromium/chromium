// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_parser.h"

#include "third_party/blink/renderer/core/svg/svg_path_string_source.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  String input_string = String::FromUTF8WithLatin1Fallback(data, size);
  blink::SVGPathStringSource source(input_string);
  class NullConsumer {
   public:
    void EmitSegment(const blink::PathSegmentData&) {}
  } null_consumer;
  blink::svg_path_parser::ParsePath(source, null_consumer);
  return 0;
}
