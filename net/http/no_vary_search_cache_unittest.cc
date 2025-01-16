// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr size_t kMaxSize = 5;

GURL TestURL(std::string_view query = {}) {
  GURL url("https://example.com/");
  if (query.empty()) {
    return url;
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

NetworkIsolationKey TestNIK() {
  SchemefulSite site(TestURL());
  return NetworkIsolationKey(site, site);
}

scoped_refptr<HttpResponseHeaders> TestHeaders(
    std::string_view no_vary_search) {
  return HttpResponseHeaders::Builder({1, 1}, "200 OK")
      .AddHeader("No-Vary-Search", no_vary_search)
      .Build();
}

class NoVarySearchCacheTest : public ::testing::Test {
 protected:
  NoVarySearchCache& cache() { return cache_; }

  // Inserts a URL with query `query` into cache with a No-Vary-Search header
  // value of `no_vary_search`.
  void Insert(std::string_view query, std::string_view no_vary_search) {
    cache_.MaybeInsert(TestNIK(), TestURL(query), *TestHeaders(no_vary_search));
  }

  // Returns true if TestURL(query) exists in cache.
  bool Exists(std::string_view query) {
    return cache_.Lookup(TestNIK(), TestURL(query)).has_value();
  }

 private:
  NoVarySearchCache cache_{kMaxSize};
};

TEST_F(NoVarySearchCacheTest, NewlyConstructedCacheIsEmpty) {
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
}

TEST_F(NoVarySearchCacheTest, LookupOnEmptyCache) {
  EXPECT_EQ(cache().Lookup(TestNIK(), TestURL()), std::nullopt);
}

TEST_F(NoVarySearchCacheTest, InsertLookupErase) {
  Insert("", "key-order");

  auto result = cache().Lookup(TestNIK(), TestURL());
  ASSERT_TRUE(result);
  EXPECT_EQ(result->original_url, TestURL());

  EXPECT_EQ(cache().GetSizeForTesting(), 1u);

  cache().Erase(std::move(result->erase_handle));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

// An asan build will find leaks, but this test works on any build.
TEST_F(NoVarySearchCacheTest, QueryNotLeaked) {
  std::optional<NoVarySearchCache::LookupResult> result;
  {
    NoVarySearchCache cache(kMaxSize);

    cache.MaybeInsert(TestNIK(), TestURL(), *TestHeaders("params"));
    result = cache.Lookup(TestNIK(), TestURL());
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->erase_handle.IsGoneForTesting());
  }
  EXPECT_TRUE(result->erase_handle.IsGoneForTesting());
}

TEST_F(NoVarySearchCacheTest, OldestItemIsEvicted) {
  for (size_t i = 0; i < kMaxSize + 1; ++i) {
    std::string query = "i=" + base::NumberToString(i);
    Insert(query, "params, except=(\"i\")");
    EXPECT_TRUE(Exists(query));
  }

  EXPECT_EQ(cache().GetSizeForTesting(), kMaxSize);

  EXPECT_FALSE(Exists("i=0"));
}

TEST_F(NoVarySearchCacheTest, RecentlyUsedItemIsNotEvicted) {
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

TEST_F(NoVarySearchCacheTest, MostRecentlyUsedItemIsNotEvicted) {
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

TEST_F(NoVarySearchCacheTest, LeastRecentlyUsedItemIsEvicted) {
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

TEST_F(NoVarySearchCacheTest, InsertUpdatesIdenticalItem) {
  Insert("a=b", "params=(\"c\")");
  auto original_result = cache().Lookup(TestNIK(), TestURL("a=b"));
  ASSERT_TRUE(original_result);
  Insert("a=b", "params=(\"c\")");
  auto new_result = cache().Lookup(TestNIK(), TestURL("a=b"));
  ASSERT_TRUE(new_result);
  EXPECT_TRUE(
      original_result->erase_handle.EqualsForTesting(new_result->erase_handle));
}

TEST_F(NoVarySearchCacheTest, InsertRemovesMatchingItem) {
  Insert("a=b&c=1", "params=(\"c\")");
  auto original_result = cache().Lookup(TestNIK(), TestURL("a=b"));
  ASSERT_TRUE(original_result);
  EXPECT_EQ(original_result->original_url, TestURL("a=b&c=1"));
  Insert("a=b&c=2", "params=(\"c\")");
  EXPECT_TRUE(original_result->erase_handle.IsGoneForTesting());
  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
  auto new_result = cache().Lookup(TestNIK(), TestURL("a=b"));
  EXPECT_EQ(new_result->original_url, TestURL("a=b&c=2"));
}

TEST_F(NoVarySearchCacheTest, MaybeInsertDoesNothingWithNoNoVarySearchHeader) {
  auto headers = HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  cache().MaybeInsert(TestNIK(), TestURL(), *headers);
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_TRUE(cache().IsTopLevelMapEmptyForTesting());
}

TEST_F(NoVarySearchCacheTest, MaybeInsertDoesNothingForDefaultBehavior) {
  // The following header values are all equivalent to default behavior.
  static constexpr auto kDefaultCases = std::to_array<std::string_view>(
      {"", " ", "params=?0", "params=()", "key-order=?0", "nonsense"});

  for (auto no_vary_search : kDefaultCases) {
    NoVarySearchCache cache(kMaxSize);

    Insert("a=b", no_vary_search);
    EXPECT_EQ(cache.GetSizeForTesting(), 0u) << no_vary_search;
  }
}

TEST_F(NoVarySearchCacheTest, MatchesWithoutQueryString) {
  auto url_with_query = GURL("https://example.com/foo?");
  cache().MaybeInsert(TestNIK(), url_with_query, *TestHeaders("key-order"));
  auto result = cache().Lookup(TestNIK(), GURL("https://example.com/foo"));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->original_url, url_with_query);
}

TEST_F(NoVarySearchCacheTest, InsertInvalidURLIsIgnored) {
  auto invalid_url = GURL("???");
  ASSERT_FALSE(invalid_url.is_valid());
  cache().MaybeInsert(TestNIK(), invalid_url, *TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
}

// There's no way to insert an invalid URL into the cache. There's also no way
// to add a query to a valid URL to make it invalid. So this test just verifies
// that we don't crash.
TEST_F(NoVarySearchCacheTest, LookupInvalidURLReturnsNullopt) {
  GURL invalid_url = GURL("???");
  ASSERT_FALSE(invalid_url.is_valid());
  auto result = cache().Lookup(TestNIK(), invalid_url);
  EXPECT_FALSE(result);
}

bool Matches(std::string_view insert,
             std::string_view lookup,
             std::string_view no_vary_search) {
  NoVarySearchCache cache(kMaxSize);

  cache.MaybeInsert(TestNIK(), TestURL(insert), *TestHeaders(no_vary_search));
  EXPECT_EQ(cache.GetSizeForTesting(), 1u);

  const auto exists = [&cache](std::string_view query) {
    return cache.Lookup(TestNIK(), TestURL(query)).has_value();
  };

  // It would be bad if the query didn't match itself.
  EXPECT_TRUE(exists(insert));

  return exists(lookup);
}

bool InsertMatches(std::string_view insert1,
                   std::string_view insert2,
                   std::string_view no_vary_search) {
  NoVarySearchCache cache(kMaxSize);
  const auto insert = [&cache, no_vary_search](std::string_view query) {
    cache.MaybeInsert(TestNIK(), TestURL(query), *TestHeaders(no_vary_search));
  };

  insert(insert1);
  EXPECT_EQ(cache.GetSizeForTesting(), 1u);
  insert(insert2);
  return cache.GetSizeForTesting() == 1u;
}

TEST_F(NoVarySearchCacheTest, MatchCases) {
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

TEST_F(NoVarySearchCacheTest, NoMatchCases) {
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
TEST_F(NoVarySearchCacheTest, NoVarySearchVariants) {
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

    cache.MaybeInsert(TestNIK(), TestURL(kQuery), *TestHeaders(variant1));
    EXPECT_EQ(cache.GetSizeForTesting(), 1u);
    cache.MaybeInsert(TestNIK(), TestURL(kQuery), *TestHeaders(variant2));
    EXPECT_EQ(cache.GetSizeForTesting(), 1u)
        << "Failing: " << description << "; variant1='" << variant1
        << "'; variant2 = '" << variant2 << "'";
  }
}

// Items with a transient NIK will not be stored in the disk cache, and so they
// shouldn't be stored in the NoVarySearchCache either.
TEST_F(NoVarySearchCacheTest, TransientNIK) {
  const auto transient = NetworkIsolationKey::CreateTransientForTesting();

  cache().MaybeInsert(transient, TestURL(), *TestHeaders("params"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);
  EXPECT_EQ(cache().Lookup(transient, TestURL()), std::nullopt);
}

TEST_F(NoVarySearchCacheTest, DifferentNIK) {
  const NetworkIsolationKey nik1 = TestNIK();
  const NetworkIsolationKey nik2(
      SchemefulSite(TestURL()),
      SchemefulSite(GURL("https://thirdparty.example/")));

  cache().MaybeInsert(nik1, TestURL(), *TestHeaders("params"));
  cache().MaybeInsert(nik2, TestURL(), *TestHeaders("params"));
  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
  const auto result1 = cache().Lookup(nik1, TestURL());
  const auto result2 = cache().Lookup(nik2, TestURL());
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  EXPECT_FALSE(result1->erase_handle.EqualsForTesting(result2->erase_handle));
}

TEST_F(NoVarySearchCacheTest, DifferentURL) {
  const GURL url1("https://example.com/a?a=b");
  const GURL url2("https://example.com/b?a=b");

  cache().MaybeInsert(TestNIK(), url1, *TestHeaders("key-order"));
  cache().MaybeInsert(TestNIK(), url2, *TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
  const auto result1 = cache().Lookup(TestNIK(), url1);
  const auto result2 = cache().Lookup(TestNIK(), url2);
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

TEST_F(NoVarySearchCacheTest, DifferentNoVarySearch) {
  Insert("a=b&c=d", "params, except=(\"a\")");
  // Make sure that the two inserts reliably get a different `inserted`
  // timestamp so that the ordering is deterministic.
  SpinUntilCurrentTimeChanges();
  Insert("a=b", "key-order");

  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
  const auto result = cache().Lookup(TestNIK(), TestURL("a=b"));
  ASSERT_TRUE(result);
  // If time goes backwards this test will flake.
  EXPECT_EQ(result->original_url, TestURL("a=b"));
}

// The winner of the lookup should depend only on insertion order and not on the
// order of iteration of the map. To ensure this works for any iteration order,
// we perform the same test in the opposite direction.
TEST_F(NoVarySearchCacheTest, DifferentNoVarySearchReverseOrder) {
  Insert("a=b", "key-order");
  SpinUntilCurrentTimeChanges();
  Insert("a=b&c=d", "params, except=(\"a\")");

  EXPECT_EQ(cache().GetSizeForTesting(), 2u);
  const auto result = cache().Lookup(TestNIK(), TestURL("a=b"));
  ASSERT_TRUE(result);
  // If time goes backwards this test will flake.
  EXPECT_EQ(result->original_url, TestURL("a=b&c=d"));
}

TEST_F(NoVarySearchCacheTest, EraseInDifferentOrder) {
  // Insert in order a, b, c.
  Insert("a", "key-order");
  Insert("b", "key-order");
  Insert("c", "key-order");

  // Look up in order b, c, a.
  const auto lookup = [this](std::string_view query) {
    return cache().Lookup(TestNIK(), TestURL(query));
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
TEST_F(NoVarySearchCacheTest, URLRefIsIgnored) {
  cache().MaybeInsert(TestNIK(), GURL("https://example.com/?a=b#foo"),
                      *TestHeaders("key-order"));
  cache().MaybeInsert(TestNIK(), GURL("https://example.com/?a=b#bar"),
                      *TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 1u);
  auto result = cache().Lookup(TestNIK(), GURL("https://example.com/?a=b#baz"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result->original_url, GURL("https://example.com/?a=b"));
}

TEST_F(NoVarySearchCacheTest, URLWithUsernameIsRejected) {
  const GURL url_with_username("https://me@example.com/?a=b");
  cache().MaybeInsert(TestNIK(), url_with_username, *TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);

  // See if it matches against the URL without the username.
  cache().MaybeInsert(TestNIK(), GURL("https://example.com/?a=b"),
                      *TestHeaders("key-order"));
  EXPECT_FALSE(cache().Lookup(TestNIK(), url_with_username));
}

TEST_F(NoVarySearchCacheTest, URLWithPasswordIsRejected) {
  const GURL url_with_password("https://:hunter123@example.com/?a=b");
  cache().MaybeInsert(TestNIK(), url_with_password, *TestHeaders("key-order"));
  EXPECT_EQ(cache().GetSizeForTesting(), 0u);

  // See if it matches against the URL without the password.
  cache().MaybeInsert(TestNIK(), GURL("https://example.com/?a=b"),
                      *TestHeaders("key-order"));
  EXPECT_FALSE(cache().Lookup(TestNIK(), url_with_password));
}

}  // namespace

}  // namespace net
