// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

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

  blink::FuzzedDataProvider fuzzed_data(data, size);

  bool use_default = fuzzed_data.ConsumeBool();

  // Keep fuzz data layout the same.
  auto maybe_locale = fuzzed_data.ConsumeRandomLengthString(10u);

  const blink::LayoutLocale* locale;
  if (use_default) {
    locale = &blink::LayoutLocale::GetDefault();
  } else {
    locale = blink::LayoutLocale::Get(AtomicString(maybe_locale));
  }

  if (!locale) {
    return 0;
  }

  auto* hyphen = locale->GetHyphenation();

  if (!hyphen) {
    return 0;
  }

  auto string_data = AtomicString(
      fuzzed_data.ConsumeRandomLengthString(fuzzed_data.RemainingBytes()));
  std::ignore = hyphen->HyphenLocations(string_data);

  return 0;
}
