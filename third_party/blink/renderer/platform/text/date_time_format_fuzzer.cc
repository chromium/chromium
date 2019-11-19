// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/date_time_format.h"

#include <stddef.h>
#include <stdint.h>
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DummyTokenHandler : public DateTimeFormat::TokenHandler {
 public:
  ~DummyTokenHandler() override = default;

  void VisitField(DateTimeFormat::FieldType field_type, int count) override {
    CHECK(field_type != DateTimeFormat::FieldType::kFieldTypeInvalid);
    CHECK_GE(count, 1);
  }

  void VisitLiteral(const WTF::String& string) override {
    CHECK_GT(string.length(), 0u);
  }
};

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::DummyTokenHandler handler;
  blink::DateTimeFormat::Parse(
      WTF::String::FromUTF8(reinterpret_cast<const char*>(data), size),
      handler);
  return 0;
}
