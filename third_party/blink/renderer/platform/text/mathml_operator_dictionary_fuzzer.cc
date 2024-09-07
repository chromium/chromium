// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/mathml_operator_dictionary.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;
  blink::FuzzedDataProvider data_provider(data, size);
  const blink::MathMLOperatorDictionaryForm form =
      data_provider.ConsumeEnum<blink::MathMLOperatorDictionaryForm>();
  String content = data_provider.ConsumeRandomLengthString(size - 1);
  content.Ensure16Bit();
  blink::FindCategory(content, form);
  return 0;
}
