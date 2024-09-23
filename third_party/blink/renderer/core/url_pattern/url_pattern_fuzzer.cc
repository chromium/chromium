// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 4096)
    return 0;

  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  DummyExceptionStateForTesting exception_state;
  auto* input = MakeGarbageCollected<V8URLPatternInput>(
      String::FromUTF8(reinterpret_cast<const char*>(data), size));
  URLPattern::Create(task_environment.isolate(), input, exception_state);
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
