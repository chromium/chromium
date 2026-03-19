// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Fuzzer for blink::ManifestParser
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  String string = String::FromUTF8(data);
  KURL manifest_url("https://whatever.test/manifest.json");
  KURL document_url("https://whatever.test/");
  ManifestParser parser(string, manifest_url, document_url,
                        /*feature_context=*/nullptr);
  parser.Parse();
  return 0;
}

}  // namespace blink
