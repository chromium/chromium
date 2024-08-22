// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;
  blink::FuzzedDataProvider data_provider(data, size);
  String content = data_provider.ConsumeRandomLengthString(size);
  content.Ensure16Bit();
  blink::UTF16TextIterator text_iterator(content.Characters16(),
                                         content.length());
  UChar32 code_point;
  while (text_iterator.Consume(code_point)) {
    WTF::unicode::ItalicMathVariant(code_point);
    text_iterator.Advance();
  }
  return 0;
}
