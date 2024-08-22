// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;

  if (size <= 4) {
    return 0;
  }

  blink::FuzzedDataProvider provider(data, size);

  const auto property_id =
      blink::ConvertToCSSPropertyID(provider.ConsumeIntegralInRange<int>(
          blink::kIntFirstCSSProperty, blink::kIntLastCSSProperty));
  const auto data_string = provider.ConsumeRemainingBytes();

  for (unsigned parser_mode = 0;
       parser_mode < blink::CSSParserMode::kNumCSSParserModes; parser_mode++) {
    auto* context = MakeGarbageCollected<blink::CSSParserContext>(
        static_cast<blink::CSSParserMode>(parser_mode),
        blink::SecureContextMode::kInsecureContext);
    blink::CSSParserFastPaths::MaybeParseValue(
        property_id,
        String::FromUTF8WithLatin1Fallback(data_string.data(),
                                           data_string.length()),
        context);
  }

  return 0;
}
