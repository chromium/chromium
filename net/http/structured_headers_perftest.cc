// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <algorithm>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace net::structured_headers {

namespace {

void BM_StructuredHeadersParseItemDistinctParams(::benchmark::State& state) {
  const int num_params = state.range(0);
  std::string input = "item";
  input.reserve(num_params * 8);
  for (int i = 0; i < num_params; ++i) {
    base::StrAppend(&input, {";k", base::NumberToString(i)});
  }

  for (auto _ : state) {
    ::benchmark::DoNotOptimize(ParseItemStrict(input));
  }
}

void BM_StructuredHeadersParseItemDuplicateParams(::benchmark::State& state) {
  const int num_params = state.range(0);
  const int unique_keys = std::max(1, num_params / 8);
  std::string input = "item";
  input.reserve(num_params * 6);
  for (int i = 0; i < num_params; ++i) {
    base::StrAppend(&input, {";k", base::NumberToString(i % unique_keys)});
  }

  for (auto _ : state) {
    ::benchmark::DoNotOptimize(ParseItemStrict(input));
  }
}

void BM_StructuredHeadersParseDictionaryDistinctKeys(
    ::benchmark::State& state) {
  const int num_keys = state.range(0);
  std::string input = "k0";
  input.reserve(num_keys * 8);
  for (int i = 1; i < num_keys; ++i) {
    base::StrAppend(&input, {", k", base::NumberToString(i)});
  }

  for (auto _ : state) {
    ::benchmark::DoNotOptimize(ParseDictionaryStrict(input));
  }
}

void BM_StructuredHeadersParseDictionaryDuplicateKeys(
    ::benchmark::State& state) {
  const int num_keys = state.range(0);
  const int unique_keys = std::max(1, num_keys / 8);
  std::string input = "k0";
  input.reserve(num_keys * 6);
  for (int i = 1; i < num_keys; ++i) {
    base::StrAppend(&input, {", k", base::NumberToString(i % unique_keys)});
  }

  for (auto _ : state) {
    ::benchmark::DoNotOptimize(ParseDictionaryStrict(input));
  }
}

BENCHMARK(BM_StructuredHeadersParseItemDistinctParams)
    ->RangeMultiplier(8)
    ->Range(8, 32768);
BENCHMARK(BM_StructuredHeadersParseItemDuplicateParams)
    ->RangeMultiplier(8)
    ->Range(8, 32768);
BENCHMARK(BM_StructuredHeadersParseDictionaryDistinctKeys)
    ->RangeMultiplier(8)
    ->Range(8, 32768);
BENCHMARK(BM_StructuredHeadersParseDictionaryDuplicateKeys)
    ->RangeMultiplier(8)
    ->Range(8, 32768);

}  // namespace

}  // namespace net::structured_headers
