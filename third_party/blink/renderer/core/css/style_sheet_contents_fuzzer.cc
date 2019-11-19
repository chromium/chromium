// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();

  const std::string data_string(reinterpret_cast<const char*>(data), size);
  const size_t data_hash = std::hash<std::string>()(data_string);
  const int is_strict_mode = (data_hash & std::numeric_limits<int>::max()) % 2;
  const int is_secure_context_mode =
      (std::hash<size_t>()(data_hash) & std::numeric_limits<int>::max()) % 2;

  auto* context = blink::MakeGarbageCollected<blink::CSSParserContext>(
      is_strict_mode ? blink::kHTMLStandardMode : blink::kHTMLQuirksMode,
      is_secure_context_mode ? blink::SecureContextMode::kSecureContext
                             : blink::SecureContextMode::kInsecureContext);
  auto* styleSheet =
      blink::MakeGarbageCollected<blink::StyleSheetContents>(context);

  styleSheet->ParseString(String::FromUTF8WithLatin1Fallback(
      reinterpret_cast<const char*>(data), size));

  return 0;
}

