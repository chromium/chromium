// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

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
    GURL url("https://example.com/");
    if (query.empty()) {
      return url;
    }

    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    return url.ReplaceComponents(replacements);
  }

  // Generates an HttpRequestInfo object containing a URL that has the query
  // `query`.
  static HttpRequestInfo TestRequest(std::string_view query = {}) {
    return TestRequest(TestURL(query));
  }

  // Generates an HttpRequestInfo object with the URL `url`.
  static HttpRequestInfo TestRequest(const GURL& url) {
    SchemefulSite site(url);
    return TestRequest(url, NetworkIsolationKey(site, site));
  }

  // Generates an HttpRequestInfo object with the given `url` and `nik`.
  static HttpRequestInfo TestRequest(const GURL& url,
                                     const NetworkIsolationKey& nik) {
    // Only fill in the fields that GenerateCacheKeyForRequest() looks at.
    HttpRequestInfo request;
    request.url = url;
    request.network_isolation_key = nik;
    request.is_subframe_document_resource = false;
    request.is_main_frame_navigation = true;
    CHECK(!request.upload_data_stream);
    request.load_flags = LOAD_NORMAL;
    CHECK(!request.initiator);
    return request;
  }

  // Returns a reference to an HttpResponseHeaders object with a No-Vary-Search
  // header with the value `no_vary_search`.
  const HttpResponseHeaders& TestHeaders(std::string_view no_vary_search) {
    response_header_storage_.push_back(
        HttpResponseHeaders::Builder({1, 1}, "200 OK")
            .AddHeader("No-Vary-Search", no_vary_search)
            .Build());
    return *response_header_storage_.back();
  }

  // Inserts a URL with query `query` into cache with a No-Vary-Search header
  // value of `no_vary_search`.
  void Insert(std::string_view query, std::string_view no_vary_search) {
    cache_.MaybeInsert(TestRequest(query), TestHeaders(no_vary_search));
  }

  // Returns true if TestURL(query) exists in cache.
  bool Exists(std::string_view query) {
    return cache_.Lookup(TestRequest(query)).has_value();
  }

  // Returns true if inserting a request with query `insert` results in a lookup
  // for query `lookup` succeeding, assuming a No-Vary-Search header value of
  // `no_vary_search`.
  bool Matches(std::string_view insert,
               std::string_view lookup,
               std::string_view no_vary_search) {
    NoVarySearchCache cache(kMaxSize);

    cache.MaybeInsert(TestRequest(insert), TestHeaders(no_vary_search));
    EXPECT_EQ(cache.GetSizeForTesting(), 1u);

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
    EXPECT_EQ(cache.GetSizeForTesting(), 1u);
    insert(insert2);
    return cache.GetSizeForTesting() == 1u;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  NoVarySearchCache cache_{kMaxSize};

  // Scratch space for HttpResponseHeaders objects so that TestHeaders() can
  // return a const reference for convenience. This is thrown away at the end of
  // each test.
  std::vector<scoped_refptr<HttpResponseHeaders>> response_header_storage_;
};

TEST_P(NoVarySearchCacheTest, NewlyConstructedCacheIsEmpty) {
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
}

TEST_P(NoVarySearchCacheTest, LookupOnEmptyCache) {
  EXPECT_EQ(cache().Lookup(TestRequest()), std::nullopt);
}

TEST_P(NoVarySearchCacheTest, InsertLookupErase) {
  Insert("", "key-order");

  auto result = cache().Lookup(TestRequest());
  ASSERT_TRUE(result);
  EXPECT_EQ(result->original_url, TestURL());

  EXPECT_EQ(cache().GetSizeForTesting(), 1u);

  cache().Erase(std::move(result->erase_handle));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
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

TEST_P(NoVarySearchCacheTest, OldestItemIsEvicted) {
  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    std::string query = "i=" + base::NumberToString(i);
    Insert(query, "params, except=(\"i\")");
    EXPECT_TRUE(Exists(query));
  }

  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  EXPECT_FALSE(Exists("i=0"));
}

TEST_P(NoVarySearchCacheTest, RecentlyUsedItemIsNotEvicted) {
  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    std::string query = "i=" + base::NumberToString(i);
    Insert(query, "params, except=(\"i\")");
    EXPECT_TRUE(Exists(query));
    // Exists() calls Lookup(), which makes an entry "used".
    EXPECT_TRUE(Exists("i=0"));
  }

  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  EXPECT_TRUE(Exists("i=0"));
  EXPECT_FALSE(Exists("i=1"));
}

TEST_P(NoVarySearchCacheTest, MostRecentlyUsedItemIsNotEvicted) {
  static constexpr char kNoVarySearchValue[] = "params, except=(\"i\")";
  const auto query = [](int i) { return "i=" + base::NumberToString(i); };
  // Fill the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(query(i), kNoVarySearchValue);
  }
  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  // Make "i=3" be the most recently used item.
  EXPECT_TRUE(Exists("i=3"));

  // Evict kMaxSize - 1 items.
  for (size_t i = kMaxSize; i < kMaxSize * 2 - 1; ++i) {
    Insert(query(i), kNoVarySearchValue);
    EXPECT_TRUE(Exists(query(i)));
  }

  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  EXPECT_TRUE(Exists("i=3"));
}

TEST_P(NoVarySearchCacheTest, LeastRecentlyUsedItemIsEvicted) {
  static constexpr char kNoVarySearchValue[] = "params, except=(\"i\")";
  const auto query = [](int i) { return "i=" + base::NumberToString(i); };
  // Fill the cache.
  for (size_t i = 0; i < kMaxSize; ++i) {
    Insert(query(i), kNoVarySearchValue);
  }
  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  // Make "i=kMaxSize-1" be the least recently used item.
  for (size_t i = 0; i < kMaxSize - 1; ++i) {
    EXPECT_TRUE(Exists(query(i)));
  }

  // Evict one item.
  Insert(query(kMaxSize), kNoVarySearchValue);

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
  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
  auto new_result = cache().Lookup(TestRequest("a=b"));
  EXPECT_EQ(new_result->original_url, TestURL("a=b&c=2"));
}

TEST_P(NoVarySearchCacheTest, MaybeInsertDoesNothingWithNoNoVarySearchHeader) {
  auto headers = HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  cache().MaybeInsert(TestRequest(), *headers);
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, MaybeInsertDoesNothingForDefaultBehavior) {
  // The following header values are all equivalent to default behavior.
  static constexpr auto kDefaultCases = std::to_array<std::string_view>(
      {"", " ", "params=?0", "params=()", "key-order=?0", "nonsense"});

  for (auto no_vary_search : kDefaultCases) {
    NoVarySearchCache cache(kMaxSize);

    Insert("a=b", no_vary_search);
    EXPECT_EQ(cache.GetSizeForTesting(), 0u) << no_vary_search;
  }
}

// A size 1 cache is a special case, because eviction results in an empty cache.
TEST_P(NoVarySearchCacheTest, EvictWithSize1Cache) {
  NoVarySearchCache cache(1u);
  cache.MaybeInsert(TestRequest("a=1"), TestHeaders("key-order"));
  cache.MaybeInsert(TestRequest("a=2"), TestHeaders("key-order"));
  EXPECT_TRUE(cache.Lookup(TestRequest("a=2")).has_value());
  EXPECT_EQ(cache.GetSizeForTesting(), 1u);
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
    std::string query = "i=" + base::NumberToString(i);
    Insert(query, "params, except=(\"i\")");
  }
  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

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
    std::string query = "i=" + base::NumberToString(i);
    Insert(query, "params, except=(\"i\")");
  }
  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

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
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
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
    EXPECT_EQ(cache.GetSizeForTesting(), 1u);
    cache.MaybeInsert(TestRequest(kQuery), TestHeaders(variant2));
    EXPECT_EQ(cache.GetSizeForTesting(), 1u)
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
    EXPECT_EQ(cache().GetSizeForTesting(), 0u);
    EXPECT_FALSE(cache().Lookup(TestRequest(TestURL(), transient)));
  } else {
    EXPECT_EQ(cache().GetSizeForTesting(), 1u);
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

  const size_t cache_size = cache().GetSizeForTesting();
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
  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
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

  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
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

  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
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

  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

// The URL "ref", also known as the "fragment", also known as the "hash", is
// ignored for matching and not stored in the cache.
TEST_P(NoVarySearchCacheTest, URLRefIsIgnored) {
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b#foo")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b#bar")),
                      TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
  auto result =
      cache().Lookup(TestRequest(GURL("https://example.com/?a=b#baz")));
  EXPECT_TRUE(result);
  EXPECT_EQ(result->original_url, GURL("https://example.com/?a=b"));
}

TEST_P(NoVarySearchCacheTest, URLWithUsernameIsRejected) {
  const GURL url_with_username("https://me@example.com/?a=b");
  cache().MaybeInsert(TestRequest(url_with_username), TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);

  // See if it matches against the URL without the username.
  cache().MaybeInsert(TestRequest(GURL("https://example.com/?a=b")),
                      TestHeaders("key-order"));
  EXPECT_FALSE(cache().Lookup(TestRequest(url_with_username)));
}

TEST_P(NoVarySearchCacheTest, URLWithPasswordIsRejected) {
  const GURL url_with_password("https://:hunter123@example.com/?a=b");
  cache().MaybeInsert(TestRequest(url_with_password), TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);

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

  cache().ClearData(UrlFilterType::kFalseIfMatches, {}, {}, base::Time(),
                    base::Time::Max());

  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_P(NoVarySearchCacheTest, ClearDataMatchOrigin) {
  // Scheme differs.
  cache().MaybeInsert(TestRequest(GURL("https://example.com/q?a=b")),
                      TestHeaders("key-order"));
  cache().MaybeInsert(TestRequest(GURL("http://example.com/q?a=b")),
                      TestHeaders("key-order"));

  cache().ClearData(UrlFilterType::kTrueIfMatches,
                    {url::Origin::Create(GURL("https://example.com/"))}, {},
                    base::Time(), base::Time::Max());

  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
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

  cache().ClearData(UrlFilterType::kTrueIfMatches, {}, {"example.com"},
                    base::Time(), base::Time::Max());

  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
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

  cache().ClearData(UrlFilterType::kFalseIfMatches, {}, {}, time("12:30:00"),
                    time("13:30:00"));

  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
  EXPECT_TRUE(Exists("a=1"));
  EXPECT_FALSE(Exists("a=2"));
  EXPECT_TRUE(Exists("a=3"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NoVarySearchCacheTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SplitCacheEnabled"
                                             : "SplitCacheDisabled";
                         });

// TODO(https://crbug.com/390216627): Test the various experiments that affect
// the cache key and make sure they also affect NoVarySearchCache.

}  // namespace

}  // namespace net
