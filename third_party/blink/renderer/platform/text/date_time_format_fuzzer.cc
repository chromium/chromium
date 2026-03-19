// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/date_time_format.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DummyTokenHandler : public DateTimeFormat::TokenHandler {
 public:
  ~DummyTokenHandler() override = default;

  void VisitField(DateTimeFormat::FieldType field_type, int count) override {
    CHECK(field_type != DateTimeFormat::FieldType::kFieldTypeInvalid);
    CHECK_GE(count, 1);
  }

  void VisitLiteral(const String& string) override {
    CHECK_GT(string.length(), 0u);
  }
};

}  // namespace blink

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;
  blink::DummyTokenHandler handler;
  blink::DateTimeFormat::Parse(blink::String::FromUTF8(data), handler);
  return 0;
}
