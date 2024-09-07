// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/segmented_string.h"

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;

  enum operation : int {
    kOpFinish,
    kOpAppend,
    kOpAdvance,
    kOpPush,
    kOpLookahead,
    kOpLast  // Keep at the end.
  };

  blink::FuzzedDataProvider fuzzed_data(data, size);

  // Take a random length string of `max_length` and replace 0 of more random
  // characters with \n.
  auto gen_str = [&fuzzed_data](size_t max_length) {
    auto str = fuzzed_data.ConsumeRandomLengthString(max_length);
    if (str.empty()) {
      return str;
    }
    while (fuzzed_data.ConsumeBool()) {
      auto offset =
          fuzzed_data.ConsumeIntegralInRange<unsigned int>(0, str.length() - 1);
      if (!offset) {
        break;
      }
      str.replace(offset, 0, String("\n"));
    }
    return str;
  };

  blink::SegmentedString seg_string(gen_str(100u));

  if (fuzzed_data.ConsumeBool()) {
    seg_string.SetExcludeLineNumbers();
  }

  bool finished = false;

  while (!finished) {
    operation op =
        static_cast<operation>(fuzzed_data.ConsumeIntegralInRange<int>(
            operation::kOpFinish, operation::kOpLast - 1));
    String character;
    switch (op) {
      case kOpFinish:
        finished = true;
        break;
      case kOpAppend:
        seg_string.Append(gen_str(100u));
        break;
      case kOpAdvance:
        std::ignore = seg_string.Advance();
        break;
      case kOpPush:
        character = fuzzed_data.ConsumeRandomLengthString(1);
        if (character.empty()) {
          break;
        }
        seg_string.Push(character[0]);
        break;
      case kOpLookahead:
        seg_string.LookAhead(gen_str(10u));
        break;
      case kOpLast:
        NOTREACHED();
    }

    seg_string.UpdateLineNumber();

    std::ignore = seg_string.CurrentColumn();
    std::ignore = seg_string.CurrentLine();
    std::ignore = seg_string.CurrentChar();
    std::ignore = seg_string.ToString();
    std::ignore = seg_string.NextSegmentedString();
  }

  return 0;
}
