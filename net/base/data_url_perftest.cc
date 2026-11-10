// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/data_url.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <string>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/base64.h"
#include "net/base/features.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "url/gurl.h"

namespace net {
namespace {

// Sizes 21/22/23 cover all padding lengths and straddle the 22-byte
// inline string capacity used by 64-bit libc++.
constexpr auto kPayloadSizes =
    std::to_array<int64_t>({0, 21, 22, 23, 64, 1024, 4096, 65536, 1048576});

enum class Input {
  kBase64,
  kEscaped,
  kWhitespace,
  kInvalid,
  kPlain,
};

std::string MakePayload(size_t size) {
  std::string payload(size, '\0');
  std::ranges::iota(payload, uint8_t{0});
  return payload;
}

void DecoderArguments(::benchmark::Benchmark* benchmark) {
  for (int64_t size : kPayloadSizes) {
    benchmark->Arg(size);
  }
  benchmark->ArgName("payload_bytes");
}

template <Input input>
void ParserArguments(::benchmark::Benchmark* benchmark) {
  for (int64_t size : kPayloadSizes) {
    // GURL trims the whitespace when there is no encoded payload after it.
    if (input == Input::kWhitespace && size == 0) {
      continue;
    }
    for (int64_t simd : {0, 1}) {
      // Plain data URLs do not use either Base64 decoder.
      if (input == Input::kPlain && simd != 0) {
        continue;
      }
      benchmark->Args({size, simd});
    }
  }
  benchmark->ArgNames({"payload_bytes", "simd"});
}

void BM_SimdutfBase64Decode(::benchmark::State& state,
                            base::Base64DecodePolicy policy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSimdutfBase64Support);
  const std::string payload =
      MakePayload(base::checked_cast<size_t>(state.range(0)));
  const std::string encoded = base::Base64Encode(payload);
  std::string output;
  CHECK(SimdutfBase64Decode(encoded, &output, policy));
  CHECK_EQ(output, payload);

  for (auto _ : state) {
    bool success = SimdutfBase64Decode(encoded, &output, policy);
    ::benchmark::DoNotOptimize(success);
    ::benchmark::DoNotOptimize(output);
    ::benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(state.iterations() * state.range(0));
}

void BM_DataURLParse(::benchmark::State& state, Input input) {
  base::test::ScopedFeatureList feature_list;
  // The parser caches FurtherOptimizeParsingDataUrls on first use. Keep it
  // enabled for every case. To skip GTest suites and select parser benchmarks:
  // --gtest_filter='-*' --benchmark_filter='^BM_DataURLParse/'
  // SimdutfBase64Support is not cached, so it can vary between cases.
  feature_list.InitWithFeatureStates({
      {features::kSimdutfBase64Support, state.range(1) != 0},
      {features::kFurtherOptimizeParsingDataUrls, true},
      {features::kDataUrlMimeTypeParameterPreservation, true},
  });

  const std::string payload =
      MakePayload(base::checked_cast<size_t>(state.range(0)));
  std::string body = base::Base64Encode(payload);
  switch (input) {
    case Input::kBase64:
      break;
    case Input::kEscaped:
      // Force the percent-unescape retry, including for an empty payload.
      body.insert(0, "%20");
      break;
    case Input::kWhitespace:
      body.insert(0, " ");
      break;
    case Input::kInvalid:
      body.append("&");
      break;
    case Input::kPlain:
      body = base::EscapeAllExceptUnreserved(payload);
      break;
  }
  const GURL url(
      base::StrCat({"data:application/octet-stream",
                    input == Input::kPlain ? "," : ";base64,", body}));
  CHECK(url.is_valid());

  const bool expected_success = input != Input::kInvalid;
  {
    std::string mime_type;
    std::string charset;
    std::string data;
    CHECK_EQ(DataURL::Parse(url, &mime_type, &charset, &data),
             expected_success);
    if (expected_success) {
      CHECK_EQ(mime_type, "application/octet-stream");
      CHECK(charset.empty());
      CHECK_EQ(data, payload);
    }
  }

  // Payload generation, GURL construction and byte checks are outside timing.
  // Fresh output strings match callers and include their destruction in timing.
  for (auto _ : state) {
    std::string mime_type;
    std::string charset;
    std::string data;
    bool success = DataURL::Parse(url, &mime_type, &charset, &data);
    ::benchmark::DoNotOptimize(success);
    ::benchmark::DoNotOptimize(mime_type);
    ::benchmark::DoNotOptimize(charset);
    ::benchmark::DoNotOptimize(data);
    ::benchmark::ClobberMemory();
  }
  if (expected_success) {
    state.SetBytesProcessed(state.iterations() * state.range(0));
  }
}

BENCHMARK_CAPTURE(BM_SimdutfBase64Decode,
                  Strict,
                  base::Base64DecodePolicy::kStrict)
    ->Apply(DecoderArguments);
BENCHMARK_CAPTURE(BM_SimdutfBase64Decode,
                  Forgiving,
                  base::Base64DecodePolicy::kForgiving)
    ->Apply(DecoderArguments);

BENCHMARK_CAPTURE(BM_DataURLParse, Base64, Input::kBase64)
    ->Apply(ParserArguments<Input::kBase64>);
BENCHMARK_CAPTURE(BM_DataURLParse, Escaped, Input::kEscaped)
    ->Apply(ParserArguments<Input::kEscaped>);
BENCHMARK_CAPTURE(BM_DataURLParse, Whitespace, Input::kWhitespace)
    ->Apply(ParserArguments<Input::kWhitespace>);
BENCHMARK_CAPTURE(BM_DataURLParse, Invalid, Input::kInvalid)
    ->Apply(ParserArguments<Input::kInvalid>);
BENCHMARK_CAPTURE(BM_DataURLParse, Plain, Input::kPlain)
    ->Apply(ParserArguments<Input::kPlain>);

}  // namespace
}  // namespace net
