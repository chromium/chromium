// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/no_vary_search_cache_test_utils.h"
#include "third_party/abseil-cpp/absl/base/call_once.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "url/gurl.h"

namespace net {
namespace {

// Maximum number of entries in the NoVarySearchCache.
constexpr size_t kMaxSize = 1000u;

// When generating a request to lookup in the cache, which key it should fail to
// match on (or kIsAsMatch if the match should succeed).
enum class DiffersWhere {
  kNIK,
  kBaseURL,
  kQuery,
  kIsAMatch,
};

// Because GoogleBench only supports integer args, different cases are specified
// as indexes into an array.
struct TestCase {
  // The three counts multiplied together will be the total number of entries in
  // the cache, and so must be <= kMaxSize.
  size_t distinct_nik_count;
  size_t distinct_base_url_count;
  size_t distinct_query_count;
  DiffersWhere differs_where;
};

constexpr auto kTestCases = std::to_array<TestCase>({
    {1000, 1, 1, DiffersWhere::kNIK},
    {1000, 1, 1, DiffersWhere::kIsAMatch},
    {1, 1000, 1, DiffersWhere::kBaseURL},
    {1, 1000, 1, DiffersWhere::kIsAMatch},
    {1, 1, 1000, DiffersWhere::kQuery},
    {1, 1, 1000, DiffersWhere::kIsAMatch},
    {10, 10, 10, DiffersWhere::kNIK},
    {10, 10, 10, DiffersWhere::kBaseURL},
    {10, 10, 10, DiffersWhere::kQuery},
    {10, 10, 10, DiffersWhere::kIsAMatch},
});

// Creates a request for insertion or lookup in the cache. The NIK, base URL and
// query parts can take different values which are specified by interpolating
// the numbers specified by `nik_index`, `base_url_index`, and `query_index`.
HttpRequestInfo CreateRequest(size_t nik_index,
                              size_t base_url_index,
                              size_t query_index) {
  const SchemefulSite top_frame_site(GURL("https://example.com/"));
  const SchemefulSite frame_site(
      GURL(base::StringPrintf("https://a%zi.example/", nik_index)));
  const NetworkIsolationKey nik(top_frame_site, frame_site);
  // The value of `junk_parameter` isn't important, but it is useful to have it
  // vary between URLs, and since this is a benchmark it is important that it be
  // deterministic.
  const uint32_t junk_parameter = static_cast<uint8_t>(
      nik_index * 999983 + base_url_index * 700001 + query_index * 100003);
  const std::string base64_junk =
      base::Base64Encode(base::byte_span_from_ref(junk_parameter));

  const GURL url(
      base::StringPrintf("https://www.a%zi.example/query?s=%zi&junk=%s",
                         base_url_index, query_index, base64_junk));
  return no_vary_search_cache_test_utils::CreateTestRequest(url, nik);
}

// Creates a request for looking up in the cache. `test_case.differs_where`
// specifies which component if any should mismatch.
HttpRequestInfo CreateRequestForLookup(const TestCase& test_case) {
  const auto& [distinct_nik_count, distinct_base_url_count,
               distinct_query_count, differs_where] = test_case;

  // These constants give the indexes of matching and non-matching values for
  // each key. Index 0 is always present, and the index after the last value is
  // never present.
  const size_t matching_nik = 0u;
  const size_t non_matching_nik = distinct_nik_count;
  const size_t matching_base_url = 0u;
  const size_t non_matching_base_url = distinct_base_url_count;
  const size_t matching_query = 0u;
  const size_t non_matching_query = distinct_query_count;

  switch (differs_where) {
    case DiffersWhere::kNIK:
      return CreateRequest(non_matching_nik, matching_base_url, matching_query);
    case DiffersWhere::kBaseURL:
      return CreateRequest(matching_nik, non_matching_base_url, matching_query);
    case DiffersWhere::kQuery:
      return CreateRequest(matching_nik, matching_base_url, non_matching_query);
    case DiffersWhere::kIsAMatch:
      return CreateRequest(matching_nik, matching_base_url, matching_query);
  }
}

// `cache` is global so FillCache() can fill it and BM_NoVarySearchCacheLookup()
// can performs lookups in it. It is wrapped in std::optional so that it can be
// destroyed and recreated for each case.
std::optional<NoVarySearchCache>& Cache() {
  static base::NoDestructor<std::optional<NoVarySearchCache>> cache;
  return *cache;
}

// To avoid state from leaking out into other performance tests in the same
// binary, this struct is constructed in SetFeatures() and destroyed again in
// ClearFeatures().
struct FeatureState {
  base::test::ScopedFeatureList scoped_feature_list;
  ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting
      equivalent_override{true};
};

std::optional<FeatureState>& GetFeatureState() {
  static base::NoDestructor<std::optional<FeatureState>> feature_state;
  return *feature_state;
}

void SetFeatures() {
  auto& feature_state = GetFeatureState();
  feature_state.emplace();
  feature_state->scoped_feature_list.InitWithFeatureState(
      features::kSplitCacheByNetworkIsolationKey, true);
}

void ClearFeatures(const benchmark::State& state) {
  GetFeatureState().reset();
}

// Sets up the contents of the cache for a single case. This can be very time
// consuming, so to avoid polluting the timing it is done in a separate setup
// step.
void FillCache(const benchmark::State& state) {
  SetFeatures();

  const int case_index = state.range(0);
  const TestCase& test_case = kTestCases[case_index];
  const auto& [distinct_nik_count, distinct_base_url_count,
               distinct_query_count, differs_where] = test_case;

  Cache().emplace(kMaxSize);
  auto& cache = Cache().value();
  const auto response_headers =
      no_vary_search_cache_test_utils::CreateTestHeaders("params=(\"junk\")");

  for (size_t nik_index = 0; nik_index < distinct_nik_count; ++nik_index) {
    for (size_t base_url_index = 0; base_url_index < distinct_base_url_count;
         ++base_url_index) {
      for (size_t query_index = 0; query_index < distinct_query_count;
           ++query_index) {
        const HttpRequestInfo request =
            CreateRequest(nik_index, base_url_index, query_index);
        cache.MaybeInsert(request, *response_headers);
      }
    }
  }

  CHECK_EQ(cache.size(),
           distinct_nik_count * distinct_base_url_count * distinct_query_count);
}

// Performs the actual benchmark, just calling the Lookup() method in a loop.
void BM_NoVarySearchCacheLookup(benchmark::State& state) {
  const int case_index = state.range(0);
  const TestCase& test_case = kTestCases[case_index];
  const auto request = CreateRequestForLookup(test_case);
  auto& cache = Cache().value();

  for (auto _ : state) {
    auto lookup_result = cache.Lookup(request);
    benchmark::DoNotOptimize(lookup_result);
    benchmark::ClobberMemory();
    CHECK_EQ(lookup_result.has_value(),
             test_case.differs_where == DiffersWhere::kIsAMatch);
  }
}

BENCHMARK(BM_NoVarySearchCacheLookup)
    ->DenseRange(0, std::size(kTestCases) - 1)
    ->Setup(FillCache)
    ->Teardown(ClearFeatures);

}  // namespace
}  // namespace net
