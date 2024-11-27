// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/identity_digest.h"

#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void CreateFuzzer(const std::string& input) {
  static blink::BlinkFuzzerTestSupport test_support;

  WTF::String header_value(input);

  blink::HTTPHeaderMap headers;
  headers.Set(blink::http_names::kIdentityDigest, AtomicString(header_value));
  auto result = blink::IdentityDigest::Create(headers);
}

FUZZ_TEST(IdentityDigest, CreateFuzzer)
    .WithDomains(fuzztest::PrintableAsciiString());
