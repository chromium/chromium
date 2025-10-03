// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <string>

#include "base/check_op.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "url/gurl.h"

namespace net {

namespace {

static constexpr char kQueryUrl[] =
    "https://www.example.com/"
    "oaekrq?w=Ba-Kirc-Svizoo+whvfusdp+qincka+zfkl&xge_fue=w2a91s8849t6h1wr&"
    "mmcyv=AT9YphEuOenPod8NwMoKK-8iUmHIV1J_6C:2065255645329&kr=7Cr_bH_"
    "SVyYq3kxTu_em5Tp&hcyxg=70&ee=O&kxln="
    "Ai45ST2VOnPImanmF66COSKgyOyPSNdQY3OZ8rE9aHbZjQpJsdDRKN8Zcx54_l-"
    "g7oxJqEddKz6ZFr0MyRDVYUA1drHTCLlBSRqPZH&wuk="
    "0vpRTHfdR2ppOvvrTLwJkjKNJDGd6Nx6T6RROflZBXPVA&bpm=5746&oqq=0460&mzx=0";

bool RunNewImplementation(const HttpNoVarySearchData& nvs_data,
                          const GURL& url1,
                          const GURL& url2) {
  return nvs_data.AreEquivalentNewImplForTesting(url1, url2);
}

bool RunOldImplementation(const HttpNoVarySearchData& nvs_data,
                          const GURL& url1,
                          const GURL& url2) {
  return nvs_data.AreEquivalentOldImplForTesting(url1, url2);
}

void Benchmark(benchmark::State& state,
               const GURL& url1,
               const GURL& url2,
               bool expected,
               bool use_new_implementation) {
  // Equivalent to "No-Vary-Search: key-order, params, except=("w" "spinptou")"
  const auto nvs_data =
      HttpNoVarySearchData::CreateFromVaryParams({"w", "spinptou"}, false);

  const auto are_equivalent =
      use_new_implementation ? RunNewImplementation : RunOldImplementation;

  for (auto _ : state) {
    bool equivalent = are_equivalent(nvs_data, url1, url2);
    benchmark::DoNotOptimize(equivalent);
    DCHECK_EQ(equivalent, expected);
  }
}

void BM_HttpNoVarySearchDataAreEquivalentMatch(benchmark::State& state,
                                               bool use_new_implementation) {
  const GURL url1(kQueryUrl);
  const GURL url2(std::string(kQueryUrl) + "&pf=cs");
  Benchmark(state, url1, url2, true, use_new_implementation);
}

void BM_HttpNoVarySearchDataAreEquivalentNoMatch(benchmark::State& state,
                                                 bool use_new_implementation) {
  const GURL url1(kQueryUrl);
  const GURL url2(std::string(kQueryUrl) + "&spinptou=7");
  Benchmark(state, url1, url2, false, use_new_implementation);
}

BENCHMARK_CAPTURE(BM_HttpNoVarySearchDataAreEquivalentMatch,
                  old_implementation,
                  false)
    ->MinWarmUpTime(1.0);
BENCHMARK_CAPTURE(BM_HttpNoVarySearchDataAreEquivalentMatch,
                  new_implementation,
                  true)
    ->MinWarmUpTime(1.0);
BENCHMARK_CAPTURE(BM_HttpNoVarySearchDataAreEquivalentNoMatch,
                  old_implementation,
                  false)
    ->MinWarmUpTime(1.0);
BENCHMARK_CAPTURE(BM_HttpNoVarySearchDataAreEquivalentNoMatch,
                  new_implementation,
                  true)
    ->MinWarmUpTime(1.0);

}  // namespace

}  // namespace net
