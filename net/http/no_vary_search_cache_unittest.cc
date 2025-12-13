// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/base/pickle.h"
#include "net/base/pickle_traits.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/no_vary_search_cache_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

namespace nvs_test = no_vary_search_cache_test_utils;

using ::testing::_;
using ::testing::AllOf;
using ::testing::EndsWith;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::Le;
using ::testing::Optional;

constexpr size_t kMaxSize = 5;

class NoVarySearchCacheTest : public ::testing::TestWithParam<bool> {
 public:
  NoVarySearchCacheTest() {
    feature_list_.InitWithFeatureState(
        features::kSplitCacheByNetworkIsolationKey, GetParam());
  }

 protected:
  NoVarySearchCache& cache() { return cache_; }

  // Generates a URL with the query `query`.
  static GURL TestURL(std::string_view query = {}) {
    return nvs_test::CreateTestURL(query);
  }

  // Generates an HttpRequestInfo object containing a URL that has the query
  // `query`.
  static HttpRequestInfo TestRequest(std::string_view query = {}) {
    return nvs_test::CreateTestRequest(query);
  }

  // Generates an HttpRequestInfo object with the URL `url`.
  static HttpRequestInfo TestRequest(const GURL& url) {
    return nvs_test::CreateTestRequest(url);
  }

  // Generates an HttpRequestInfo object with the given `url` and `nik`.
  static HttpRequestInfo TestRequest(const GURL& url,
                                     const NetworkIsolationKey& nik) {
    return nvs_test::CreateTestRequest(url, nik);
  }

  // Returns a reference to an HttpResponseHeaders object with a No-Vary-Search
  // header with the value `no_vary_search`.
  const HttpResponseHeaders& TestHeaders(std::string_view no_vary_search) {
    response_header_storage_.push_back(
        nvs_test::CreateTestHeaders(no_vary_search));
    return *response_header_storage_.back();
  }

  // Inserts a URL with query `query` into cache with a No-Vary-Search header
  // value of `no_vary_search`.
  void Insert(std::string_view query, std::string_view no_vary_search) {
    nvs_test::Insert(cache_, query, no_vary_search);
  }

  // Returns true if TestURL(query) exists in cache.
  bool Exists(std::string_view query) {
    return nvs_test::Exists(cache_, query);
  }

  // Returns true if inserting a request with query `insert` results in a lookup
  // for query `lookup` succeeding, assuming a No-Vary-Search header value of
  // `no_vary_search`.
  bool Matches(std::string_view insert,
               std::string_view lookup,
               std::string_view no_vary_search) {
    NoVarySearchCache cache(kMaxSize);

    cache.MaybeInsert(TestRequest(insert), TestHeaders(no_vary_search));
    EXPECT_EQ(cache.size(), 1u);

    const auto exists = [&cache](std::string_view query) {
      return cache.Lookup(TestRequest(query)).has_value();
    };

    // It would be bad if the query didn't match itself.
    EXPECT_TRUE(exists(insert));

    return exists(lookup);
  }

  // Returns true if inserting a request with query `insert2` after a request
  // for `insert1` results in the new request replacing the old one, meaning
  // they were match according to the No-Vary-Search header value of
  // `no_vary_search`.
  bool InsertMatches(std::string_view insert1,
                     std::string_view insert2,
                     std::string_view no_vary_search) {
    NoVarySearchCache cache(kMaxSize);
    const auto insert = [&cache, no_vary_search, this](std::string_view query) {
      cache.MaybeInsert(TestRequest(query), TestHeaders(no_vary_search));
    };

    insert(insert1);
    EXPECT_EQ(cache.size(), 1u);
    insert(insert2);
    return cache.size() == 1u;
  }

  std::string GenerateCachePartitionKey(std::string_view url) {
    const auto request = TestRequest(GURL(url));
    std::optional<std::string> maybe_cache_key =
        HttpCache::GenerateCachePartitionKeyForRequest(request);
    EXPECT_TRUE(maybe_cache_key);
    return maybe_cache_key.value_or(std::string());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  NoVarySearchCache cache_{kMaxSize};

  // Scratch space for HttpResponseHeaders objects so that TestHeaders() can
  // return a const reference for convenience. This is thrown away at the end of
  // each test.
  std::vector<scoped_refptr<HttpResponseHeaders>> response_header_storage_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NoVarySearchCacheTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SplitCacheEnabled"
                                             : "SplitCacheDisabled";
                         });

TEST_P(NoVarySearchCacheTest, NewlyConstructedCacheIsEmpty) {
  EXPECT_EQ(cache().size(), 0u);
}

TEST_P(NoVarySearchCacheTest, LookupOnEmptyCache) {
  EXPECT_EQ(cache().Lookup(TestRequest()), std::nullopt);
}

TEST_P(NoVarySearchCacheTest, InsertLookupErase) {
  Insert("", "key-order");

  auto result = cache().Lookup(TestRequest());
  ASSERT_TRUE(result);
  EXPECT_EQ(result->original_url, TestURL());

  EXPECT_EQ(cache().size(), 1u);

  cache().Erase(std::move(result->erase_handle));
  EXPECT_EQ(cache().size(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, MoveConstruct) {
  Insert("a=b", "key-order");

  NoVarySearchCache new_cache = std::move(cache());

  EXPECT_TRUE(new_cache.Lookup(TestRequest("a=b")));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(cache().size(), 0u);
}

// An asan build will find leaks, but this test works on any build.
TEST_P(NoVarySearchCacheTest, QueryNotLeaked) {
  std::optional<NoVarySearchCache::LookupResult> result;
  {
    NoVarySearchCache cache(kMaxSize);

    cache.MaybeInsert(TestRequest(), TestHeaders("params"));
    result = cache.Lookup(TestRequest());
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->erase_handle.IsGoneForTesting());
  }
  EXPECT_TRUE(result->erase_handle.IsGoneForTesting());
}

std::string QueryWithIParameter(size_t i) {
  return "i=" + base::NumberToString(i);
}

constexpr std::string_view kVaryOnIParameter = "params, except=(\"i\")";

TEST_P(NoVarySearchCacheTest, OldestItemIsEvicted) {
  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    std::string query = QueryWithIParameter(i);
    Insert(query, kVaryOnIParameter);
    EXPECT_TRUE(Exists(query));
  }

  EXPECT_EQ(cache().size(), kMaxSize);

  EXPECT_FALSE(Exists("i=0"));
}

TEST_P(NoVarySearchCacheTest, RecentlyUsedItemIsNotEvicted) {
  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    std::string query = QueryWithIParameter(i);
    Insert(query, kVaryOnIParameter);
    EXPECT_TRUE(Exists(query));
    // Exists() calls Lookup(), which makes an entry "used".
    EXPECT_TRUE(Exists("i=0"));
  }

  EXPECT_EQ(cache().size(), kMaxSize);

  EXPECT_TRUE(Exists("i=0"));
  EXPECT_FALSE(Exists("i=1"));
}

TEST_P(NoVarySearchCacheTest, MostRecentlyUsedItemIsNotEvicted) {
  const auto query = QueryWithIParameter;
  // Fill the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(query(i), kVaryOnIParameter);
  }
  EXPECT_EQ(cache().size(), kMaxSize);

  // Make "i=3" be the most recently used item.
  EXPECT_TRUE(Exists("i=3"));

  // Evict kMaxSize - 1 items.
  for (size_t i = kMaxSize; i < kMaxSize * 2 - 1; ++i) {
    Insert(query(i), kVaryOnIParameter);
    EXPECT_TRUE(Exists(query(i)));
  }

  EXPECT_EQ(cache().size(), kMaxSize);

  EXPECT_TRUE(Exists("i=3"));
}

TEST_P(NoVarySearchCacheTest, LeastRecentlyUsedItemIsEvicted) {
  const auto query = QueryWithIParameter;
  // Fill the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(query(i), kVaryOnIParameter);
  }
  EXPECT_EQ(cache().size(), kMaxSize);

  // Make "i=kMaxSize-1" be the least recently used item.
  for (size_t i = 0; i < kMaxSize - 1; ++i) {
    EXPECT_TRUE(Exists(query(i)));
  }

  // Evict one item.
  Insert(query(kMaxSize), kVaryOnIParameter);

  // Verify it was the least-recently-used item.
  EXPECT_FALSE(Exists(query(kMaxSize - 1)));
}

TEST_P(NoVarySearchCacheTest, InsertUpdatesIdenticalItem) {
  Insert("a=b", "params=(\"c\")");
  auto original_result = cache().Lookup(TestRequest("a=b"));
  ASSERT_TRUE(original_result);
  Insert("a=b", "params=(\"c\")");
  auto new_result = cache().Lookup(TestRequest("a=b"));
  ASSERT_TRUE(new_result);
  EXPECT_TRUE(
      original_result->erase_handle.EqualsForTesting(new_result->erase_handle));
}

TEST_P(NoVarySearchCacheTest, InsertRemovesMatchingItem) {
  Insert("a=b&c=1", "params=(\"c\")");
  auto original_result = cache().Lookup(TestRequest("a=b"));
  ASSERT_TRUE(original_result);
  EXPECT_EQ(original_result->original_url, TestURL("a=b&c=1"));
  Insert("a=b&c=2", "params=(\"c\")");
  EXPECT_TRUE(original_result->erase_handle.IsGoneForTesting());
  EXPECT_EQ(cache().size(), 1u);
  auto new_result = cache().Lookup(TestRequest("a=b"));
  EXPECT_EQ(new_result->original_url, TestURL("a=b&c=2"));
}

TEST_P(NoVarySearchCacheTest, MaybeInsertDoesNothingWithNoNoVarySearchHeader) {
  auto headers = HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  cache().MaybeInsert(TestRequest(), *headers);
  EXPECT_EQ(cache().size(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, MaybeInsertDoesNothingForDefaultBehavior) {
  // The following header values are all equivalent to default behavior.
  static constexpr auto kDefaultCases = std::to_array<std::string_view>(
      {"", " ", "params=?0", "params=()", "key-order=?0", "nonsense"});

  for (auto no_vary_search : kDefaultCases) {
    NoVarySearchCache cache(kMaxSize);

    Insert("a=b", no_vary_search);
    EXPECT_EQ(cache.size(), 0u) << no_vary_search;
  }
}

// A size 1 cache is a special case, because eviction results in an empty cache.
TEST_P(NoVarySearchCacheTest, EvictWithSize1Cache) {
  NoVarySearchCache cache(1u);
  cache.MaybeInsert(TestRequest("a=1"), TestHeaders("key-order"));
  cache.MaybeInsert(TestRequest("a=2"), TestHeaders("key-order"));
  EXPECT_TRUE(cache.Lookup(TestRequest("a=2")).has_value());
  EXPECT_EQ(cache.size(), 1u);
}

// This is a regression test for a bug where insertion led to eviction of the
// only cache entry with the same base URL.
TEST_P(NoVarySearchCacheTest, InsertWithBaseURLMatchingEvicted) {
  static constexpr auto my_test_request = [](std::string_view query) {
    constexpr std::string_view kBaseURL = "https://a.example/?";
    return TestRequest(GURL(base::StrCat({kBaseURL, query})));
  };

  cache().MaybeInsert(my_test_request("will-be-evicted"),
                      TestHeaders("key-order"));
  for (size_t i = 1; i < kMaxSize; ++i) {
    Insert(QueryWithIParameter(i), kVaryOnIParameter);
  }
  EXPECT_EQ(cache().size(), kMaxSize);

  cache().MaybeInsert(my_test_request("same-base-url"),
                      TestHeaders("key-order"));
  EXPECT_TRUE(cache().Lookup(my_test_request("same-base-url")).has_value());
}

// This is a regression test for a bug where insertion led to eviction of the
// only cache entry with the same base URL & No-Vary-Search header value.
// The difference from the InsertWithBaseURLMatchingEvicted test is that in this
// test there are other entries with the same base URL.
TEST_P(NoVarySearchCacheTest, InsertWithNoVarySearchValueMatchingEvicted) {
  Insert("will-be-evicted", "params=(\"ignored\")");
  for (size_t i = 1; i < kMaxSize; ++i) {
    Insert(QueryWithIParameter(i), kVaryOnIParameter);
  }
  EXPECT_EQ(cache().size(), kMaxSize);

  Insert("same-nvs", "params=(\"ignored\")");
  EXPECT_TRUE(Exists("same-nvs"));
}

TEST_P(NoVarySearchCacheTest, MatchesWithoutQueryString) {
  auto url_with_query = GURL("https://example.com/foo?");
  cache().MaybeInsert(TestRequest(url_with_query), TestHeaders("key-order"));
  auto result = cache().Lookup(TestRequest(GURL("https://example.com/foo")));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->original_url, url_with_query);
}

TEST_P(NoVarySearchCacheTest, InsertInvalidURLIsIgnored) {
  auto invalid_url = GURL("???");
  ASSERT_FALSE(invalid_url.is_valid());
  cache().MaybeInsert(TestRequest(invalid_url), TestHeaders("key-order"));
  EXPECT_EQ(cache().size(), 0u);
}

// There's no way to insert an invalid URL into the cache. There's also no way
// to add a query to a valid URL to make it invalid. So this test just verifies
// that we don't crash.
TEST_P(NoVarySearchCacheTest, LookupInvalidURLReturnsNullopt) {
  GURL invalid_url = GURL("???");
  ASSERT_FALSE(invalid_url.is_valid());
  auto result = cache().Lookup(TestRequest(invalid_url));
  EXPECT_FALSE(result);
}

TEST_P(NoVarySearchCacheTest, MatchCases) {
  struct Case {
    std::string_view description;
    std::string_view query1;
    std::string_view query2;
    std::string_view no_vary_search;
  };

  static constexpr Case cases[] = {
      {"Encoded & in key", "%26=a&b=c", "b=c", "params=(\"&\")"},
      {"Encoded & in value", "a=b%26&c=d", "c=d&a=b%26", "key-order"},
      {"Encoded =", "%3d=a", "%3D=a", "key-order"},
      {"Encoded and unencoded =", "a=%3d", "a==", "key-order"},
      {"Embedded null in key", "a%00b=c", "", "params=(\"a%00b\")"},
      {"Embedded null in value", "a=b%00c", "a=b%00c", "key-order"},
      {"Encoded space in key", "+=a", "%20=b", "params=(\" \")"},
      {"Encoded space in value", "a=b&c=+", "c=%20&a=b", "key-order"},
      {"Key is ?", "?=1", "", "params=(\"?\")"},
      {"Empty key", "=7&c=d", "c=d", "params=(\"\")"},
      {"Empty value", "a=&c=d", "c=d&a", "key-order"},
      {"Bad UTF8", "%fe=%ff", "\xfe=\xff", "key-order"},
      {"Two params removed", "a=b&c=d&e", "e", R"(params=("a" "c"))"},
  };

  for (const auto& [description, query1, query2, no_vary_search] : cases) {
    EXPECT_TRUE(Matches(query1, query2, no_vary_search))
        << "Testing forwards: " << description;
    EXPECT_TRUE(Matches(query2, query1, no_vary_search))
        << "Testing backwards: " << description;
    EXPECT_TRUE(InsertMatches(query1, query2, no_vary_search))
        << "Testing double insert: " << description;
  }
}

TEST_P(NoVarySearchCacheTest, NoMatchCases) {
  struct Case {
    std::string_view description;
    std::string_view query1;
    std::string_view query2;
  };

  static constexpr Case cases[] = {
      {"Encoded &", "a&b", "a%26b"},
      {"Encoded =", "a=b", "a%3db"},
  };

  static constexpr std::string_view kKeyOrder = "key-order";

  for (const auto& [description, query1, query2] : cases) {
    EXPECT_FALSE(Matches(query1, query2, kKeyOrder))
        << "Testing forwards: " << description;
    EXPECT_FALSE(Matches(query2, query1, kKeyOrder))
        << "Testing backwards: " << description;
    EXPECT_FALSE(InsertMatches(query1, query2, kKeyOrder))
        << "Testing double insert: " << description;
  }
}

// Different representations of No-Very-Search headers that should compare
// equal.
TEST_P(NoVarySearchCacheTest, NoVarySearchVariants) {
  struct Case {
    std::string_view description;
    std::string_view variant1;
    std::string_view variant2;
  };

  static constexpr Case cases[] = {
      {"Extra space", R"(params=("a" "b"))", R"(params=( "a"  "b" ))"},
      {"Bool or omitted params", "params", "params=?1"},
      {"Bool or omitted key-order", "key-order", "key-order=?1"},
      {"Absent or false key-order", "params, key-order=?0", "params"},
      {"Ignored entry", "params, ignored", "params"},
      {"Empty except", "params, except=()", "except=(), params"},
      {"Different order", R"(params, except=("a"), key-order)",
       R"(key-order, except=("a"), params)"},
  };

  static constexpr std::string_view kQuery = "a=b&b=7&c=d";

  for (const auto& [description, variant1, variant2] : cases) {
    NoVarySearchCache cache(kMaxSize);

    cache.MaybeInsert(TestRequest(kQuery), TestHeaders(variant1));
    EXPECT_EQ(cache.size(), 1u);
    cache.MaybeInsert(TestRequest(kQuery), TestHeaders(variant2));
    EXPECT_EQ(cache.size(), 1u)
        << "Failing: " << description << "; variant1='" << variant1
        << "'; variant2 = '" << variant2 << "'";
  }
}

// Items with a transient NIK will not be stored in the disk cache, and so they
// shouldn't be stored in the NoVarySearchCache either.
TEST_P(NoVarySearchCacheTest, TransientNIK) {
  const auto transient = NetworkIsolationKey::CreateTransientForTesting();

  cache().MaybeInsert(TestRequest(TestURL(), transient), TestHeaders("params"));
  if (HttpCache::IsSplitCacheEnabled()) {
    EXPECT_EQ(cache().size(), 0u);
    EXPECT_FALSE(cache().Lookup(TestRequest(TestURL(), transient)));
  } else {
    EXPECT_EQ(cache().size(), 1u);
    EXPECT_TRUE(cache().Lookup(TestRequest(TestURL(), transient)));
  }
}

TEST_P(NoVarySearchCacheTest, DifferentNIK) {
  const NetworkIsolationKey different_nik(
      SchemefulSite(TestURL()),
      SchemefulSite(GURL("https://thirdparty.example/")));

  cache().MaybeInsert(TestRequest(), TestHeaders("params"));
  cache().MaybeInsert(TestRequest(TestURL(), different_nik),
                      TestHeaders("params"));

  const auto result1 = cache().Lookup(TestRequest());
  const auto result2 = cache().Lookup(TestRequest(TestURL(), different_nik));
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);

  const size_t cache_size = cache().size();
  const bool handles_are_equal =
      result1->erase_handle.EqualsForTesting(result2->erase_handle);

  if (HttpCache::IsSplitCacheEnabled()) {
    EXPECT_EQ(cache_size, 2u);
    EXPECT_FALSE(handles_are_equal);
  } else {
    EXPECT_EQ(cache_size, 1u);
    EXPECT_TRUE(handles_are_equal);
  }
}

TEST_P(NoVarySearchCacheTest, DifferentURL) {
  const GURL url1("https://example.com/a?a=b");
  const GURL url2("https://example.com/b?a=b");

  cache().MaybeInsert(TestRequest(url1), TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(url2), TestHeaders("key-order"));
  EXPECT_EQ(cache().size(), 2u);
  const auto result1 = cache().Lookup(TestRequest(url1));
  const auto result2 = cache().Lookup(TestRequest(url2));
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  EXPECT_FALSE(result1->erase_handle.EqualsForTesting(result2->erase_handle));
}

void SpinUntilCurrentTimeChanges() {
  const auto start = base::Time::Now();
  while (start == base::Time::Now()) {
    base::PlatformThread::YieldCurrentThread();
  }
}

TEST_P(NoVarySearchCacheTest, DifferentNoVarySearch) {
  Insert("a=b&c=d", "params, except=(\"a\")");
  // Make sure that the two inserts reliably get a different `inserted`
  // timestamp so that the ordering is deterministic.
  SpinUntilCurrentTimeChanges();
  Insert("a=b", "key-order");

  EXPECT_EQ(cache().size(), 2u);
  const auto result = cache().Lookup(TestRequest("a=b"));
  ASSERT_TRUE(result);
  // If time goes backwards this test will flake.
  EXPECT_EQ(result->original_url, TestURL("a=b"));
}

// The winner of the lookup should depend only on insertion order and not on the
// order of iteration of the map. To ensure this works for any iteration order,
// we perform the same test in the opposite direction.
TEST_P(NoVarySearchCacheTest, DifferentNoVarySearchReverseOrder) {
  Insert("a=b", "key-order");
  SpinUntilCurrentTimeChanges();
  Insert("a=b&c=d", "params, except=(\"a\")");

  EXPECT_EQ(cache().size(), 2u);
  const auto result = cache().Lookup(TestRequest("a=b"));
  ASSERT_TRUE(result);
  // If time goes backwards this test will flake.
  EXPECT_EQ(result->original_url, TestURL("a=b&c=d"));
}

TEST_P(NoVarySearchCacheTest, EraseInDifferentOrder) {
  // Insert in order a, b, c.
  Insert("a", "key-order");
  Insert("b", "key-order");
  Insert("c", "key-order");

  // Look up in order b, c, a.
  const auto lookup = [this](std::string_view query) {
    return cache().Lookup(TestRequest(query));
  };

  auto result_b = lookup("b");
  auto result_c = lookup("c");
  auto result_a = lookup("a");

  ASSERT_TRUE(result_a);
  ASSERT_TRUE(result_b);
  ASSERT_TRUE(result_c);

  // Erase in order c, a, b.
  cache().Erase(std::move(result_c->erase_handle));
  EXPECT_TRUE(Exists("a"));
  EXPECT_TRUE(Exists("b"));
  EXPECT_FALSE(Exists("c"));

  cache().Erase(std::move(result_a->erase_handle));
  EXPECT_FALSE(Exists("a"));
  EXPECT_TRUE(Exists("b"));

  cache().Erase(std::move(result_b->erase_handle));
  EXPECT_FALSE(Exists("b"));

  EXPECT_EQ(cache().size(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

// The URL "ref", also known as the "fragment", also known as the "hash", is
// ignored for matching and not stored in the cache.
TEST_P(NoVarySearchCacheTest, URLRefIsIgnored) {
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b#foo")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b#bar")),
                      TestHeaders("key-order"));
  EXPECT_EQ(cache().size(), 1u);
  auto result =
      cache().Lookup(TestRequest(GURL("https://example.com/?a=b#baz")));
  EXPECT_TRUE(result);
  EXPECT_EQ(result->original_url, GURL("https://example.com/?a=b"));
}

TEST_P(NoVarySearchCacheTest, URLWithUsernameIsRejected) {
  const GURL url_with_username("https://me@example.com/?a=b");
  cache().MaybeInsert(TestRequest(url_with_username), TestHeaders("key-order"));
  EXPECT_EQ(cache().size(), 0u);

  // See if it matches against the URL without the username.
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b")),
                      TestHeaders("key-order"));
  EXPECT_FALSE(cache().Lookup(TestRequest(url_with_username)));
}

TEST_P(NoVarySearchCacheTest, URLWithPasswordIsRejected) {
  const GURL url_with_password("https://:hunter123@example.com/?a=b");
  cache().MaybeInsert(TestRequest(url_with_password), TestHeaders("key-order"));
  EXPECT_EQ(cache().size(), 0u);

  // See if it matches against the URL without the password.
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b")),
                      TestHeaders("key-order"));
  EXPECT_FALSE(cache().Lookup(TestRequest(url_with_password)));
}

TEST_P(NoVarySearchCacheTest, ClearDataEverything) {
  cache().MaybeInsert(TestRequest(GURL("http://example/q?a=b")),
                      TestHeaders("key-order"));
  const NetworkIsolationKey different_nik(
      SchemefulSite(TestURL()),
      SchemefulSite(GURL("https://thirdparty.example/")));
  cache().MaybeInsert(TestRequest(GURL("http://example/q?a=b"), different_nik),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("https://example.com/search?q=z")),
                      TestHeaders("key-order"));

  const bool cleared = cache().ClearData(UrlFilterType::kFalseIfMatches, {}, {},
                                         base::Time(), base::Time::Max());

  EXPECT_TRUE(cleared);
  EXPECT_EQ(cache().size(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, ClearDataMatchOrigin) {
  // Scheme differs.
  cache().MaybeInsert(TestRequest(GURL("https://example.com/q?a=b")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("http://example.com/q?a=b")),
                      TestHeaders("key-order"));

  const bool cleared =
      cache().ClearData(UrlFilterType::kTrueIfMatches,
                        {url::Origin::Create(GURL("https://example.com/"))}, {},
                        base::Time(), base::Time::Max());

  EXPECT_TRUE(cleared);
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(cache()
                  .Lookup(TestRequest(GURL("http://example.com/q?a=b")))
                  .has_value());
}

TEST_P(NoVarySearchCacheTest, ClearDataMatchDomain) {
  // Scheme differs.
  cache().MaybeInsert(TestRequest(GURL("http://example.com:80/q?a=b")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("https://www.example.com:8080/q?a=b")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("https://other.example/q?a=b")),
                      TestHeaders("key-order"));

  const bool cleared =
      cache().ClearData(UrlFilterType::kTrueIfMatches, {}, {"example.com"},
                        base::Time(), base::Time::Max());

  EXPECT_TRUE(cleared);
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(cache()
                  .Lookup(TestRequest(GURL("https://other.example/q?a=b")))
                  .has_value());
}

TEST_P(NoVarySearchCacheTest, ClearDataMatchTime) {
  base::test::TaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME);
  // Function to convert a time string in the format HH:MM:SS to a base::Time.
  static constexpr auto time = [](std::string_view time) {
    base::Time output;
    const bool parsed = base::Time::FromUTCString(
        base::StrCat({"Tue, 4 Mar 2025 ", time, " GMT"}).c_str(), &output);
    CHECK(parsed);
    return output;
  };

  // Advance the mock time to `when`, specified in the format HH:MM:SS.
  const auto advance_time_to = [&](std::string_view when) {
    base::Time target_time = time(when);
    auto mock_now = base::Time::Now();
    CHECK_GT(target_time, mock_now);
    task_environment.FastForwardBy(target_time - mock_now);
  };

  advance_time_to("12:00:00");
  Insert("a=1", "key-order");

  advance_time_to("13:00:00");
  Insert("a=2", "key-order");

  advance_time_to("14:00:00");
  Insert("a=3", "key-order");

  const bool cleared = cache().ClearData(UrlFilterType::kFalseIfMatches, {}, {},
                                         time("12:30:00"), time("13:30:00"));

  EXPECT_TRUE(cleared);
  EXPECT_EQ(cache().size(), 2u);
  EXPECT_TRUE(Exists("a=1"));
  EXPECT_FALSE(Exists("a=2"));
  EXPECT_TRUE(Exists("a=3"));
}

TEST_P(NoVarySearchCacheTest, ClearDataEmptyCache) {
  const bool cleared =
      cache().ClearData(UrlFilterType::kTrueIfMatches,
                        {url::Origin::Create(GURL("https://example.com/"))}, {},
                        base::Time(), base::Time::Max());

  EXPECT_FALSE(cleared);
  EXPECT_EQ(cache().size(), 0u);
}

TEST_P(NoVarySearchCacheTest, ClearDataNoMatch) {
  Insert("a=1", "key-order");

  const bool cleared = cache().ClearData(
      UrlFilterType::kTrueIfMatches,
      {url::Origin::Create(GURL("https://nomatch.com:9999/"))}, {},
      base::Time(), base::Time::Max());

  EXPECT_FALSE(cleared);
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("a=1"));
}

std::optional<NoVarySearchCache> TestPickleRoundTrip(
    const NoVarySearchCache& cache) {
  base::Pickle pickle;
  WriteToPickle(pickle, cache);
  // The estimate of PickleSize should always be correct.
  EXPECT_EQ(EstimatePickleSize(cache), pickle.payload_size());
  auto maybe_cache = ReadValueFromPickle<NoVarySearchCache>(pickle);
  if (!maybe_cache) {
    return std::nullopt;
  }

  EXPECT_EQ(cache.size(), maybe_cache->size());
  return maybe_cache;
}

TEST_P(NoVarySearchCacheTest, SerializeDeserializeEmpty) {
  EXPECT_TRUE(TestPickleRoundTrip(cache()));
}

TEST_P(NoVarySearchCacheTest, SerializeDeserializeSimple) {
  Insert("b=1", "key-order");
  Insert("c&d", "key-order");
  Insert("f=3", "params=(\"a\")");

  auto new_cache = TestPickleRoundTrip(cache());
  ASSERT_TRUE(new_cache);

  const auto lookup = [&](std::string_view params) {
    return new_cache->Lookup(TestRequest(params));
  };

  auto maybe_handle1 = lookup("b=1");
  auto maybe_handle2 = lookup("d&c");
  auto maybe_handle3 = lookup("f=3&a=7");

  ASSERT_TRUE(maybe_handle1);
  ASSERT_TRUE(maybe_handle2);
  ASSERT_TRUE(maybe_handle3);

  new_cache->Erase(std::move(maybe_handle1->erase_handle));
  new_cache->Erase(std::move(maybe_handle2->erase_handle));
  new_cache->Erase(std::move(maybe_handle3->erase_handle));

  EXPECT_EQ(new_cache->size(), 0u);
  EXPECT_TRUE(new_cache->IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, SerializeDeserializeFull) {
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(QueryWithIParameter(i), kVaryOnIParameter);
  }

  auto new_cache = TestPickleRoundTrip(cache());
  ASSERT_TRUE(new_cache);

  for (size_t i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(new_cache->Lookup(TestRequest(QueryWithIParameter(i))));
  }
}

TEST_P(NoVarySearchCacheTest, DeserializeBadSizes) {
  struct TestCase {
    std::string_view test_description;
    int size;
    int max_size;
    int map_size;
  };
  static constexpr auto kTestCases = std::to_array<TestCase>({
      {"Negative size", -1, 1, 0},
      {"Size larger than max_size", 2, 1, 0},
      {"Size bigger than map contents", 1, 1, 0},
      {"Negative max_size", 0, -1, 0},
      {"Zero max_size", 0, 0, 0},
      {"Negative map size", 0, 1, -1},
      {"Map size larger than map contents", 0, 1, 1},
  });

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.test_description);
    base::Pickle pickle;
    // This uses the fact that containers use an integer for size.
    WriteToPickle(pickle, test_case.size, test_case.max_size,
                  test_case.map_size);
    EXPECT_FALSE(ReadValueFromPickle<NoVarySearchCache>(pickle));
  }
}

// A truncated Pickle should never deserialize to a NoVarySearchCache object.
// This tests covers many different checks for bad data during deserialization.
TEST_P(NoVarySearchCacheTest, TruncatedPickle) {
  Insert("a=9&b=1", "params=(\"a\")");
  Insert("a=8&b=2", "params=(\"a\")");
  Insert("f=3", "params, except=(\"f\")");
  Insert("", "params, except=(\"f\")");

  base::Pickle pickle;
  WriteToPickle(pickle, cache());

  // Go up in increments of 4 bytes because a Pickle with a size that is not a
  // multiple of 4 is invalid in a way that is not interesting to this test.
  for (size_t bytes = 4u; bytes < pickle.payload_size(); bytes += 4) {
    SCOPED_TRACE(bytes);
    base::Pickle truncated;
    truncated.WriteBytes(pickle.payload_bytes().first(bytes));
    EXPECT_FALSE(ReadValueFromPickle<NoVarySearchCache>(truncated));
  }
}

// A Journal that registers and deregisters itself automatically.
class ScopedJournal : public NoVarySearchCache::Journal {
 public:
  explicit ScopedJournal(NoVarySearchCache& cache) : cache_(cache) {
    cache.SetJournal(this);
  }

  ~ScopedJournal() override { cache_->SetJournal(nullptr); }

 private:
  raw_ref<NoVarySearchCache> cache_;
};

// A Journal object implemented by GoogleMock.
class ScopedMockJournal : public ScopedJournal {
 public:
  using ScopedJournal::ScopedJournal;

  MOCK_METHOD(void,
              OnInsert,
              (const std::string&,
               const std::string&,
               const HttpNoVarySearchData&,
               const std::optional<std::string>&,
               base::Time),
              (override));
  MOCK_METHOD(void,
              OnErase,
              (const std::string&,
               const std::string&,
               const HttpNoVarySearchData&,
               const std::optional<std::string>&),
              (override));
};

// A matcher which matches No-Vary-Search: key-order
const auto IsKeyOrder =
    Eq(HttpNoVarySearchData::CreateFromNoVaryParams({}, false));

TEST_P(NoVarySearchCacheTest, JournalNewInsert) {
  ScopedMockJournal journal(cache());

  const base::Time now = base::Time::Now();

  EXPECT_CALL(journal, OnInsert(_, "https://example.com/", IsKeyOrder,
                                Optional(Eq("a=0")), Ge(now)));

  Insert("a=0", "key-order");
}

TEST_P(NoVarySearchCacheTest, JournalRefresh) {
  Insert("a=1", "key-order");

  // Start journalling now.
  ScopedMockJournal journal(cache());

  const base::Time now = base::Time::Now();

  EXPECT_CALL(journal, OnInsert(_, "https://example.com/", IsKeyOrder,
                                Optional(Eq("a=1")), Ge(now)));

  Insert("a=1", "key-order");
}

TEST_P(NoVarySearchCacheTest, JournalReplacement) {
  Insert("a=2&k=1", "params=(\"k\")");

  ScopedMockJournal journal(cache());

  const auto params_k =
      HttpNoVarySearchData::CreateFromNoVaryParams({"k"}, true);

  const base::Time now = base::Time::Now();

  EXPECT_CALL(journal, OnInsert(_, "https://example.com/", Eq(params_k),
                                Optional(Eq("a=2&k=2")), Ge(now)));
  EXPECT_CALL(journal, OnErase).Times(0);

  // This one replaces the one inserted earlier, but OnErase() is not called to
  // reflect that the old one was removed.
  Insert("a=2&k=2", "params=(\"k\")");
}

TEST_P(NoVarySearchCacheTest, JournalErase) {
  Insert("a=3", "key-order");

  auto [original_url, erase_handle] =
      cache().Lookup(TestRequest("a=3")).value();

  ScopedMockJournal journal(cache());

  EXPECT_CALL(journal, OnErase(_, "https://example.com/", IsKeyOrder,
                               Optional(Eq("a=3"))));

  cache().Erase(std::move(erase_handle));
}

TEST_P(NoVarySearchCacheTest, DontJournalEviction) {
  ScopedMockJournal journal(cache());

  EXPECT_CALL(journal, OnInsert(_, "https://example.com/", _, _, _))
      .Times(kMaxSize + 1);

  // Eviction does not result in a call to OnErase().
  EXPECT_CALL(journal, OnErase).Times(0);

  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    Insert(QueryWithIParameter(i), "key-order");
  }
}

TEST_P(NoVarySearchCacheTest, DontJournalNonInsertion) {
  ScopedMockJournal journal(cache());

  EXPECT_CALL(journal, OnInsert).Times(0);

  // This No-Vary-Search value is equivalent to the default, so doesn't get
  // inserted into the cache.
  Insert("a=5", "params=()");
}

TEST_P(NoVarySearchCacheTest, DontJournalClearData) {
  Insert("a=6", "key-order");

  ScopedMockJournal journal(cache());

  EXPECT_CALL(journal, OnErase).Times(0);

  // Matches everything.
  cache().ClearData(UrlFilterType::kFalseIfMatches, {}, {}, base::Time(),
                    base::Time::Max());
}

TEST_P(NoVarySearchCacheTest, DontJournalLookup) {
  Insert("a=6", "key-order");

  ScopedMockJournal journal(cache());

  EXPECT_CALL(journal, OnInsert).Times(0);
  EXPECT_CALL(journal, OnErase).Times(0);

  cache().Lookup(TestRequest("a=6"));
}

// A Journal that clones all changes into a target NoVarySearchCache object.
class CloningJournal : public ScopedJournal {
 public:
  CloningJournal(NoVarySearchCache& source, NoVarySearchCache& target)
      : ScopedJournal(source), target_(target) {}

  void OnInsert(const std::string& partition_key,
                const std::string& base_url,
                const HttpNoVarySearchData& nvs_data,
                const std::optional<std::string>& query,
                base::Time update_time) override {
    target_->ReplayInsert(partition_key, base_url, nvs_data, query,
                          update_time);
  }

  // Called when an entry is erased by the Erase() method.
  void OnErase(const std::string& partition_key,
               const std::string& base_url,
               const HttpNoVarySearchData& nvs_data,
               const std::optional<std::string>& query) override {
    target_->ReplayErase(partition_key, base_url, nvs_data, query);
  }

 private:
  raw_ref<NoVarySearchCache> target_;
};

struct CloneMaker {
  NoVarySearchCache clone;
  CloningJournal journal;

  explicit CloneMaker(NoVarySearchCache& source)
      : clone(kMaxSize), journal(source, clone) {}
};

class NoVarySearchCacheReplayTest : public NoVarySearchCacheTest {
 protected:
  struct ReplayTestCase {
    std::string_view description;
    HttpRequestInfo to_insert;
    std::string_view no_vary_search_value;
    HttpRequestInfo to_lookup;
  };

  auto ReplayTestCases() {
    return std::to_array<ReplayTestCase>({
        {
            "Simple key-order",
            TestRequest("c=2&a=1"),
            "key-order",
            TestRequest("a=1&c=2"),
        },
        {
            "Different no-vary-search",
            TestRequest("d=3&e=5"),
            "params=(\"d\")",
            TestRequest("e=5"),
        },
        {
            "Different base URL",
            TestRequest(GURL("https://www.example.com/other?c=2&a=1")),
            "key-order",
            TestRequest(GURL("https://www.example.com/other?a=1&c=2")),
        },
        {
            "Different site",
            TestRequest(GURL("https://example.example/?c=2&a=1")),
            "key-order",
            TestRequest(GURL("https://example.example/?a=1&c=2")),
        },
        {
            "No question mark",
            TestRequest(GURL("https://example2.example/")),
            "params",
            TestRequest(GURL("https://example2.example/?q=ignored")),
        },
    });
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         NoVarySearchCacheReplayTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SplitCacheEnabled"
                                             : "SplitCacheDisabled";
                         });

TEST_P(NoVarySearchCacheReplayTest, Inserts) {
  const auto test_cases = ReplayTestCases();
  CloneMaker maker(cache());

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    cache().MaybeInsert(to_insert, TestHeaders(no_vary_search_value));
  }

  EXPECT_EQ(maker.clone.size(), test_cases.size());

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    SCOPED_TRACE(description);
    auto source_lookup_result = cache().Lookup(to_lookup);
    auto target_lookup_result = maker.clone.Lookup(to_lookup);

    ASSERT_TRUE(source_lookup_result);
    ASSERT_TRUE(target_lookup_result);
    EXPECT_EQ(source_lookup_result->original_url,
              target_lookup_result->original_url);
  }
}

TEST_P(NoVarySearchCacheReplayTest, Erases) {
  const auto test_cases = ReplayTestCases();
  CloneMaker maker(cache());

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    cache().MaybeInsert(to_insert, TestHeaders(no_vary_search_value));
  }

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    SCOPED_TRACE(description);
    auto source_lookup_result = cache().Lookup(to_lookup);
    ASSERT_TRUE(source_lookup_result);

    cache().Erase(std::move(source_lookup_result->erase_handle));

    EXPECT_FALSE(maker.clone.Lookup(to_lookup));
  }

  EXPECT_EQ(maker.clone.size(), 0u);
  EXPECT_TRUE(maker.clone.IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, ReplayInsertBadURLs) {
  struct TestCase {
    std::string_view description;
    std::string_view bad_url;
  };
  static constexpr auto kBadURLs = std::to_array<TestCase>({
      {"Invalid URL", ":"},
      {"Not canonical", "https://example%2Eexample/"},
      {"No path", "what:"},
      {"Has username", "https://bob@example.example/"},
      {"Has password", "https://:pass@example.example/"},
      {"Has query", "https://example.example/?"},
      {"Has ref", "https://example.example/#water"},
  });
  static constexpr std::string_view kRealURL = "https://example.example/test";
  const std::string partition_key = GenerateCachePartitionKey(kRealURL);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  for (const auto& [description, bad_url] : kBadURLs) {
    SCOPED_TRACE(description);
    cache().ReplayInsert(partition_key, std::string(bad_url), nvs_data, query,
                         update_time);
    EXPECT_EQ(cache().size(), 0u);
  }
}

TEST_P(NoVarySearchCacheTest, ReplayInsertBadQuery) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, "t=1#what",
                       update_time);
  EXPECT_EQ(cache().size(), 0u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseSuccess) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, query,
                       update_time);

  cache().ReplayErase(partition_key, std::string(kUrl), nvs_data, query);
  EXPECT_EQ(cache().size(), 0u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseOnEmptyCache) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  cache().ReplayErase(partition_key, std::string(kUrl), nvs_data, "t=1");
  EXPECT_EQ(cache().size(), 0u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseMismatchedPartition) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, query,
                       update_time);

  cache().ReplayErase(partition_key + ".", std::string(kUrl), nvs_data, query);
  EXPECT_EQ(cache().size(), 1u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseMismatchedBaseUrl) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, query,
                       update_time);

  cache().ReplayErase(partition_key, std::string(kUrl) + ".", nvs_data, query);
  EXPECT_EQ(cache().size(), 1u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseMismatchedNVSData) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, query,
                       update_time);

  const auto mismatched_nvs_data =
      HttpNoVarySearchData::CreateFromNoVaryParams({"z"}, true);
  cache().ReplayErase(partition_key, std::string(kUrl), mismatched_nvs_data,
                      query);
  EXPECT_EQ(cache().size(), 1u);
}

TEST_P(NoVarySearchCacheTest, ReplayEraseMismatchedQuery) {
  static constexpr std::string_view kUrl = "https://example.example/";
  const std::string partition_key = GenerateCachePartitionKey(kUrl);
  const auto nvs_data = HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  const std::optional<std::string> query = "t=1";
  const base::Time update_time;
  cache().ReplayInsert(partition_key, std::string(kUrl), nvs_data, query,
                       update_time);

  const std::optional<std::string> mismatched_query = "t=2";
  cache().ReplayErase(partition_key, std::string(kUrl), nvs_data,
                      mismatched_query);
  EXPECT_EQ(cache().size(), 1u);
}

// This test doesn't actually cover the Replay methods, but uses the same data
// set for convenience.
TEST_P(NoVarySearchCacheReplayTest, MergeFrom) {
  const auto test_cases = ReplayTestCases();

  const base::Time before_inserts = base::Time::Now();

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    cache().MaybeInsert(to_insert, TestHeaders(no_vary_search_value));
  }

  const base::Time after_inserts = base::Time::Now();

  NoVarySearchCache target(kMaxSize);
  ScopedMockJournal journal(target);

  EXPECT_CALL(journal, OnErase).Times(0);

  {
    InSequence s;
    for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
         test_cases) {
      auto expected_nvs_data = HttpNoVarySearchData::ParseFromHeaders(
          TestHeaders(no_vary_search_value));
      const GURL& url = to_insert.url;
      std::optional<std::string_view> query;
      if (url.has_query()) {
        query = url.query();
      }
      std::string base_url = url.spec();
      if (size_t pos = base_url.find('?'); pos != std::string::npos) {
        base_url = base_url.substr(0, pos);
      }
      EXPECT_CALL(journal,
                  OnInsert(_, Eq(base_url), Eq(expected_nvs_data), Eq(query),
                           AllOf(Ge(before_inserts), Le(after_inserts))));
    }
  }

  target.MergeFrom(cache());

  EXPECT_EQ(cache().size(), target.size());

  for (const auto& [description, to_insert, no_vary_search_value, to_lookup] :
       test_cases) {
    SCOPED_TRACE(description);
    auto source_lookup_result = cache().Lookup(to_lookup);
    auto target_lookup_result = target.Lookup(to_lookup);

    ASSERT_TRUE(source_lookup_result);
    ASSERT_TRUE(target_lookup_result);
    EXPECT_EQ(source_lookup_result->original_url,
              target_lookup_result->original_url);
  }
}

TEST_P(NoVarySearchCacheReplayTest, MergeFromTargetQueriesConsideredOlder) {
  const auto query = QueryWithIParameter;
  NoVarySearchCache target(kMaxSize);

  // Fill the target cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    target.MaybeInsert(TestRequest(query(i)), TestHeaders(kVaryOnIParameter));
  }

  // Put one entry in the source cache.
  cache().MaybeInsert(TestRequest(query(kMaxSize)),
                      TestHeaders(kVaryOnIParameter));

  target.MergeFrom(cache());

  EXPECT_EQ(target.size(), kMaxSize);

  // i=0 has been evicted.
  EXPECT_FALSE(target.Lookup(TestRequest(query(0u))));
}

TEST_P(NoVarySearchCacheReplayTest, LRUOrderPreserved) {
  const auto query = QueryWithIParameter;
  NoVarySearchCache target(kMaxSize);

  // Fill the source cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(query(i), kVaryOnIParameter);
  }

  // Make i=1 be most recently used.
  EXPECT_TRUE(Exists(query(1u)));

  // Merge to target cache.
  target.MergeFrom(cache());

  int next_i = kMaxSize;
  const auto expect_to_evict = [&](size_t i) {
    target.MaybeInsert(TestRequest(query(next_i)),
                       TestHeaders(kVaryOnIParameter));
    EXPECT_FALSE(target.Lookup(TestRequest(query(i))));
    ++next_i;
  };

  // Evict i=0.
  expect_to_evict(0u);

  // Evict i=2 to i=kMaxSize-1.
  for (size_t i = 2; i < kMaxSize; ++i) {
    expect_to_evict(i);
  }

  // Evict i=1.
  expect_to_evict(1u);
}

TEST_P(NoVarySearchCacheTest, SetMaxSizeSame) {
  Insert("a=1", "key-order");
  Insert("a=2", "key-order");
  ASSERT_EQ(cache().size(), 2u);
  ASSERT_EQ(cache().max_size(), kMaxSize);

  cache().SetMaxSize(kMaxSize);

  EXPECT_EQ(cache().size(), 2u);
  EXPECT_EQ(cache().max_size(), kMaxSize);
  EXPECT_TRUE(Exists("a=1"));
  EXPECT_TRUE(Exists("a=2"));
}

TEST_P(NoVarySearchCacheTest, SetMaxSizeSmaller) {
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(QueryWithIParameter(i), "key-order");
  }
  ASSERT_EQ(cache().size(), kMaxSize);

  cache().SetMaxSize(kMaxSize - 2);

  EXPECT_EQ(cache().size(), kMaxSize - 2);
  EXPECT_EQ(cache().max_size(), kMaxSize - 2);

  // The two least recently used items should be evicted.
  EXPECT_FALSE(Exists("i=0"));
  EXPECT_FALSE(Exists("i=1"));
  EXPECT_TRUE(Exists("i=2"));
  EXPECT_TRUE(Exists("i=3"));
  EXPECT_TRUE(Exists("i=4"));
}

TEST_P(NoVarySearchCacheTest, SetMaxSizeLarger) {
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(QueryWithIParameter(i), "key-order");
  }
  ASSERT_EQ(cache().size(), kMaxSize);

  cache().SetMaxSize(kMaxSize + 2);

  EXPECT_EQ(cache().size(), kMaxSize);
  EXPECT_EQ(cache().max_size(), kMaxSize + 2);

  // All original items should still be there.
  for (size_t i = 0; i < kMaxSize; ++i) {
    EXPECT_TRUE(Exists(QueryWithIParameter(i)));
  }

  // Add two more items.
  Insert(QueryWithIParameter(kMaxSize), "key-order");
  Insert(QueryWithIParameter(kMaxSize + 1), "key-order");

  EXPECT_EQ(cache().size(), kMaxSize + 2);
  EXPECT_TRUE(Exists(QueryWithIParameter(kMaxSize)));
  EXPECT_TRUE(Exists(QueryWithIParameter(kMaxSize + 1)));
}

TEST_P(NoVarySearchCacheTest, SetMaxSizeOnEmptyCache) {
  ASSERT_EQ(cache().size(), 0u);
  cache().SetMaxSize(kMaxSize + 5);
  EXPECT_EQ(cache().size(), 0u);
  EXPECT_EQ(cache().max_size(), kMaxSize + 5);
}

// TODO(https://crbug.com/390216627): Test the various experiments that affect
// the cache key and make sure they also affect NoVarySearchCache.

}  // namespace

}  // namespace net
