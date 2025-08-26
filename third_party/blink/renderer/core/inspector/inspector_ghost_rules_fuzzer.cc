// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ghost_rules.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void PopulateFuzzer(const std::string& input) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  blink::DummyPageHolder holder;
  blink::CSSStyleSheet* sheet =
      blink::css_test_helpers::CreateStyleSheet(holder.GetDocument());
  sheet->replaceSync(blink::String(input), IGNORE_EXCEPTION_FOR_TESTING);
  {
    blink::InspectorGhostRules ghost_rules;
    blink::HeapVector<blink::Member<blink::CSSStyleSheet>> sheets;
    sheets.push_back(sheet);
    ghost_rules.PopulateSheetsWithAssertion(std::move(sheets));
    // Note: ~InspectorGhostRules() is a relevant part of this test.
  }
}

FUZZ_TEST(InspectorGhostRules, PopulateFuzzer);
