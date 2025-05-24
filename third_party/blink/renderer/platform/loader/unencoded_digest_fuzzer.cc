// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/unencoded_digest.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

static blink::BlinkFuzzerTestSupport test_support =
    blink::BlinkFuzzerTestSupport();

void CreateFuzzer(const std::string& input) {
  WTF::String header_value(input);

  blink::HTTPHeaderMap headers;
  headers.Set(blink::http_names::kUnencodedDigest, AtomicString(header_value));
  auto result = blink::UnencodedDigest::Create(headers);
}

FUZZ_TEST(UnencodedDigest, CreateFuzzer)
    .WithDomains(fuzztest::PrintableAsciiString());

void MatchFuzzer(const std::string& arbitrary) {
  AtomicString valid_header_value(
      "sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:");
  blink::HTTPHeaderMap headers;
  headers.Set(blink::http_names::kUnencodedDigest, valid_header_value);
  auto unencoded_digest = blink::UnencodedDigest::Create(headers);

  WTF::SegmentedBuffer buffer;
  buffer.Append(base::span(arbitrary));
  unencoded_digest->DoesMatch(&buffer);
}

FUZZ_TEST(UnencodedDigest, MatchFuzzer).WithDomains(fuzztest::String());
