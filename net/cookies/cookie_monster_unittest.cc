// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cookies/cookie_monster.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cookie_partition_key.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster_store_test.h"  // For CookieStore mock
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_change_unittest.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/cookies/cookie_store_unittest.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

namespace net {

using base::Time;
using CookieDeletionInfo = net::CookieDeletionInfo;

namespace {

using testing::ElementsAre;

// False means 'less than or equal', so we test both ways for full equal.
MATCHER_P(CookieEquals, expected, "") {
  return !(arg.FullCompare(expected) || expected.FullCompare(arg));
}

MATCHER_P2(MatchesCookieNameDomain, name, domain, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(testing::Property(&net::CanonicalCookie::Name, name),
                     testing::Property(&net::CanonicalCookie::Domain, domain)),
      arg, result_listener);
}

MATCHER_P4(MatchesCookieNameValueCreationExpiry,
           name,
           value,
           creation,
           expiry,
           "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Property(&net::CanonicalCookie::Name, name),
          testing::Property(&net::CanonicalCookie::Value, value),
          testing::Property(&net::CanonicalCookie::CreationDate, creation),
          // We need a margin of error when testing the ExpiryDate as, if
          // clamped, it is set relative to the current time.
          testing::Property(&net::CanonicalCookie::ExpiryDate,
                            testing::Gt(expiry - base::Minutes(1))),
          testing::Property(&net::CanonicalCookie::ExpiryDate,
                            testing::Lt(expiry + base::Minutes(1)))),
      arg, result_listener);
}

const char kTopLevelDomainPlus1[] = "http://www.harvard.edu";
const char kTopLevelDomainPlus2[] = "http://www.math.harvard.edu";
const char kTopLevelDomainPlus2Secure[] = "https://www.math.harvard.edu";
const char kTopLevelDomainPlus3[] = "http://www.bourbaki.math.harvard.edu";
const char kOtherDomain[] = "http://www.mit.edu";

struct CookieMonsterTestTraits {
  static std::unique_ptr<CookieStore> Create() {
    return std::make_unique<CookieMonster>(nullptr /* store */,
                                           nullptr /* netlog */);
  }

  static void DeliverChangeNotifications() { base::RunLoop().RunUntilIdle(); }

  static const bool supports_http_only = true;
  static const bool supports_non_dotted_domains = true;
  static const bool preserves_trailing_dots = true;
  static const bool filters_schemes = true;
  static const bool has_path_prefix_bug = false;
  static const bool forbids_setting_empty_name = false;
  static const bool supports_global_cookie_tracking = true;
  static const bool supports_url_cookie_tracking = true;
  static const bool supports_named_cookie_tracking = true;
  static const bool supports_multiple_tracking_callbacks = true;
  static const bool has_exact_change_cause = true;
  static const bool has_exact_change_ordering = true;
  static const int creation_time_granularity_in_ms = 0;
  static const bool supports_cookie_access_semantics = true;
  static const bool supports_partitioned_cookies = true;
};

INSTANTIATE_TYPED_TEST_SUITE_P(CookieMonster,
                               CookieStoreTest,
                               CookieMonsterTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieMonster,
                               CookieStoreChangeGlobalTest,
                               CookieMonsterTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieMonster,
                               CookieStoreChangeUrlTest,
                               CookieMonsterTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieMonster,
                               CookieStoreChangeNamedTest,
                               CookieMonsterTestTraits);

template <typename T>
class CookieMonsterTestBase : public CookieStoreTest<T> {
 public:
  using CookieStoreTest<T>::SetCookie;

 protected:
  using CookieStoreTest<T>::http_www_foo_;
  using CookieStoreTest<T>::https_www_foo_;

  CookieList GetAllCookiesForURLWithOptions(
      CookieMonster* cm,
      const GURL& url,
      const CookieOptions& options,
      const CookiePartitionKeyCollection& cookie_partition_key_collection =
          CookiePartitionKeyCollection()) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetCookieListWithOptionsAsync(
        url, options, cookie_partition_key_collection, callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.cookies();
  }

  CookieList GetAllCookies(CookieMonster* cm) {
    DCHECK(cm);
    GetAllCookiesCallback callback;
    cm->GetAllCookiesAsync(callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.cookies();
  }

  CookieAccessResultList GetExcludedCookiesForURLWithOptions(
      CookieMonster* cm,
      const GURL& url,
      const CookieOptions& options,
      const CookiePartitionKeyCollection& cookie_partition_key_collection =
          CookiePartitionKeyCollection()) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetCookieListWithOptionsAsync(
        url, options, cookie_partition_key_collection, callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.excluded_cookies();
  }

  bool SetAllCookies(CookieMonster* cm, const CookieList& list) {
    DCHECK(cm);
    ResultSavingCookieCallback<CookieAccessResult> callback;
    cm->SetAllCookiesAsync(list, callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result().status.IsInclude();
  }

  bool SetCookieWithCreationTime(
      CookieMonster* cm,
      const GURL& url,
      const std::string& cookie_line,
      base::Time creation_time,
      std::optional<CookiePartitionKey> cookie_partition_key = std::nullopt) {
    DCHECK(cm);
    DCHECK(!creation_time.is_null());
    ResultSavingCookieCallback<CookieAccessResult> callback;
    cm->SetCanonicalCookieAsync(
        CanonicalCookie::CreateForTesting(url, cookie_line, creation_time,
                                          std::nullopt /* server_time */,
                                          cookie_partition_key),
        url, CookieOptions::MakeAllInclusive(), callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result().status.IsInclude();
  }

  uint32_t DeleteAllCreatedInTimeRange(CookieMonster* cm,
                                       const TimeRange& creation_range) {
    DCHECK(cm);
    ResultSavingCookieCallback<uint32_t> callback;
    cm->DeleteAllCreatedInTimeRangeAsync(creation_range,
                                         callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result();
  }

  uint32_t DeleteAllMatchingInfo(CookieMonster* cm,
                                 CookieDeletionInfo delete_info) {
    DCHECK(cm);
    ResultSavingCookieCallback<uint32_t> callback;
    cm->DeleteAllMatchingInfoAsync(std::move(delete_info),
                                   callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result();
  }

  uint32_t DeleteMatchingCookies(CookieMonster* cm,
                                 CookieStore::DeletePredicate predicate) {
    DCHECK(cm);
    ResultSavingCookieCallback<uint32_t> callback;
    cm->DeleteMatchingCookiesAsync(std::move(predicate),
                                   callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result();
  }

  // Helper for PredicateSeesAllCookies test; repopulates CM with same layout
  // each time. Returns the time which is strictly greater than any creation
  // time which was passed to created cookies.
  base::Time PopulateCmForPredicateCheck(CookieMonster* cm) {
    std::string url_top_level_domain_plus_1(GURL(kTopLevelDomainPlus1).host());
    std::string url_top_level_domain_plus_2(GURL(kTopLevelDomainPlus2).host());
    std::string url_top_level_domain_plus_3(GURL(kTopLevelDomainPlus3).host());
    std::string url_top_level_domain_secure(
        GURL(kTopLevelDomainPlus2Secure).host());
    std::string url_other(GURL(kOtherDomain).host());

    this->DeleteAll(cm);

    // Static population for probe:
    //    * Three levels of domain cookie (.b.a, .c.b.a, .d.c.b.a)
    //    * Three levels of host cookie (w.b.a, w.c.b.a, w.d.c.b.a)
    //    * http_only cookie (w.c.b.a)
    //    * same_site cookie (w.c.b.a)
    //    * Two secure cookies (.c.b.a, w.c.b.a)
    //    * Two domain path cookies (.c.b.a/dir1, .c.b.a/dir1/dir2)
    //    * Two host path cookies (w.c.b.a/dir1, w.c.b.a/dir1/dir2)

    std::vector<std::unique_ptr<CanonicalCookie>> cookies;
    const base::Time now = base::Time::Now();

    // Domain cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "dom_1", "A", ".harvard.edu", "/", now, base::Time(), base::Time(),
        base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "dom_2", "B", ".math.harvard.edu", "/", now, base::Time(), base::Time(),
        base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "dom_3", "C", ".bourbaki.math.harvard.edu", "/", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));

    // Host cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "host_1", "A", url_top_level_domain_plus_1, "/", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "host_2", "B", url_top_level_domain_plus_2, "/", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "host_3", "C", url_top_level_domain_plus_3, "/", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));

    // http_only cookie
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "httpo_check", "A", url_top_level_domain_plus_2, "/", now, base::Time(),
        base::Time(), base::Time(), false, true, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));

    // same-site cookie
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "same_site_check", "A", url_top_level_domain_plus_2, "/", now,
        base::Time(), base::Time(), base::Time(), false, false,
        CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT));

    // Secure cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "sec_dom", "A", ".math.harvard.edu", "/", now, base::Time(),
        base::Time(), base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "sec_host", "B", url_top_level_domain_plus_2, "/", now, base::Time(),
        base::Time(), base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT));

    // Domain path cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "dom_path_1", "A", ".math.harvard.edu", "/dir1", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "dom_path_2", "B", ".math.harvard.edu", "/dir1/dir2", now, base::Time(),
        base::Time(), base::Time(), false, false, CookieSameSite::LAX_MODE,
        COOKIE_PRIORITY_DEFAULT));

    // Host path cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "host_path_1", "A", url_top_level_domain_plus_2, "/dir1", now,
        base::Time(), base::Time(), base::Time(), false, false,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "host_path_2", "B", url_top_level_domain_plus_2, "/dir1/dir2", now,
        base::Time(), base::Time(), base::Time(), false, false,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT));

    // Partitioned cookies
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "__Host-pc_1", "A", url_top_level_domain_secure, "/", now, base::Time(),
        base::Time(), base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
        CookiePriority::COOKIE_PRIORITY_DEFAULT,
        CookiePartitionKey::FromURLForTesting(GURL(kTopLevelDomainPlus1))));
    cookies.push_back(CanonicalCookie::CreateUnsafeCookieForTesting(
        "__Host-pc_2", "B", url_top_level_domain_secure, "/", now, base::Time(),
        base::Time(), base::Time(), true, false, CookieSameSite::NO_RESTRICTION,
        CookiePriority::COOKIE_PRIORITY_DEFAULT,
        CookiePartitionKey::FromURLForTesting(GURL(kTopLevelDomainPlus1))));

    for (auto& cookie : cookies) {
      GURL source_url = cookie_util::SimulatedCookieSource(
          *cookie, cookie->SecureAttribute() ? "https" : "http");
      EXPECT_TRUE(this->SetCanonicalCookie(cm, std::move(cookie), source_url,
                                           true /* modify_httponly */));
    }

    EXPECT_EQ(cookies.size(), this->GetAllCookies(cm).size());
    return now + base::Milliseconds(100);
  }

  Time GetFirstCookieAccessDate(CookieMonster* cm) {
    const CookieList all_cookies(this->GetAllCookies(cm));
    return all_cookies.front().LastAccessDate();
  }

  bool FindAndDeleteCookie(CookieMonster* cm,
                           const std::string& domain,
                           const std::string& name) {
    CookieList cookies = this->GetAllCookies(cm);
    for (auto& cookie : cookies)
      if (cookie.Domain() == domain && cookie.Name() == name)
        return this->DeleteCanonicalCookie(cm, cookie);
    return false;
  }

  void TestHostGarbageCollectHelper() {
    int domain_max_cookies = CookieMonster::kDomainMaxCookies;
    int domain_purge_cookies = CookieMonster::kDomainPurgeCookies;
    const int more_than_enough_cookies = domain_max_cookies + 10;
    // Add a bunch of cookies on a single host, should purge them.
    {
      auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), cookie));
        std::string cookies = this->GetCookies(cm.get(), http_www_foo_.url());
        // Make sure we find it in the cookies.
        EXPECT_NE(cookies.find(cookie), std::string::npos);
        // Count the number of cookies.
        EXPECT_LE(base::ranges::count(cookies, '='), domain_max_cookies);
      }
    }

    // Add a bunch of cookies on multiple hosts within a single eTLD.
    // Should keep at least kDomainMaxCookies - kDomainPurgeCookies
    // between them.  We shouldn't go above kDomainMaxCookies for both together.
    GURL url_google_specific(http_www_foo_.Format("http://www.gmail.%D"));
    {
      auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie_general = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), cookie_general));
        std::string cookie_specific = base::StringPrintf("c%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), url_google_specific, cookie_specific));
        std::string cookies_general =
            this->GetCookies(cm.get(), http_www_foo_.url());
        EXPECT_NE(cookies_general.find(cookie_general), std::string::npos);
        std::string cookies_specific =
            this->GetCookies(cm.get(), url_google_specific);
        EXPECT_NE(cookies_specific.find(cookie_specific), std::string::npos);
        EXPECT_LE((base::ranges::count(cookies_general, '=') +
                   base::ranges::count(cookies_specific, '=')),
                  domain_max_cookies);
      }
      // After all this, there should be at least
      // kDomainMaxCookies - kDomainPurgeCookies for both URLs.
      std::string cookies_general =
          this->GetCookies(cm.get(), http_www_foo_.url());
      std::string cookies_specific =
          this->GetCookies(cm.get(), url_google_specific);
      int total_cookies = (base::ranges::count(cookies_general, '=') +
                           base::ranges::count(cookies_specific, '='));
      EXPECT_GE(total_cookies, domain_max_cookies - domain_purge_cookies);
      EXPECT_LE(total_cookies, domain_max_cookies);
    }

    // Test histogram for the number of registrable domains affected by domain
    // purge.
    {
      auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
      GURL url;
      for (int domain_num = 0; domain_num < 3; ++domain_num) {
        url = GURL(base::StringPrintf("http://domain%d.test", domain_num));
        for (int i = 0; i < more_than_enough_cookies; ++i) {
          std::string cookie = base::StringPrintf("a%03d=b", i);
          EXPECT_TRUE(SetCookie(cm.get(), url, cookie));
          std::string cookies = this->GetCookies(cm.get(), url);
          // Make sure we find it in the cookies.
          EXPECT_NE(cookies.find(cookie), std::string::npos);
          // Count the number of cookies.
          EXPECT_LE(base::ranges::count(cookies, '='), domain_max_cookies);
        }
      }

      // Triggering eviction again for a previously affected registrable domain
      // does not increment the histogram.
      for (int i = 0; i < domain_purge_cookies * 2; ++i) {
        // Add some extra cookies (different names than before).
        std::string cookie = base::StringPrintf("b%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), url, cookie));
        std::string cookies = this->GetCookies(cm.get(), url);
        // Make sure we find it in the cookies.
        EXPECT_NE(cookies.find(cookie), std::string::npos);
        // Count the number of cookies.
        EXPECT_LE(base::ranges::count(cookies, '='), domain_max_cookies);
      }
    }
  }

  CookiePriority CharToPriority(char ch) {
    switch (ch) {
      case 'L':
        return COOKIE_PRIORITY_LOW;
      case 'M':
        return COOKIE_PRIORITY_MEDIUM;
      case 'H':
        return COOKIE_PRIORITY_HIGH;
    }
    NOTREACHED_IN_MIGRATION();
    return COOKIE_PRIORITY_DEFAULT;
  }

  // Instantiates a CookieMonster, adds multiple cookies (to http_www_foo_)
  // with priorities specified by |coded_priority_str|, and tests priority-aware
  // domain cookie eviction.
  //
  // Example: |coded_priority_string| of "2MN 3LS MN 4HN" specifies sequential
  // (i.e., from least- to most-recently accessed) insertion of 2
  // medium-priority non-secure cookies, 3 low-priority secure cookies, 1
  // medium-priority non-secure cookie, and 4 high-priority non-secure cookies.
  //
  // Within each priority, only the least-accessed cookies should be evicted.
  // Thus, to describe expected suriving cookies, it suffices to specify the
  // expected population of surviving cookies per priority, i.e.,
  // |expected_low_count|, |expected_medium_count|, and |expected_high_count|.
  void TestPriorityCookieCase(CookieMonster* cm,
                              const std::string& coded_priority_str,
                              size_t expected_low_count,
                              size_t expected_medium_count,
                              size_t expected_high_count,
                              size_t expected_nonsecure,
                              size_t expected_secure) {
    SCOPED_TRACE(coded_priority_str);
    this->DeleteAll(cm);
    int next_cookie_id = 0;
    // A list of cookie IDs, indexed by secure status, then by priority.
    std::vector<int> id_list[2][3];
    // A list of all the cookies stored, along with their properties.
    std::vector<std::pair<bool, CookiePriority>> cookie_data;

    // Parse |coded_priority_str| and add cookies.
    for (const std::string& token :
         base::SplitString(coded_priority_str, " ", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      DCHECK(!token.empty());

      bool is_secure = token.back() == 'S';

      // The second-to-last character is the priority. Grab and discard it.
      CookiePriority priority = CharToPriority(token[token.size() - 2]);

      // Discard the security status and priority tokens. The rest of the string
      // (possibly empty) specifies repetition.
      int rep = 1;
      if (!token.empty()) {
        bool result = base::StringToInt(
            base::MakeStringPiece(token.begin(), token.end() - 2), &rep);
        DCHECK(result);
      }
      for (; rep > 0; --rep, ++next_cookie_id) {
        std::string cookie =
            base::StringPrintf("a%d=b;priority=%s;%s", next_cookie_id,
                               CookiePriorityToString(priority).c_str(),
                               is_secure ? "secure" : "");

        EXPECT_TRUE(SetCookie(
            cm, is_secure ? https_www_foo_.url() : http_www_foo_.url(),
            cookie));
        cookie_data.emplace_back(is_secure, priority);
        id_list[is_secure][priority].push_back(next_cookie_id);
      }
    }

    int num_cookies = static_cast<int>(cookie_data.size());
    // A list of cookie IDs, indexed by secure status, then by priority.
    std::vector<int> surviving_id_list[2][3];

    // Parse the list of cookies
    std::string cookie_str = this->GetCookies(cm, https_www_foo_.url());
    // If any part of OBC is active then we also need to query the insecure url
    // and combine the resulting strings.
    if (cookie_util::IsOriginBoundCookiesPartiallyEnabled()) {
      std::string cookie_str_insecure =
          this->GetCookies(cm, http_www_foo_.url());

      std::vector<std::string_view> to_be_combined;
      // The cookie strings may be empty, only add them to our vector if
      // they're not. Otherwise we'll get an extra separator added which is bad.
      if (!cookie_str.empty()) {
        to_be_combined.push_back(cookie_str);
      }
      if (!cookie_str_insecure.empty()) {
        to_be_combined.push_back(cookie_str_insecure);
      }

      cookie_str = base::JoinString(to_be_combined, /*separator=*/"; ");
    }

    size_t num_nonsecure = 0;
    size_t num_secure = 0;
    for (const std::string& token : base::SplitString(
             cookie_str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // Assuming *it is "a#=b", so extract and parse "#" portion.
      int id = -1;
      bool result = base::StringToInt(
          base::MakeStringPiece(token.begin() + 1, token.end() - 2), &id);
      DCHECK(result);
      DCHECK_GE(id, 0);
      DCHECK_LT(id, num_cookies);
      surviving_id_list[cookie_data[id].first][cookie_data[id].second]
          .push_back(id);
      if (cookie_data[id].first)
        num_secure += 1;
      else
        num_nonsecure += 1;
    }

    EXPECT_EQ(expected_nonsecure, num_nonsecure);
    EXPECT_EQ(expected_secure, num_secure);

    // Validate each priority.
    size_t expected_count[3] = {expected_low_count, expected_medium_count,
                                expected_high_count};
    for (int i = 0; i < 3; ++i) {
      size_t num_for_priority =
          surviving_id_list[0][i].size() + surviving_id_list[1][i].size();
      EXPECT_EQ(expected_count[i], num_for_priority);
      // Verify that the remaining cookies are the most recent among those
      // with the same priorities.
      if (expected_count[i] == num_for_priority) {
        // Non-secure:
        std::sort(surviving_id_list[0][i].begin(),
                  surviving_id_list[0][i].end());
        EXPECT_TRUE(std::equal(
            surviving_id_list[0][i].begin(), surviving_id_list[0][i].end(),
            id_list[0][i].end() - surviving_id_list[0][i].size()));

        // Secure:
        std::sort(surviving_id_list[1][i].begin(),
                  surviving_id_list[1][i].end());
        EXPECT_TRUE(std::equal(
            surviving_id_list[1][i].begin(), surviving_id_list[1][i].end(),
            id_list[1][i].end() - surviving_id_list[1][i].size()));
      }
    }
  }

  // Represents a number of cookies to create, if they are Secure cookies, and
  // a url to add them to.
  struct CookiesEntry {
    size_t num_cookies;
    bool is_secure;
  };
  // A number of secure and a number of non-secure alternative hosts to create
  // for testing.
  typedef std::pair<size_t, size_t> AltHosts;
  // Takes an array of CookieEntries which specify the number, type, and order
  // of cookies to create. Cookies are created in the order they appear in
  // cookie_entries. The value of cookie_entries[x].num_cookies specifies how
  // many cookies of that type to create consecutively, while if
  // cookie_entries[x].is_secure is |true|, those cookies will be marked as
  // Secure.
  void TestSecureCookieEviction(base::span<const CookiesEntry> cookie_entries,
                                size_t expected_secure_cookies,
                                size_t expected_non_secure_cookies,
                                const AltHosts* alt_host_entries) {
    std::unique_ptr<CookieMonster> cm;

    if (alt_host_entries == nullptr) {
      cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
    } else {
      // When generating all of these cookies on alternate hosts, they need to
      // be all older than the max "safe" date for GC, which is currently 30
      // days, so we set them to 60.
      cm = CreateMonsterFromStoreForGC(
          alt_host_entries->first, alt_host_entries->first,
          alt_host_entries->second, alt_host_entries->second, 60);
    }

    int next_cookie_id = 0;
    for (const auto& cookie_entry : cookie_entries) {
      for (size_t j = 0; j < cookie_entry.num_cookies; j++) {
        std::string cookie;
        if (cookie_entry.is_secure)
          cookie = base::StringPrintf("a%d=b; Secure", next_cookie_id);
        else
          cookie = base::StringPrintf("a%d=b", next_cookie_id);
        EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), cookie));
        ++next_cookie_id;
      }
    }

    CookieList cookies = this->GetAllCookies(cm.get());
    EXPECT_EQ(expected_secure_cookies + expected_non_secure_cookies,
              cookies.size());
    size_t total_secure_cookies = 0;
    size_t total_non_secure_cookies = 0;
    for (const auto& cookie : cookies) {
      if (cookie.SecureAttribute()) {
        ++total_secure_cookies;
      } else {
        ++total_non_secure_cookies;
      }
    }

    EXPECT_EQ(expected_secure_cookies, total_secure_cookies);
    EXPECT_EQ(expected_non_secure_cookies, total_non_secure_cookies);
  }

  void TestPriorityAwareGarbageCollectHelperNonSecure() {
    // Hard-coding limits in the test, but use DCHECK_EQ to enforce constraint.
    DCHECK_EQ(180U, CookieMonster::kDomainMaxCookies);
    DCHECK_EQ(150U, CookieMonster::kDomainMaxCookies -
                        CookieMonster::kDomainPurgeCookies);

    auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
    // Key:
    // Round 1 => LN; round 2 => LS; round 3 => MN.
    // Round 4 => HN; round 5 => MS; round 6 => HS

    // Each test case adds 181 cookies, so 31 cookies are evicted.
    // Cookie same priority, repeated for each priority.
    TestPriorityCookieCase(cm.get(), "181LN", 150U, 0U, 0U, 150U, 0U);
    TestPriorityCookieCase(cm.get(), "181MN", 0U, 150U, 0U, 150U, 0U);
    TestPriorityCookieCase(cm.get(), "181HN", 0U, 0U, 150U, 150U, 0U);

    // Pairwise scenarios.
    // Round 1 => none; round2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "10HN 171MN", 0U, 140U, 10U, 150U, 0U);
    // Round 1 => 10L; round2 => 21M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "141MN 40LN", 30U, 120U, 0U, 150U, 0U);
    // Round 1 => none; round2 => 30M; round 3 => 1H.
    TestPriorityCookieCase(cm.get(), "101HN 80MN", 0U, 50U, 100U, 150U, 0U);

    // For {low, medium} priorities right on quota, different orders.
    // Round 1 => 1L; round 2 => none, round3 => 30H.
    TestPriorityCookieCase(cm.get(), "31LN 50MN 100HN", 30U, 50U, 70U, 150U,
                           0U);
    // Round 1 => none; round 2 => 1M, round3 => 30H.
    TestPriorityCookieCase(cm.get(), "51MN 100HN 30LN", 30U, 50U, 70U, 150U,
                           0U);
    // Round 1 => none; round 2 => none; round3 => 31H.
    TestPriorityCookieCase(cm.get(), "101HN 50MN 30LN", 30U, 50U, 70U, 150U,
                           0U);

    // Round 1 => 10L; round 2 => 10M; round3 => 11H.
    TestPriorityCookieCase(cm.get(), "81HN 60MN 40LN", 30U, 50U, 70U, 150U, 0U);

    // More complex scenarios.
    // Round 1 => 10L; round 2 => 10M; round 3 => 11H.
    TestPriorityCookieCase(cm.get(), "21HN 60MN 40LN 60HN", 30U, 50U, 70U, 150U,
                           0U);
    // Round 1 => 10L; round 2 => 21M; round 3 => 0H.
    TestPriorityCookieCase(cm.get(), "11HN 10MN 20LN 110MN 20LN 10HN", 30U, 99U,
                           21U, 150U, 0U);
    // Round 1 => none; round 2 => none; round 3 => 31H.
    TestPriorityCookieCase(cm.get(), "11LN 10MN 140HN 10MN 10LN", 21U, 20U,
                           109U, 150U, 0U);
    // Round 1 => none; round 2 => 21M; round 3 => 10H.
    TestPriorityCookieCase(cm.get(), "11MN 10HN 10LN 60MN 90HN", 10U, 50U, 90U,
                           150U, 0U);
    // Round 1 => none; round 2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "11MN 10HN 10LN 90MN 60HN", 10U, 70U, 70U,
                           150U, 0U);

    // Round 1 => 20L; round 2 => 0; round 3 => 11H
    TestPriorityCookieCase(cm.get(), "50LN 131HN", 30U, 0U, 120U, 150U, 0U);
    // Round 1 => 20L; round 2 => 0; round 3 => 11H
    TestPriorityCookieCase(cm.get(), "131HN 50LN", 30U, 0U, 120U, 150U, 0U);
    // Round 1 => 20L; round 2 => none; round 3 => 11H.
    TestPriorityCookieCase(cm.get(), "50HN 50LN 81HN", 30U, 0U, 120U, 150U, 0U);
    // Round 1 => 20L; round 2 => none; round 3 => 11H.
    TestPriorityCookieCase(cm.get(), "81HN 50LN 50HN", 30U, 0U, 120U, 150U, 0U);
  }

  void TestPriorityAwareGarbageCollectHelperSecure() {
    // Hard-coding limits in the test, but use DCHECK_EQ to enforce constraint.
    DCHECK_EQ(180U, CookieMonster::kDomainMaxCookies);
    DCHECK_EQ(150U, CookieMonster::kDomainMaxCookies -
                        CookieMonster::kDomainPurgeCookies);

    auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
    // Key:
    // Round 1 => LN; round 2 => LS; round 3 => MN.
    // Round 4 => HN; round 5 => MS; round 6 => HS

    // Each test case adds 181 cookies, so 31 cookies are evicted.
    // Cookie same priority, repeated for each priority.
    // Round 1 => 31L; round2 => none; round 3 => none.
    TestPriorityCookieCase(cm.get(), "181LS", 150U, 0U, 0U, 0U, 150U);
    // Round 1 => none; round2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "181MS", 0U, 150U, 0U, 0U, 150U);
    // Round 1 => none; round2 => none; round 3 => 31H.
    TestPriorityCookieCase(cm.get(), "181HS", 0U, 0U, 150U, 0U, 150U);

    // Pairwise scenarios.
    // Round 1 => none; round2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "10HS 171MS", 0U, 140U, 10U, 0U, 150U);
    // Round 1 => 10L; round2 => 21M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "141MS 40LS", 30U, 120U, 0U, 0U, 150U);
    // Round 1 => none; round2 => 30M; round 3 => 1H.
    TestPriorityCookieCase(cm.get(), "101HS 80MS", 0U, 50U, 100U, 0U, 150U);

    // For {low, medium} priorities right on quota, different orders.
    // Round 1 => 1L; round 2 => none, round3 => 30H.
    TestPriorityCookieCase(cm.get(), "31LS 50MS 100HS", 30U, 50U, 70U, 0U,
                           150U);
    // Round 1 => none; round 2 => 1M, round3 => 30H.
    TestPriorityCookieCase(cm.get(), "51MS 100HS 30LS", 30U, 50U, 70U, 0U,
                           150U);
    // Round 1 => none; round 2 => none; round3 => 31H.
    TestPriorityCookieCase(cm.get(), "101HS 50MS 30LS", 30U, 50U, 70U, 0U,
                           150U);

    // Round 1 => 10L; round 2 => 10M; round3 => 11H.
    TestPriorityCookieCase(cm.get(), "81HS 60MS 40LS", 30U, 50U, 70U, 0U, 150U);

    // More complex scenarios.
    // Round 1 => 10L; round 2 => 10M; round 3 => 11H.
    TestPriorityCookieCase(cm.get(), "21HS 60MS 40LS 60HS", 30U, 50U, 70U, 0U,
                           150U);
    // Round 1 => 10L; round 2 => 21M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "11HS 10MS 20LS 110MS 20LS 10HS", 30U, 99U,
                           21U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => 31H.
    TestPriorityCookieCase(cm.get(), "11LS 10MS 140HS 10MS 10LS", 21U, 20U,
                           109U, 0U, 150U);
    // Round 1 => none; round 2 => 21M; round 3 => 10H.
    TestPriorityCookieCase(cm.get(), "11MS 10HS 10LS 60MS 90HS", 10U, 50U, 90U,
                           0U, 150U);
    // Round 1 => none; round 2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "11MS 10HS 10LS 90MS 60HS", 10U, 70U, 70U,
                           0U, 150U);
  }

  void TestPriorityAwareGarbageCollectHelperMixed() {
    // Hard-coding limits in the test, but use DCHECK_EQ to enforce constraint.
    DCHECK_EQ(180U, CookieMonster::kDomainMaxCookies);
    DCHECK_EQ(150U, CookieMonster::kDomainMaxCookies -
                        CookieMonster::kDomainPurgeCookies);

    auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
    // Key:
    // Round 1 => LN; round 2 => LS; round 3 => MN.
    // Round 4 => HN; round 5 => MS; round 6 => HS

    // Each test case adds 180 secure cookies, and some non-secure cookie. The
    // secure cookies take priority, so the non-secure cookie is removed, along
    // with 30 secure cookies. Repeated for each priority, and with the
    // non-secure cookie as older and newer.
    // Round 1 => 1LN; round 2 => 30LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1LN 180LS", 150U, 0U, 0U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => 1MN.
    // Round 4 => none; round 5 => 30MS; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1MN 180MS", 0U, 150U, 0U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => none.
    // Round 4 => 1HN; round 5 => none; round 6 => 30HS.
    TestPriorityCookieCase(cm.get(), "1HN 180HS", 0U, 0U, 150U, 0U, 150U);
    // Round 1 => 1LN; round 2 => 30LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "180LS 1LN", 150U, 0U, 0U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => 1MN.
    // Round 4 => none; round 5 => 30MS; round 6 => none.
    TestPriorityCookieCase(cm.get(), "180MS 1MN", 0U, 150U, 0U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => none.
    // Round 4 => 1HN; round 5 => none; round 6 => 30HS.
    TestPriorityCookieCase(cm.get(), "180HS 1HN", 0U, 0U, 150U, 0U, 150U);

    // Quotas should be correctly maintained when a given priority has both
    // secure and non-secure cookies.
    //
    // Round 1 => 10LN; round 2 => none; round 3 => none.
    // Round 4 => 21HN; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "39LN 1LS 141HN", 30U, 0U, 120U, 149U, 1U);
    // Round 1 => none; round 2 => none; round 3 => 10MN.
    // Round 4 => none; round 5 => none; round 6 => 21HS.
    TestPriorityCookieCase(cm.get(), "29LN 1LS 59MN 1MS 91HS", 30U, 50U, 70U,
                           78U, 72U);

    // Low-priority secure cookies are removed before higher priority non-secure
    // cookies.
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "180LS 1MN", 149U, 1U, 0U, 1U, 149U);
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "180LS 1HN", 149U, 0U, 1U, 1U, 149U);
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1MN 180LS", 149U, 1U, 0U, 1U, 149U);
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1HN 180LS", 149U, 0U, 1U, 1U, 149U);

    // Higher-priority non-secure cookies are removed before any secure cookie
    // with greater than low-priority. Is it true? How about the quota?
    // Round 1 => none; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => 31MS; round 6 => none.
    TestPriorityCookieCase(cm.get(), "180MS 1HN", 0U, 149U, 1U, 1U, 149U);
    // Round 1 => none; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => 31MS; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1HN 180MS", 0U, 149U, 1U, 1U, 149U);

    // Pairwise:
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1LS 180LN", 150U, 0U, 0U, 149U, 1U);
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "100LS 81LN", 150U, 0U, 0U, 50U, 100U);
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "150LS 31LN", 150U, 0U, 0U, 0U, 150U);
    // Round 1 => none; round 2 => none; round 3 => none.
    // Round 4 => 31HN; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "1LS 180HN", 1U, 0U, 149U, 149U, 1U);
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "100LS 81HN", 69U, 0U, 81U, 81U, 69U);
    // Round 1 => none; round 2 => 31LS; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "150LS 31HN", 119U, 0U, 31U, 31U, 119U);

    // Quota calculations inside non-secure/secure blocks remain in place:
    // Round 1 => none; round 2 => 20LS; round 3 => none.
    // Round 4 => 11HN; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "50HN 50LS 81HS", 30U, 0U, 120U, 39U,
                           111U);
    // Round 1 => none; round 2 => none; round 3 => 31MN.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "11MS 10HN 10LS 90MN 60HN", 10U, 70U, 70U,
                           129U, 21U);
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    TestPriorityCookieCase(cm.get(), "40LS 40LN 101HS", 49U, 0U, 101U, 9U,
                           141U);

    // Multiple GC rounds end up with consistent behavior:
    // GC is started as soon as there are 181 cookies in the store.
    // On each major round it tries to preserve the quota for each priority.
    // It is not aware about more cookies going in.
    // 1 GC notices there are 181 cookies - 100HS 81LN 0MN
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    // 2 GC notices there are 181 cookies - 100HS 69LN 12MN
    // Round 1 => 31LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => none.
    // 3 GC notices there are 181 cookies - 100HS 38LN 43MN
    // Round 1 =>  8LN; round 2 => none; round 3 => none.
    // Round 4 => none; round 5 => none; round 6 => 23HS.
    // 4 GC notcies there are 181 cookies - 77HS 30LN 74MN
    // Round 1 => none; round 2 => none; round 3 => 24MN.
    // Round 4 => none; round 5 => none; round 6 =>  7HS.
    TestPriorityCookieCase(cm.get(), "100HS 100LN 100MN", 30U, 76U, 70U, 106U,
                           70U);
  }

  // Function for creating a CM with a number of cookies in it,
  // no store (and hence no ability to affect access time).
  std::unique_ptr<CookieMonster> CreateMonsterForGC(int num_cookies) {
    auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
    base::Time creation_time = base::Time::Now();
    for (int i = 0; i < num_cookies; i++) {
      std::unique_ptr<CanonicalCookie> cc(
          CanonicalCookie::CreateUnsafeCookieForTesting(
              "a", "1", base::StringPrintf("h%05d.izzle", i), /*path=*/"/",
              creation_time, /*=expiration_time=*/base::Time(),
              /*last_access=*/creation_time, /*last_update=*/creation_time,
              /*secure=*/true,
              /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
              COOKIE_PRIORITY_DEFAULT));
      GURL source_url = cookie_util::SimulatedCookieSource(*cc, "https");
      cm->SetCanonicalCookieAsync(std::move(cc), source_url,
                                  CookieOptions::MakeAllInclusive(),
                                  CookieStore::SetCookiesCallback());
    }
    return cm;
  }

  bool IsCookieInList(const CanonicalCookie& cookie, const CookieList& list) {
    for (const auto& c : list) {
      if (c.Name() == cookie.Name() && c.Value() == cookie.Value() &&
          c.Domain() == cookie.Domain() && c.Path() == cookie.Path() &&
          c.CreationDate() == cookie.CreationDate() &&
          c.ExpiryDate() == cookie.ExpiryDate() &&
          c.LastAccessDate() == cookie.LastAccessDate() &&
          c.LastUpdateDate() == cookie.LastUpdateDate() &&
          c.SecureAttribute() == cookie.SecureAttribute() &&
          c.IsHttpOnly() == cookie.IsHttpOnly() &&
          c.Priority() == cookie.Priority()) {
        return true;
      }
    }

    return false;
  }
  RecordingNetLogObserver net_log_;
};

using CookieMonsterTest = CookieMonsterTestBase<CookieMonsterTestTraits>;

class CookieMonsterTestGarbageCollectionObc
    : public CookieMonsterTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CookieMonsterTestGarbageCollectionObc() {
    scoped_feature_list_.InitWithFeatureStates(
        {{net::features::kEnableSchemeBoundCookies, IsSchemeBoundEnabled()},
         {net::features::kEnablePortBoundCookies, IsPortBoundEnabled()}});
  }

  bool IsSchemeBoundEnabled() const { return std::get<0>(GetParam()); }

  bool IsPortBoundEnabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using CookieMonsterTestPriorityGarbageCollectionObc =
    CookieMonsterTestGarbageCollectionObc;

struct CookiesInputInfo {
  const GURL url;
  const std::string name;
  const std::string value;
  const std::string domain;
  const std::string path;
  const base::Time expiration_time;
  bool secure;
  bool http_only;
  CookieSameSite same_site;
  CookiePriority priority;
};

}  // namespace

// This test suite verifies the task deferral behaviour of the CookieMonster.
// Specifically, for each asynchronous method, verify that:
// 1. invoking it on an uninitialized cookie store causes the store to begin
//    chain-loading its backing data or loading data for a specific domain key
//    (eTLD+1).
// 2. The initial invocation does not complete until the loading completes.
// 3. Invocations after the loading has completed complete immediately.
class DeferredCookieTaskTest : public CookieMonsterTest {
 protected:
  DeferredCookieTaskTest() {
    persistent_store_ = base::MakeRefCounted<MockPersistentCookieStore>();
    persistent_store_->set_store_load_commands(true);
    cookie_monster_ = std::make_unique<CookieMonster>(persistent_store_.get(),
                                                      net::NetLog::Get());
  }

  // Defines a cookie to be returned from PersistentCookieStore::Load
  void DeclareLoadedCookie(const GURL& url,
                           const std::string& cookie_line,
                           const base::Time& creation_time) {
    AddCookieToList(url, cookie_line, creation_time, &loaded_cookies_);
  }

  void ExecuteLoads(CookieStoreCommand::Type type) {
    const auto& commands = persistent_store_->commands();
    for (size_t i = 0; i < commands.size(); ++i) {
      // Only the first load command will produce the cookies.
      if (commands[i].type == type) {
        persistent_store_->TakeCallbackAt(i).Run(std::move(loaded_cookies_));
      }
    }
  }

  std::string CommandSummary(
      const MockPersistentCookieStore::CommandList& commands) {
    std::string out;
    for (const auto& command : commands) {
      switch (command.type) {
        case CookieStoreCommand::LOAD:
          base::StrAppend(&out, {"LOAD; "});
          break;
        case CookieStoreCommand::LOAD_COOKIES_FOR_KEY:
          base::StrAppend(&out, {"LOAD_FOR_KEY:", command.key, "; "});
          break;
        case CookieStoreCommand::ADD:
          base::StrAppend(&out, {"ADD; "});
          break;
        case CookieStoreCommand::REMOVE:
          base::StrAppend(&out, {"REMOVE; "});
          break;
      }
    }
    return out;
  }

  std::string TakeCommandSummary() {
    return CommandSummary(persistent_store_->TakeCommands());
  }

  // Holds cookies to be returned from PersistentCookieStore::Load or
  // PersistentCookieStore::LoadCookiesForKey.
  std::vector<std::unique_ptr<CanonicalCookie>> loaded_cookies_;

  std::unique_ptr<CookieMonster> cookie_monster_;
  scoped_refptr<MockPersistentCookieStore> persistent_store_;
};

TEST_F(DeferredCookieTaskTest, DeferredGetCookieList) {
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  GetCookieListCallback call1;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  // Finish the per-key load, not everything-load (which is always initiated).
  ExecuteLoads(CookieStoreCommand::LOAD_COOKIES_FOR_KEY);
  call1.WaitUntilDone();
  EXPECT_THAT(call1.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ", TakeCommandSummary());

  GetCookieListCallback call2;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call2.MakeCallback());
  // Already ready, no need for second load.
  EXPECT_THAT(call2.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredSetCookie) {
  // Generate puts to store w/o needing a proper expiration.
  cookie_monster_->SetPersistSessionCookies(true);

  ResultSavingCookieCallback<CookieAccessResult> call1;
  cookie_monster_->SetCanonicalCookieAsync(
      CanonicalCookie::CreateForTesting(http_www_foo_.url(), "A=B",
                                        base::Time::Now()),
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD_COOKIES_FOR_KEY);
  call1.WaitUntilDone();
  EXPECT_TRUE(call1.result().status.IsInclude());
  EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ADD; ", TakeCommandSummary());

  ResultSavingCookieCallback<CookieAccessResult> call2;
  cookie_monster_->SetCanonicalCookieAsync(
      CanonicalCookie::CreateForTesting(http_www_foo_.url(), "X=Y",
                                        base::Time::Now()),
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      call2.MakeCallback());
  ASSERT_TRUE(call2.was_run());
  EXPECT_TRUE(call2.result().status.IsInclude());
  EXPECT_EQ("ADD; ", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredSetAllCookies) {
  // Generate puts to store w/o needing a proper expiration.
  cookie_monster_->SetPersistSessionCookies(true);

  CookieList list;
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "." + http_www_foo_.domain(), "/", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), false, true,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "D", "." + http_www_foo_.domain(), "/", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), false, true,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  ResultSavingCookieCallback<CookieAccessResult> call1;
  cookie_monster_->SetAllCookiesAsync(list, call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_TRUE(call1.result().status.IsInclude());
  EXPECT_EQ("LOAD; ADD; ADD; ", TakeCommandSummary());

  // 2nd set doesn't need to read from store. It erases the old cookies, though.
  ResultSavingCookieCallback<CookieAccessResult> call2;
  cookie_monster_->SetAllCookiesAsync(list, call2.MakeCallback());
  ASSERT_TRUE(call2.was_run());
  EXPECT_TRUE(call2.result().status.IsInclude());
  EXPECT_EQ("REMOVE; REMOVE; ADD; ADD; ", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllCookies) {
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  GetAllCookiesCallback call1;
  cookie_monster_->GetAllCookiesAsync(call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_THAT(call1.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  GetAllCookiesCallback call2;
  cookie_monster_->GetAllCookiesAsync(call2.MakeCallback());
  EXPECT_TRUE(call2.was_run());
  EXPECT_THAT(call2.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlCookies) {
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  GetCookieListCallback call1;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD_COOKIES_FOR_KEY);
  call1.WaitUntilDone();
  EXPECT_THAT(call1.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ", TakeCommandSummary());

  GetCookieListCallback call2;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call2.MakeCallback());
  EXPECT_TRUE(call2.was_run());
  EXPECT_THAT(call2.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlWithOptionsCookies) {
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  GetCookieListCallback call1;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD_COOKIES_FOR_KEY);
  call1.WaitUntilDone();
  EXPECT_THAT(call1.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ", TakeCommandSummary());

  GetCookieListCallback call2;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), call2.MakeCallback());
  EXPECT_TRUE(call2.was_run());
  EXPECT_THAT(call2.cookies(), MatchesCookieLine("X=1"));
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCookies) {
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteAllAsync(call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(1u, call1.result());
  EXPECT_EQ("LOAD; REMOVE; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteAllAsync(call2.MakeCallback());
  // This needs an event loop spin since DeleteAllAsync always reports
  // asynchronously.
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCreatedInTimeRangeCookies) {
  const TimeRange time_range(base::Time(), base::Time::Now());

  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteAllCreatedInTimeRangeAsync(time_range,
                                                    call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(0u, call1.result());
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteAllCreatedInTimeRangeAsync(time_range,
                                                    call2.MakeCallback());
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest,
       DeferredDeleteAllWithPredicateCreatedInTimeRangeCookies) {
  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteAllMatchingInfoAsync(
      CookieDeletionInfo(Time(), Time::Now()), call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(0u, call1.result());
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteAllMatchingInfoAsync(
      CookieDeletionInfo(Time(), Time::Now()), call2.MakeCallback());
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteMatchingCookies) {
  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteMatchingCookiesAsync(
      base::BindRepeating(
          [](const net::CanonicalCookie& cookie) { return true; }),
      call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(0u, call1.result());
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteMatchingCookiesAsync(
      base::BindRepeating(
          [](const net::CanonicalCookie& cookie) { return true; }),
      call2.MakeCallback());
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteCanonicalCookie) {
  std::unique_ptr<CanonicalCookie> cookie = BuildCanonicalCookie(
      http_www_foo_.url(), "X=1; path=/", base::Time::Now());

  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteCanonicalCookieAsync(*cookie, call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  // TODO(morlovich): Fix DeleteCanonicalCookieAsync. This test should pass
  // when using LOAD_COOKIES_FOR_KEY instead, with that reflected in
  // TakeCommandSummary() as well.
  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(0u, call1.result());
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteCanonicalCookieAsync(*cookie, call2.MakeCallback());
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteSessionCookies) {
  ResultSavingCookieCallback<uint32_t> call1;
  cookie_monster_->DeleteSessionCookiesAsync(call1.MakeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(call1.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD);
  call1.WaitUntilDone();
  EXPECT_EQ(0u, call1.result());
  EXPECT_EQ("LOAD; ", TakeCommandSummary());

  ResultSavingCookieCallback<uint32_t> call2;
  cookie_monster_->DeleteSessionCookiesAsync(call2.MakeCallback());
  call2.WaitUntilDone();
  EXPECT_EQ(0u, call2.result());
  EXPECT_EQ("", TakeCommandSummary());
}

// Verify that a series of queued tasks are executed in order upon loading of
// the backing store and that new tasks received while the queued tasks are
// being dispatched go to the end of the queue.
TEST_F(DeferredCookieTaskTest, DeferredTaskOrder) {
  cookie_monster_->SetPersistSessionCookies(true);
  DeclareLoadedCookie(http_www_foo_.url(),
                      "X=1; path=/" + FutureCookieExpirationString(),
                      Time::Now() + base::Days(3));

  bool get_cookie_list_callback_was_run = false;
  GetCookieListCallback get_cookie_list_callback_deferred;
  ResultSavingCookieCallback<CookieAccessResult> set_cookies_callback;
  base::RunLoop run_loop;
  cookie_monster_->GetCookieListWithOptionsAsync(
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(),
      base::BindLambdaForTesting(
          [&](const CookieAccessResultList& cookies,
              const CookieAccessResultList& excluded_list) {
            // This should complete before the set.
            get_cookie_list_callback_was_run = true;
            EXPECT_FALSE(set_cookies_callback.was_run());
            EXPECT_THAT(cookies, MatchesCookieLine("X=1"));
            // Can't use TakeCommandSummary here since ExecuteLoads is walking
            // through the data it takes.
            EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ",
                      CommandSummary(persistent_store_->commands()));

            // Queue up a second get. It should see the result of the set queued
            // before it.
            cookie_monster_->GetCookieListWithOptionsAsync(
                http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
                CookiePartitionKeyCollection(),
                get_cookie_list_callback_deferred.MakeCallback());

            run_loop.Quit();
          }));

  cookie_monster_->SetCanonicalCookieAsync(
      CanonicalCookie::CreateForTesting(http_www_foo_.url(), "A=B",
                                        base::Time::Now()),
      http_www_foo_.url(), CookieOptions::MakeAllInclusive(),
      set_cookies_callback.MakeCallback());

  // Nothing happened yet, before loads are done.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(get_cookie_list_callback_was_run);
  EXPECT_FALSE(set_cookies_callback.was_run());

  ExecuteLoads(CookieStoreCommand::LOAD_COOKIES_FOR_KEY);
  run_loop.Run();
  EXPECT_EQ("LOAD; LOAD_FOR_KEY:foo.com; ADD; ", TakeCommandSummary());
  EXPECT_TRUE(get_cookie_list_callback_was_run);
  ASSERT_TRUE(set_cookies_callback.was_run());
  EXPECT_TRUE(set_cookies_callback.result().status.IsInclude());

  ASSERT_TRUE(get_cookie_list_callback_deferred.was_run());
  EXPECT_THAT(get_cookie_list_callback_deferred.cookies(),
              MatchesCookieLine("A=B; X=1"));
}

TEST_F(CookieMonsterTest, TestCookieDeleteAll) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  CookieOptions options = CookieOptions::MakeAllInclusive();

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), kValidCookieLine));
  EXPECT_EQ("A=B", GetCookies(cm.get(), http_www_foo_.url()));

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_foo_.url(), "C=D; httponly",
                                 options));
  EXPECT_EQ("A=B; C=D",
            GetCookiesWithOptions(cm.get(), http_www_foo_.url(), options));

  EXPECT_EQ(2u, DeleteAll(cm.get()));
  EXPECT_EQ("", GetCookiesWithOptions(cm.get(), http_www_foo_.url(), options));
  EXPECT_EQ(0u, store->commands().size());

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(),
                        kValidCookieLine + FutureCookieExpirationString()));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ(1u, DeleteAll(cm.get()));  // sync_to_store = true.
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  EXPECT_EQ("", GetCookiesWithOptions(cm.get(), http_www_foo_.url(), options));

  // Create a Partitioned cookie.
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));
  EXPECT_TRUE(SetCookie(
      cm.get(), https_www_foo_.url(),
      "__Host-" + std::string(kValidCookieLine) + "; partitioned; secure",
      cookie_partition_key));
  EXPECT_EQ(1u, DeleteAll(cm.get()));
  EXPECT_EQ("", GetCookiesWithOptions(
                    cm.get(), http_www_foo_.url(), options,
                    CookiePartitionKeyCollection(cookie_partition_key)));
  EXPECT_EQ(2u, store->commands().size());
}

TEST_F(CookieMonsterTest, TestCookieDeleteAllCreatedInTimeRangeTimestamps) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0u, DeleteAllCreatedInTimeRange(
                    cm.get(), TimeRange(now - base::Days(99), Time())));

  // Create 5 cookies with different creation dates.
  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), http_www_foo_.url(), "T-0=Now", now));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-1=Yesterday", now - base::Days(1)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-2=DayBefore", now - base::Days(2)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-3=ThreeDays", now - base::Days(3)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-7=LastWeek", now - base::Days(7)));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2u,
            DeleteAllCreatedInTimeRange(
                cm.get(), TimeRange(now - base::Days(3), now - base::Days(1))));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(
                    cm.get(), TimeRange(now - base::Days(2), now)));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(
                    cm.get(), TimeRange(now - base::Days(7), now)));

  // Delete the last (now) item.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(cm.get(), TimeRange()));

  // Really make sure everything is gone.
  EXPECT_EQ(0u, DeleteAll(cm.get()));

  // Test the same deletion process with partitioned cookies. Partitioned
  // cookies should behave the same way as unpartitioned cookies here, they are
  // just stored in a different data structure internally.

  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), http_www_foo_.url(), "T-0=Now", now,
                                CookiePartitionKey::FromURLForTesting(
                                    GURL("https://toplevelsite0.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), https_www_foo_.url(), "T-1=Yesterday", now - base::Days(1),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite1.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-2=DayBefore", now - base::Days(2),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite1.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-3=ThreeDays", now - base::Days(3),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite2.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-7=LastWeek", now - base::Days(7),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite3.com"))));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2u,
            DeleteAllCreatedInTimeRange(
                cm.get(), TimeRange(now - base::Days(3), now - base::Days(1))));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(
                    cm.get(), TimeRange(now - base::Days(2), now)));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(
                    cm.get(), TimeRange(now - base::Days(7), now)));

  // Delete the last (now) item.
  EXPECT_EQ(1u, DeleteAllCreatedInTimeRange(cm.get(), TimeRange()));

  // Really make sure everything is gone.
  EXPECT_EQ(0u, DeleteAll(cm.get()));
}

TEST_F(CookieMonsterTest,
       TestCookieDeleteAllCreatedInTimeRangeTimestampsWithInfo) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  Time now = Time::Now();

  CanonicalCookie test_cookie;

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0u,
            DeleteAllMatchingInfo(
                cm.get(), CookieDeletionInfo(now - base::Days(99), Time())));

  // Create 5 cookies with different creation dates.
  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), http_www_foo_.url(), "T-0=Now", now));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-1=Yesterday", now - base::Days(1)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-2=DayBefore", now - base::Days(2)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-3=ThreeDays", now - base::Days(3)));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "T-7=LastWeek", now - base::Days(7)));

  // Delete threedays and the daybefore.
  EXPECT_EQ(2u, DeleteAllMatchingInfo(cm.get(),
                                      CookieDeletionInfo(now - base::Days(3),
                                                         now - base::Days(1))));

  // Delete yesterday, also make sure that delete_end is not inclusive.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(
                    cm.get(), CookieDeletionInfo(now - base::Days(2), now)));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(
                    cm.get(), CookieDeletionInfo(now - base::Days(7), now)));

  // Delete the last (now) item.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(cm.get(), CookieDeletionInfo()));

  // Really make sure everything is gone.
  EXPECT_EQ(0u, DeleteAll(cm.get()));

  // Test the same deletion process with partitioned cookies. Partitioned
  // cookies should behave the same way as unpartitioned cookies here, they are
  // just stored in a different data structure internally.

  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), http_www_foo_.url(), "T-0=Now", now,
                                CookiePartitionKey::FromURLForTesting(
                                    GURL("https://toplevelsite0.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), https_www_foo_.url(), "T-1=Yesterday", now - base::Days(1),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite1.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-2=DayBefore", now - base::Days(2),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite1.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-3=ThreeDays", now - base::Days(3),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite2.com"))));
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), http_www_foo_.url(), "T-7=LastWeek", now - base::Days(7),
      CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite3.com"))));

  // Delete threedays and the daybefore.
  EXPECT_EQ(2u, DeleteAllMatchingInfo(cm.get(),
                                      CookieDeletionInfo(now - base::Days(3),
                                                         now - base::Days(1))));

  // Delete yesterday, also make sure that delete_end is not inclusive.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(
                    cm.get(), CookieDeletionInfo(now - base::Days(2), now)));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(
                    cm.get(), CookieDeletionInfo(now - base::Days(7), now)));

  // Delete the last (now) item.
  EXPECT_EQ(1u, DeleteAllMatchingInfo(cm.get(), CookieDeletionInfo()));

  // Really make sure everything is gone.
  EXPECT_EQ(0u, DeleteAll(cm.get()));
}

TEST_F(CookieMonsterTest, TestCookieDeleteMatchingCookies) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0u, DeleteMatchingCookies(
                    cm.get(),
                    base::BindRepeating([](const net::CanonicalCookie& cookie) {
                      return true;
                    })));

  // Create 5 cookies with different domains and security status.
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), GURL("https://a.com"),
                                        "a1=1;Secure", now));
  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), GURL("https://a.com"), "a2=2", now));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), GURL("https://b.com"),
                                        "b1=1;Secure", now));
  EXPECT_TRUE(
      SetCookieWithCreationTime(cm.get(), GURL("http://b.com"), "b2=2", now));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), GURL("https://c.com"),
                                        "c1=1;Secure", now));

  // Set a partitioned cookie.
  EXPECT_TRUE(SetCookieWithCreationTime(
      cm.get(), GURL("https://d.com"),
      "__Host-pc=123; path=/; secure; partitioned", now,
      CookiePartitionKey::FromURLForTesting(GURL("https://e.com"))));

  // Delete http cookies.
  EXPECT_EQ(2u, DeleteMatchingCookies(
                    cm.get(),
                    base::BindRepeating([](const net::CanonicalCookie& cookie) {
                      return !cookie.SecureAttribute();
                    })));
  EXPECT_THAT(GetAllCookies(cm.get()),
              ElementsAre(MatchesCookieNameDomain("a1", "a.com"),
                          MatchesCookieNameDomain("b1", "b.com"),
                          MatchesCookieNameDomain("c1", "c.com"),
                          MatchesCookieNameDomain("__Host-pc", "d.com")));

  // Delete remaining cookie for a.com.
  EXPECT_EQ(1u, DeleteMatchingCookies(
                    cm.get(),
                    base::BindRepeating([](const net::CanonicalCookie& cookie) {
                      return cookie.Domain() == "a.com";
                    })));
  EXPECT_THAT(GetAllCookies(cm.get()),
              ElementsAre(MatchesCookieNameDomain("b1", "b.com"),
                          MatchesCookieNameDomain("c1", "c.com"),
                          MatchesCookieNameDomain("__Host-pc", "d.com")));

  // Delete the partitioned cookie.
  EXPECT_EQ(1u, DeleteMatchingCookies(
                    cm.get(),
                    base::BindRepeating([](const net::CanonicalCookie& cookie) {
                      return cookie.IsPartitioned();
                    })));

  // Delete the last two item.
  EXPECT_EQ(2u, DeleteMatchingCookies(
                    cm.get(),
                    base::BindRepeating([](const net::CanonicalCookie& cookie) {
                      return true;
                    })));

  // Really make sure everything is gone.
  EXPECT_TRUE(GetAllCookies(cm.get()).empty());
}

static const base::TimeDelta kLastAccessThreshold = base::Milliseconds(200);
static const base::TimeDelta kAccessDelay =
    kLastAccessThreshold + base::Milliseconds(20);

TEST_F(CookieMonsterTest, TestLastAccess) {
  auto cm = std::make_unique<CookieMonster>(nullptr, kLastAccessThreshold,
                                            net::NetLog::Get());

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=B"));
  const Time last_access_date(GetFirstCookieAccessDate(cm.get()));

  // Reading the cookie again immediately shouldn't update the access date,
  // since we're inside the threshold.
  EXPECT_EQ("A=B", GetCookies(cm.get(), http_www_foo_.url()));
  EXPECT_EQ(last_access_date, GetFirstCookieAccessDate(cm.get()));

  // Reading after a short wait will update the access date, if the cookie
  // is requested with options that would update the access date. First, test
  // that the flag's behavior is respected.
  base::PlatformThread::Sleep(kAccessDelay);
  CookieOptions options = CookieOptions::MakeAllInclusive();
  options.set_do_not_update_access_time();
  EXPECT_EQ("A=B",
            GetCookiesWithOptions(cm.get(), http_www_foo_.url(), options));
  EXPECT_EQ(last_access_date, GetFirstCookieAccessDate(cm.get()));

  // Getting all cookies for a URL doesn't update the accessed time either.
  CookieList cookies = GetAllCookiesForURL(cm.get(), http_www_foo_.url());
  auto it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ(http_www_foo_.host(), it->Domain());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("B", it->Value());
  EXPECT_EQ(last_access_date, GetFirstCookieAccessDate(cm.get()));
  EXPECT_TRUE(++it == cookies.end());

  // If the flag isn't set, the last accessed time should be updated.
  options.set_update_access_time();
  EXPECT_EQ("A=B",
            GetCookiesWithOptions(cm.get(), http_www_foo_.url(), options));
  EXPECT_FALSE(last_access_date == GetFirstCookieAccessDate(cm.get()));
}

TEST_P(CookieMonsterTestPriorityGarbageCollectionObc,
       TestHostGarbageCollection) {
  TestHostGarbageCollectHelper();
}

TEST_P(CookieMonsterTestPriorityGarbageCollectionObc,
       TestPriorityAwareGarbageCollectionNonSecure) {
  TestPriorityAwareGarbageCollectHelperNonSecure();
}

TEST_P(CookieMonsterTestPriorityGarbageCollectionObc,
       TestPriorityAwareGarbageCollectionSecure) {
  TestPriorityAwareGarbageCollectHelperSecure();
}

TEST_P(CookieMonsterTestPriorityGarbageCollectionObc,
       TestPriorityAwareGarbageCollectionMixed) {
  TestPriorityAwareGarbageCollectHelperMixed();
}

// Test that domain cookies are always deleted before host cookies for a given
// {priority, secureness}. In this case, default priority and secure.
TEST_P(CookieMonsterTestGarbageCollectionObc, DomainCookiesPreferred) {
  ASSERT_TRUE(cookie_util::IsOriginBoundCookiesPartiallyEnabled());
  // This test requires the following values.
  ASSERT_EQ(180U, CookieMonster::kDomainMaxCookies);
  ASSERT_EQ(150U, CookieMonster::kDomainMaxCookies -
                      CookieMonster::kDomainPurgeCookies);

  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // Insert an extra host cookie so that one will need to be deleted;
  // demonstrating that host cookies will still be deleted if need be but they
  // aren't preferred.
  for (int i = 0; i < 151; i++) {
    std::string cookie = "host_" + base::NumberToString(i) + "=foo; Secure";
    EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), cookie));
  }

  // By adding the domain cookies after the host cookies they are more recently
  // accessed, which would normally cause these cookies to be preserved. By
  // showing that they're still deleted before the host cookies we can
  // demonstrate that domain cookies are preferred for deletion.
  for (int i = 0; i < 30; i++) {
    std::string cookie = "domain_" + base::NumberToString(i) +
                         "=foo; Secure; Domain=" + https_www_foo_.domain();
    EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), cookie));
  }

  auto cookie_list = this->GetAllCookiesForURL(cm.get(), https_www_foo_.url());

  int domain_count = 0;
  int host_count = 0;
  for (const auto& cookie : cookie_list) {
    if (cookie.IsHostCookie()) {
      host_count++;
    } else {
      domain_count++;
    }
  }

  EXPECT_EQ(host_count, 150);
  EXPECT_EQ(domain_count, 0);
}

// Securely set cookies should always be deleted after non-securely set cookies.
TEST_P(CookieMonsterTestGarbageCollectionObc, SecureCookiesPreferred) {
  ASSERT_TRUE(cookie_util::IsOriginBoundCookiesPartiallyEnabled());
  // This test requires the following values.
  ASSERT_EQ(180U, CookieMonster::kDomainMaxCookies);
  ASSERT_EQ(150U, CookieMonster::kDomainMaxCookies -
                      CookieMonster::kDomainPurgeCookies);

  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // If scheme binding is enabled then the secure url is enough, otherwise we
  // need to also add "Secure" to the cookie line.
  std::string secure_attr =
      cookie_util::IsSchemeBoundCookiesEnabled() ? "" : "; Secure";

  // These cookies would normally be preferred for deletion because they're 1)
  // Domain cookies, and 2) they're least recently accessed. But, since they're
  // securely set they'll be deleted after non-secure cookies.
  for (int i = 0; i < 151; i++) {
    std::string cookie = "domain_" + base::NumberToString(i) +
                         "=foo; Domain=" + https_www_foo_.domain() +
                         secure_attr;
    EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), cookie));
  }

  for (int i = 0; i < 30; i++) {
    std::string cookie = "host_" + base::NumberToString(i) + "=foo";
    EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), cookie));
  }

  auto secure_cookie_list =
      this->GetAllCookiesForURL(cm.get(), https_www_foo_.url());
  auto insecure_cookie_list =
      this->GetAllCookiesForURL(cm.get(), http_www_foo_.url());

  int domain_count = 0;
  int host_count = 0;

  for (const auto& cookie : secure_cookie_list) {
    if (cookie.IsHostCookie()) {
      host_count++;
    } else {
      domain_count++;
    }
  }

  for (const auto& cookie : insecure_cookie_list) {
    if (cookie.IsHostCookie()) {
      host_count++;
    } else {
      domain_count++;
    }
  }

  EXPECT_EQ(host_count, 0);
  EXPECT_EQ(domain_count, 150);
}

TEST_F(CookieMonsterTest, TestPartitionedCookiesGarbageCollection_Memory) {
  // Limit should be 10 KB.
  DCHECK_EQ(1024u * 10u, CookieMonster::kPerPartitionDomainMaxCookieBytes);

  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite1.com"));

  for (size_t i = 0; i < 41; ++i) {
    std::string cookie_value((10240 / 40) - (i < 10 ? 1 : 2), '0');
    std::string cookie =
        base::StrCat({base::NumberToString(i), "=", cookie_value});
    EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(),
                          cookie + "; secure; path=/; partitioned",
                          cookie_partition_key))
        << "Failed to set cookie " << i;
  }

  std::string cookies =
      this->GetCookies(cm.get(), https_www_foo_.url(),
                       CookiePartitionKeyCollection(cookie_partition_key));

  EXPECT_THAT(cookies, CookieStringIs(
                           testing::Not(testing::Contains(testing::Key("0")))));
  for (size_t i = 1; i < 41; ++i) {
    EXPECT_THAT(cookies, CookieStringIs(testing::Contains(
                             testing::Key(base::NumberToString(i)))))
        << "Failed to find cookie " << i;
  }
}

TEST_F(CookieMonsterTest, TestPartitionedCookiesGarbageCollection_MaxCookies) {
  // Partitioned cookies also limit domains to 180 cookies per partition.
  DCHECK_EQ(180u, CookieMonster::kPerPartitionDomainMaxCookies);

  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  for (size_t i = 0; i < 181; ++i) {
    std::string cookie = base::StrCat({base::NumberToString(i), "=0"});
    EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(),
                          cookie + "; secure; path=/; partitioned",
                          cookie_partition_key))
        << "Failed to set cookie " << i;
  }

  std::string cookies =
      this->GetCookies(cm.get(), https_www_foo_.url(),
                       CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_THAT(cookies, CookieStringIs(
                           testing::Not(testing::Contains(testing::Key("0")))));
  for (size_t i = 1; i < 181; ++i) {
    std::string cookie = base::StrCat({base::NumberToString(i), "=0"});
    EXPECT_THAT(cookies, CookieStringIs(testing::Contains(
                             testing::Key(base::NumberToString(i)))))
        << "Failed to find cookie " << i;
  }
}

TEST_F(CookieMonsterTest, SetCookieableSchemes) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  auto cm_foo = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // Only cm_foo should allow foo:// cookies.
  std::vector<std::string> schemes;
  schemes.push_back("foo");
  ResultSavingCookieCallback<bool> cookie_scheme_callback;
  cm_foo->SetCookieableSchemes(schemes, cookie_scheme_callback.MakeCallback());
  cookie_scheme_callback.WaitUntilDone();
  EXPECT_TRUE(cookie_scheme_callback.result());

  GURL foo_url("foo://host/path");
  GURL http_url("http://host/path");

  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "x=1").IsInclude());
  EXPECT_TRUE(
      SetCanonicalCookieReturnAccessResult(
          cm.get(),
          CanonicalCookie::CreateForTesting(http_url, "y=1", now, server_time),
          http_url, false /*modify_httponly*/)
          .status.IsInclude());

  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), foo_url, "x=1")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME}));
  EXPECT_TRUE(
      SetCanonicalCookieReturnAccessResult(
          cm.get(),
          CanonicalCookie::CreateForTesting(foo_url, "y=1", now, server_time),
          foo_url, false /*modify_httponly*/)
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME}));

  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm_foo.get(), foo_url, "x=1").IsInclude());
  EXPECT_TRUE(
      SetCanonicalCookieReturnAccessResult(
          cm_foo.get(),
          CanonicalCookie::CreateForTesting(foo_url, "y=1", now, server_time),
          foo_url, false /*modify_httponly*/)
          .status.IsInclude());

  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm_foo.get(), http_url, "x=1")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME}));
  EXPECT_TRUE(
      SetCanonicalCookieReturnAccessResult(
          cm_foo.get(),
          CanonicalCookie::CreateForTesting(http_url, "y=1", now, server_time),
          http_url, false /*modify_httponly*/)
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME}));
}

TEST_F(CookieMonsterTest, SetCookieableSchemes_StoreInitialized) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
  // Initializes the cookie store.
  this->GetCookies(cm.get(), https_www_foo_.url(),
                   CookiePartitionKeyCollection());

  std::vector<std::string> schemes;
  schemes.push_back("foo");
  ResultSavingCookieCallback<bool> cookie_scheme_callback;
  cm->SetCookieableSchemes(schemes, cookie_scheme_callback.MakeCallback());
  cookie_scheme_callback.WaitUntilDone();
  EXPECT_FALSE(cookie_scheme_callback.result());

  base::Time now = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  GURL foo_url("foo://host/path");
  EXPECT_TRUE(
      SetCanonicalCookieReturnAccessResult(
          cm.get(),
          CanonicalCookie::CreateForTesting(foo_url, "y=1", now, server_time),
          foo_url, false /*modify_httponly*/)
          .status.HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME}));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURL) {
  auto cm = std::make_unique<CookieMonster>(nullptr, kLastAccessThreshold,
                                            net::NetLog::Get());

  // Create an httponly cookie.
  CookieOptions options = CookieOptions::MakeAllInclusive();

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_foo_.url(), "A=B; httponly",
                                 options));
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_foo_.url(),
                                 http_www_foo_.Format("C=D; domain=.%D"),
                                 options));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_foo_.url(),
      http_www_foo_.Format("E=F; domain=.%D; secure"), options));

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_bar_.url(),
                                 http_www_bar_.Format("G=H; domain=.%D"),
                                 options));

  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_foo_.url(),
      https_www_foo_.Format("I=J; domain=.%D; secure"), options));

  // Create partitioned cookies for the same site with some partition key.
  auto cookie_partition_key1 =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite1.com"));
  auto cookie_partition_key2 =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite2.com"));
  auto cookie_partition_key3 =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite3.com"));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_bar_.url(), "__Host-K=L; secure; path=/; partitioned",
      options, std::nullopt, std::nullopt, cookie_partition_key1));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_bar_.url(), "__Host-M=N; secure; path=/; partitioned",
      options, std::nullopt, std::nullopt, cookie_partition_key2));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_bar_.url(), "__Host-O=P; secure; path=/; partitioned",
      options, std::nullopt, std::nullopt, cookie_partition_key3));

  const Time last_access_date(GetFirstCookieAccessDate(cm.get()));

  base::PlatformThread::Sleep(kAccessDelay);

  // Check cookies for url.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), http_www_foo_.url()),
      ElementsAre(MatchesCookieNameDomain("A", http_www_foo_.host()),
                  MatchesCookieNameDomain("C", http_www_foo_.Format(".%D"))));

  // Check cookies for url excluding http-only cookies.
  CookieOptions exclude_httponly = options;
  exclude_httponly.set_exclude_httponly();

  EXPECT_THAT(
      GetAllCookiesForURLWithOptions(cm.get(), http_www_foo_.url(),
                                     exclude_httponly),
      ElementsAre(MatchesCookieNameDomain("C", http_www_foo_.Format(".%D"))));

  // Test secure cookies.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_foo_.url()),
      ElementsAre(MatchesCookieNameDomain("A", http_www_foo_.host()),
                  MatchesCookieNameDomain("C", http_www_foo_.Format(".%D")),
                  MatchesCookieNameDomain("E", http_www_foo_.Format(".%D")),
                  MatchesCookieNameDomain("I", http_www_foo_.Format(".%D"))));

  // Test reading partitioned cookies for a single partition.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key1)),
      ElementsAre(MatchesCookieNameDomain("G", https_www_bar_.Format(".%D")),
                  MatchesCookieNameDomain("__Host-K", https_www_bar_.host())));
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key2)),
      ElementsAre(MatchesCookieNameDomain("G", https_www_bar_.Format(".%D")),
                  MatchesCookieNameDomain("__Host-M", https_www_bar_.host())));

  // Test reading partitioned cookies from multiple partitions.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(
                              {cookie_partition_key1, cookie_partition_key2})),
      ElementsAre(MatchesCookieNameDomain("G", https_www_bar_.Format(".%D")),
                  MatchesCookieNameDomain("__Host-K", https_www_bar_.host()),
                  MatchesCookieNameDomain("__Host-M", https_www_bar_.host())));

  // Test reading partitioned cookies from every partition.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection::ContainsAll()),
      ElementsAre(MatchesCookieNameDomain("G", https_www_bar_.Format(".%D")),
                  MatchesCookieNameDomain("__Host-K", https_www_bar_.host()),
                  MatchesCookieNameDomain("__Host-M", https_www_bar_.host()),
                  MatchesCookieNameDomain("__Host-O", https_www_bar_.host())));

  // Test excluding partitioned cookies.
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection()),
      ElementsAre(MatchesCookieNameDomain("G", https_www_bar_.Format(".%D"))));

  // Reading after a short wait should not update the access date.
  EXPECT_EQ(last_access_date, GetFirstCookieAccessDate(cm.get()));
}

TEST_F(CookieMonsterTest, GetExcludedCookiesForURL) {
  auto cm = std::make_unique<CookieMonster>(nullptr, kLastAccessThreshold,
                                            net::NetLog::Get());

  // Create an httponly cookie.
  CookieOptions options = CookieOptions::MakeAllInclusive();

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_foo_.url(), "A=B; httponly",
                                 options));
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), http_www_foo_.url(),
                                 http_www_foo_.Format("C=D; domain=.%D"),
                                 options));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_foo_.url(),
      http_www_foo_.Format("E=F; domain=.%D; secure"), options));

  base::PlatformThread::Sleep(kAccessDelay);

  // Check that no cookies are sent when option is turned off
  CookieOptions do_not_return_excluded;
  do_not_return_excluded.unset_return_excluded_cookies();

  CookieAccessResultList excluded_cookies = GetExcludedCookiesForURLWithOptions(
      cm.get(), http_www_foo_.url(), do_not_return_excluded);
  auto iter = excluded_cookies.begin();

  EXPECT_TRUE(excluded_cookies.empty());

  // Checking that excluded cookies get sent with their statuses with http
  // request.
  excluded_cookies = GetExcludedCookiesForURL(cm.get(), http_www_foo_.url(),
                                              CookiePartitionKeyCollection());
  iter = excluded_cookies.begin();

  ASSERT_TRUE(iter != excluded_cookies.end());
  EXPECT_EQ(http_www_foo_.Format(".%D"), iter->cookie.Domain());
  EXPECT_EQ("E", iter->cookie.Name());
  EXPECT_TRUE(iter->access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  ASSERT_TRUE(++iter == excluded_cookies.end());

  // Checking that excluded cookies get sent with their statuses with http-only.
  CookieOptions return_excluded;
  return_excluded.set_return_excluded_cookies();
  return_excluded.set_exclude_httponly();
  return_excluded.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT));

  excluded_cookies = GetExcludedCookiesForURLWithOptions(
      cm.get(), http_www_foo_.url(), return_excluded);
  iter = excluded_cookies.begin();

  ASSERT_TRUE(iter != excluded_cookies.end());
  EXPECT_EQ(http_www_foo_.host(), iter->cookie.Domain());
  EXPECT_EQ("A", iter->cookie.Name());
  EXPECT_TRUE(iter->access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));

  ASSERT_TRUE(++iter != excluded_cookies.end());
  EXPECT_EQ(http_www_foo_.Format(".%D"), iter->cookie.Domain());
  EXPECT_EQ("E", iter->cookie.Name());
  EXPECT_TRUE(iter->access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  ASSERT_TRUE(++iter == excluded_cookies.end());

  // Check that no excluded cookies are sent with secure request
  excluded_cookies = GetExcludedCookiesForURL(cm.get(), https_www_foo_.url(),
                                              CookiePartitionKeyCollection());
  iter = excluded_cookies.begin();

  EXPECT_TRUE(excluded_cookies.empty());
}

TEST_F(CookieMonsterTest, GetAllCookiesForURLPathMatching) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  CookieOptions options = CookieOptions::MakeAllInclusive();

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), www_foo_foo_.url(),
                                 "A=B; path=/foo;", options));
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), www_foo_bar_.url(),
                                 "C=D; path=/bar;", options));
  EXPECT_TRUE(
      CreateAndSetCookie(cm.get(), http_www_foo_.url(), "E=F;", options));

  CookieList cookies = GetAllCookiesForURL(cm.get(), www_foo_foo_.url());
  auto it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm.get(), www_foo_bar_.url());
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());
}

TEST_F(CookieMonsterTest, GetExcludedCookiesForURLPathMatching) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  CookieOptions options = CookieOptions::MakeAllInclusive();

  EXPECT_TRUE(CreateAndSetCookie(cm.get(), www_foo_foo_.url(),
                                 "A=B; path=/foo;", options));
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), www_foo_bar_.url(),
                                 "C=D; path=/bar;", options));
  EXPECT_TRUE(
      CreateAndSetCookie(cm.get(), http_www_foo_.url(), "E=F;", options));

  CookieAccessResultList excluded_cookies = GetExcludedCookiesForURL(
      cm.get(), www_foo_foo_.url(), CookiePartitionKeyCollection());
  auto it = excluded_cookies.begin();

  ASSERT_TRUE(it != excluded_cookies.end());
  EXPECT_EQ("C", it->cookie.Name());
  EXPECT_EQ("/bar", it->cookie.Path());
  EXPECT_TRUE(it->access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));

  ASSERT_TRUE(++it == excluded_cookies.end());

  excluded_cookies = GetExcludedCookiesForURL(cm.get(), www_foo_bar_.url(),
                                              CookiePartitionKeyCollection());
  it = excluded_cookies.begin();

  ASSERT_TRUE(it != excluded_cookies.end());
  EXPECT_EQ("A", it->cookie.Name());
  EXPECT_EQ("/foo", it->cookie.Path());
  EXPECT_TRUE(it->access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));

  ASSERT_TRUE(++it == excluded_cookies.end());
}

TEST_F(CookieMonsterTest, CookieSorting) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  base::Time system_time = base::Time::Now();
  for (const char* cookie_line :
       {"B=B1; path=/", "B=B2; path=/foo", "B=B3; path=/foo/bar",
        "A=A1; path=/", "A=A2; path=/foo", "A=A3; path=/foo/bar"}) {
    EXPECT_TRUE(SetCookieWithSystemTime(cm.get(), http_www_foo_.url(),
                                        cookie_line, system_time));
    system_time += base::Milliseconds(100);
  }

  // Re-set cookie which should not change sort order, as the creation date
  // will be retained, as per RFC 6265 5.3.11.3.
  EXPECT_TRUE(SetCookieWithSystemTime(cm.get(), http_www_foo_.url(),
                                      "B=B3; path=/foo/bar", system_time));

  CookieList cookies = GetAllCookies(cm.get());
  ASSERT_EQ(6u, cookies.size());
  EXPECT_EQ("B3", cookies[0].Value());
  EXPECT_EQ("A3", cookies[1].Value());
  EXPECT_EQ("B2", cookies[2].Value());
  EXPECT_EQ("A2", cookies[3].Value());
  EXPECT_EQ("B1", cookies[4].Value());
  EXPECT_EQ("A1", cookies[5].Value());
}

TEST_F(CookieMonsterTest, InheritCreationDate) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  base::Time the_not_so_distant_past(base::Time::Now() - base::Seconds(1000));
  EXPECT_TRUE(SetCookieWithCreationTime(cm.get(), http_www_foo_.url(),
                                        "Name=Value; path=/",
                                        the_not_so_distant_past));

  CookieList cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(the_not_so_distant_past, cookies[0].CreationDate());
  base::Time last_update = cookies[0].LastUpdateDate();

  // Overwrite the cookie with the same value, and verify that the creation date
  // is inherited. The update date isn't inherited though.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "Name=Value; path=/"));

  cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(the_not_so_distant_past, cookies[0].CreationDate());
  // If this is flakey you many need to manually set the last update time.
  EXPECT_LT(last_update, cookies[0].LastUpdateDate());
  last_update = cookies[0].LastUpdateDate();

  // New value => new creation date.
  EXPECT_TRUE(
      SetCookie(cm.get(), http_www_foo_.url(), "Name=NewValue; path=/"));

  cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_NE(the_not_so_distant_past, cookies[0].CreationDate());
  // If this is flakey you many need to manually set the last update time.
  EXPECT_LT(last_update, cookies[0].LastUpdateDate());
}

TEST_F(CookieMonsterTest, OverwriteSource) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // Set cookie with unknown source.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=0", std::nullopt,
                        CookieSourceType::kUnknown));
  CookieList cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("0", cookies[0].Value());
  EXPECT_EQ(CookieSourceType::kUnknown, cookies[0].SourceType());

  // Overwrite the cookie with the same value and an http source.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=0", std::nullopt,
                        CookieSourceType::kHTTP));
  cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("0", cookies[0].Value());
  EXPECT_EQ(CookieSourceType::kHTTP, cookies[0].SourceType());

  // Overwrite the cookie with a new value and a script source.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=1", std::nullopt,
                        CookieSourceType::kScript));
  cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("1", cookies[0].Value());
  EXPECT_EQ(CookieSourceType::kScript, cookies[0].SourceType());

  // Overwrite the cookie with the same value and an other source.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=1", std::nullopt,
                        CookieSourceType::kOther));
  cookies = GetAllCookies(cm.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("1", cookies[0].Value());
  EXPECT_EQ(CookieSourceType::kOther, cookies[0].SourceType());
}

// Check that GetAllCookiesForURL() does not return expired cookies and deletes
// them.
TEST_F(CookieMonsterTest, DeleteExpiredCookiesOnGet) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=B;"));

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "C=D;"));

  CookieList cookies = GetAllCookiesForURL(cm.get(), http_www_foo_.url());
  EXPECT_EQ(2u, cookies.size());

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(),
                        "C=D; expires=Thu, 01-Jan-1970 00:00:00 GMT"));

  cookies = GetAllCookiesForURL(cm.get(), http_www_foo_.url());
  EXPECT_EQ(1u, cookies.size());

  // Test partitioned cookies. They should exhibit the same behavior but are
  // stored in a different data structure internally.
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/; partitioned",
                        cookie_partition_key));
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; partitioned",
                        cookie_partition_key));

  cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_EQ(2u, cookies.size());

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; partitioned; expires=Thu, "
                        "01-Jan-1970 00:00:00 GMT",
                        cookie_partition_key));

  cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_EQ(1u, cookies.size());
}

// Test that cookie expiration works correctly when a cookie expires because
// time elapses.
TEST_F(CookieMonsterTest, DeleteExpiredCookiesAfterTimeElapsed) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/",
                        /*cookie_partition_key=*/std::nullopt));
  // Set a cookie with a Max-Age. Since we only parse integers for this
  // attribute, 1 second is the minimum allowable time.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; max-age=1",
                        /*cookie_partition_key=*/std::nullopt));

  CookieList cookies = GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                                           CookiePartitionKeyCollection());
  EXPECT_EQ(2u, cookies.size());

  // Sleep for entire Max-Age of the second cookie.
  base::PlatformThread::Sleep(base::Seconds(1));

  cookies = GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                                CookiePartitionKeyCollection());
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
}

TEST_F(CookieMonsterTest, DeleteExpiredPartitionedCookiesAfterTimeElapsed) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/; partitioned",
                        cookie_partition_key));
  // Set a cookie with a Max-Age. Since we only parse integers for this
  // attribute, 1 second is the minimum allowable time.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; partitioned; max-age=1",
                        cookie_partition_key));

  CookieList cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_EQ(2u, cookies.size());

  // Sleep for entire Max-Age of the second cookie.
  base::PlatformThread::Sleep(base::Seconds(1));

  cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
}

// This test is for verifying the fix of https://crbug.com/353034832.
TEST_F(CookieMonsterTest, ExpireSinglePartitionedCookie) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  // Set a cookie with a Max-Age. Since we only parse integers for this
  // attribute, 1 second is the minimum allowable time.
  ASSERT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=1; secure; path=/; partitioned; max-age=1",
                        cookie_partition_key));
  CookieList cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  ASSERT_EQ(1u, cookies.size());

  // Sleep for entire Max-Age of the cookie.
  base::PlatformThread::Sleep(base::Seconds(1));

  cookies = GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                                CookiePartitionKeyCollection::ContainsAll());
  EXPECT_EQ(0u, cookies.size());
}

TEST_F(CookieMonsterTest, DeleteExpiredAfterTimeElapsed_GetAllCookies) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/",
                        /*cookie_partition_key=*/std::nullopt));
  // Set a cookie with a Max-Age. Since we only parse integers for this
  // attribute, 1 second is the minimum allowable time.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; max-age=1",
                        /*cookie_partition_key=*/std::nullopt));

  GetAllCookiesCallback get_cookies_callback1;
  cm->GetAllCookiesAsync(get_cookies_callback1.MakeCallback());
  get_cookies_callback1.WaitUntilDone();
  ASSERT_EQ(2u, get_cookies_callback1.cookies().size());

  // Sleep for entire Max-Age of the second cookie.
  base::PlatformThread::Sleep(base::Seconds(1));

  GetAllCookiesCallback get_cookies_callback2;
  cm->GetAllCookiesAsync(get_cookies_callback2.MakeCallback());
  get_cookies_callback2.WaitUntilDone();

  ASSERT_EQ(1u, get_cookies_callback2.cookies().size());
  EXPECT_EQ("__Host-A", get_cookies_callback2.cookies()[0].Name());
}

TEST_F(CookieMonsterTest,
       DeleteExpiredPartitionedCookiesAfterTimeElapsed_GetAllCookies) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/; partitioned",
                        cookie_partition_key));
  // Set a cookie with a Max-Age. Since we only parse integers for this
  // attribute, 1 second is the minimum allowable time.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; max-age=1; partitioned",
                        cookie_partition_key));

  GetAllCookiesCallback get_cookies_callback1;
  cm->GetAllCookiesAsync(get_cookies_callback1.MakeCallback());
  get_cookies_callback1.WaitUntilDone();
  ASSERT_EQ(2u, get_cookies_callback1.cookies().size());

  // Sleep for entire Max-Age of the second cookie.
  base::PlatformThread::Sleep(base::Seconds(1));

  GetAllCookiesCallback get_cookies_callback2;
  cm->GetAllCookiesAsync(get_cookies_callback2.MakeCallback());
  get_cookies_callback2.WaitUntilDone();

  ASSERT_EQ(1u, get_cookies_callback2.cookies().size());
  EXPECT_EQ("__Host-A", get_cookies_callback2.cookies()[0].Name());
}

TEST_F(CookieMonsterTest, DeletePartitionedCookie) {
  auto cm = std::make_unique<CookieMonster>(
      /*store=*/nullptr, net::NetLog::Get());
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-A=B; secure; path=/; partitioned",
                        cookie_partition_key));
  // Set another partitioned and an unpartitioned cookie and make sure they are
  // unaffected.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-C=D; secure; path=/; partitioned",
                        cookie_partition_key));
  EXPECT_TRUE(SetCookie(cm.get(), https_www_bar_.url(),
                        "__Host-E=F; secure; path=/", std::nullopt));

  auto cookie = CanonicalCookie::CreateForTesting(
      https_www_bar_.url(), "__Host-A=B; secure; path=/; partitioned",
      /*creation_time=*/Time::Now(), /*server_time=*/std::nullopt,
      cookie_partition_key);
  ASSERT_TRUE(cookie);

  ResultSavingCookieCallback<unsigned int> delete_callback;
  cm->DeleteCanonicalCookieAsync(*cookie, delete_callback.MakeCallback());
  delete_callback.WaitUntilDone();

  CookieList cookies =
      GetAllCookiesForURL(cm.get(), https_www_bar_.url(),
                          CookiePartitionKeyCollection(cookie_partition_key));
  EXPECT_EQ(2u, cookies.size());
  EXPECT_EQ(cookies[0].Name(), "__Host-C");
  EXPECT_EQ(cookies[1].Name(), "__Host-E");
}

// Tests importing from a persistent cookie store that contains duplicate
// equivalent cookies. This situation should be handled by removing the
// duplicate cookie (both from the in-memory cache, and from the backing store).
//
// This is a regression test for: http://crbug.com/17855.
TEST_F(CookieMonsterTest, DontImportDuplicateCookies) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();

  // We will fill some initial cookies into the PersistentCookieStore,
  // to simulate a database with 4 duplicates.  Note that we need to
  // be careful not to have any duplicate creation times at all (as it's a
  // violation of a CookieMonster invariant) even if Time::Now() doesn't
  // move between calls.
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;

  // Insert 4 cookies with name "X" on path "/", with varying creation
  // dates. We expect only the most recent one to be preserved following
  // the import.

  AddCookieToList(GURL("http://www.foo.com"),
                  "X=1; path=/" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(3), &initial_cookies);

  AddCookieToList(GURL("http://www.foo.com"),
                  "X=2; path=/" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(1), &initial_cookies);

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList(GURL("http://www.foo.com"),
                  "X=3; path=/" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(4), &initial_cookies);

  AddCookieToList(GURL("http://www.foo.com"),
                  "X=4; path=/" + FutureCookieExpirationString(), Time::Now(),
                  &initial_cookies);

  // Insert 2 cookies with name "X" on path "/2", with varying creation
  // dates. We expect only the most recent one to be preserved the import.

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList(GURL("http://www.foo.com"),
                  "X=a1; path=/2" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(9), &initial_cookies);

  AddCookieToList(GURL("http://www.foo.com"),
                  "X=a2; path=/2" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(2), &initial_cookies);

  // Insert 1 cookie with name "Y" on path "/".
  AddCookieToList(GURL("http://www.foo.com"),
                  "Y=a; path=/" + FutureCookieExpirationString(),
                  Time::Now() + base::Days(10), &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, std::move(initial_cookies));

  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Verify that duplicates were not imported for path "/".
  // (If this had failed, GetCookies() would have also returned X=1, X=2, X=4).
  EXPECT_EQ("X=3; Y=a", GetCookies(cm.get(), GURL("http://www.foo.com/")));

  // Verify that same-named cookie on a different path ("/x2") didn't get
  // messed up.
  EXPECT_EQ("X=a1; X=3; Y=a",
            GetCookies(cm.get(), GURL("http://www.foo.com/2/x")));

  // Verify that the PersistentCookieStore was told to kill its 4 duplicates.
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[0].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
}

TEST_F(CookieMonsterTest, DontImportDuplicateCookies_PartitionedCookies) {
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;

  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com"));
  GURL cookie_url("https://www.bar.com");

  // Insert 3 partitioned cookies with same name, partition key, and path.

  // ===> This one is the WINNER (biggest creation time).  <====
  auto cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Z=a; Secure; Path=/; Partitioned; Max-Age=3456000",
      Time::Now() + base::Days(2), std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));

  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Z=b; Secure; Path=/; Partitioned; Max-Age=3456000",
      Time::Now(), std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));

  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Z=c; Secure; Path=/; Partitioned; Max-Age=3456000",
      Time::Now() + base::Days(1), std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  store->SetLoadExpectation(true, std::move(initial_cookies));

  EXPECT_EQ("__Host-Z=a",
            GetCookies(cm.get(), GURL("https://www.bar.com/"),
                       CookiePartitionKeyCollection(cookie_partition_key)));

  // Verify that the PersistentCookieStore was told to kill the 2
  // duplicates.
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[0].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
}

// Tests importing from a persistent cookie store that contains cookies
// with duplicate creation times.  This is OK now, but it still interacts
// with the de-duplication algorithm.
//
// This is a regression test for: http://crbug.com/43188.
TEST_F(CookieMonsterTest, ImportDuplicateCreationTimes) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();

  Time now(Time::Now());
  Time earlier(now - base::Days(1));

  // Insert 8 cookies, four with the current time as creation times, and
  // four with the earlier time as creation times.  We should only get
  // two cookies remaining, but which two (other than that there should
  // be one from each set) will be random.
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;
  AddCookieToList(GURL("http://www.foo.com"), "X=1; path=/", now,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "X=2; path=/", now,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "X=3; path=/", now,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "X=4; path=/", now,
                  &initial_cookies);

  AddCookieToList(GURL("http://www.foo.com"), "Y=1; path=/", earlier,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "Y=2; path=/", earlier,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "Y=3; path=/", earlier,
                  &initial_cookies);
  AddCookieToList(GURL("http://www.foo.com"), "Y=4; path=/", earlier,
                  &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, std::move(initial_cookies));

  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  CookieList list(GetAllCookies(cm.get()));
  EXPECT_EQ(2U, list.size());
  // Confirm that we have one of each.
  std::string name1(list[0].Name());
  std::string name2(list[1].Name());
  EXPECT_TRUE(name1 == "X" || name2 == "X");
  EXPECT_TRUE(name1 == "Y" || name2 == "Y");
  EXPECT_NE(name1, name2);
}

TEST_F(CookieMonsterTest, ImportDuplicateCreationTimes_PartitionedCookies) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();

  Time now(Time::Now());
  Time earlier(now - base::Days(1));

  GURL cookie_url("https://www.foo.com");
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com"));

  // Insert 6 cookies, four with the current time as creation times, and
  // four with the earlier time as creation times.  We should only get
  // two cookies remaining, but which two (other than that there should
  // be one from each set) will be random.

  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;
  auto cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-X=1; Secure; Path=/; Partitioned; Max-Age=3456000",
      now, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));
  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-X=2; Secure; Path=/; Partitioned; Max-Age=3456000",
      now, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));
  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-X=3; Secure; Path=/; Partitioned; Max-Age=3456000",
      now, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));

  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Y=1; Secure; Path=/; Partitioned; Max-Age=3456000",
      earlier, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));
  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Y=2; Secure; Path=/; Partitioned; Max-Age=3456000",
      earlier, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));
  cc = CanonicalCookie::CreateForTesting(
      cookie_url, "__Host-Y=3; Secure; Path=/; Partitioned; Max-Age=3456000",
      earlier, std::nullopt, cookie_partition_key);
  initial_cookies.push_back(std::move(cc));

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, std::move(initial_cookies));

  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  CookieList list(GetAllCookies(cm.get()));
  EXPECT_EQ(2U, list.size());
  // Confirm that we have one of each.
  std::string name1(list[0].Name());
  std::string name2(list[1].Name());
  EXPECT_TRUE(name1 == "__Host-X" || name2 == "__Host-X");
  EXPECT_TRUE(name1 == "__Host-Y" || name2 == "__Host-Y");
  EXPECT_NE(name1, name2);
}

TEST_F(CookieMonsterTest, PredicateSeesAllCookies) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  const base::Time now = PopulateCmForPredicateCheck(cm.get());
  // We test that we can see all cookies with |delete_info|. This includes
  // host, http_only, host secure, and all domain cookies.
  CookieDeletionInfo delete_info(base::Time(), now);
  delete_info.value_for_testing = "A";

  EXPECT_EQ(8u, DeleteAllMatchingInfo(cm.get(), std::move(delete_info)));

  EXPECT_EQ("dom_2=B; dom_3=C; host_3=C",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_2=B; host_2=B; sec_host=B",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("", GetCookies(cm.get(), GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=B; host_path_2=B; dom_2=B; host_2=B; sec_host=B",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure +
                                      std::string("/dir1/dir2/xxx"))));
  EXPECT_EQ("dom_2=B; host_2=B; sec_host=B; __Host-pc_2=B",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure),
                       CookiePartitionKeyCollection(
                           CookiePartitionKey::FromURLForTesting(
                               GURL(kTopLevelDomainPlus1)))));
}

// Mainly a test of GetEffectiveDomain, or more specifically, of the
// expected behavior of GetEffectiveDomain within the CookieMonster.
TEST_F(CookieMonsterTest, GetKey) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // This test is really only interesting if GetKey() actually does something.
  EXPECT_EQ("foo.com", cm->GetKey("www.foo.com"));
  EXPECT_EQ("google.izzie", cm->GetKey("www.google.izzie"));
  EXPECT_EQ("google.izzie", cm->GetKey(".google.izzie"));
  EXPECT_EQ("bbc.co.uk", cm->GetKey("bbc.co.uk"));
  EXPECT_EQ("bbc.co.uk", cm->GetKey("a.b.c.d.bbc.co.uk"));
  EXPECT_EQ("apple.com", cm->GetKey("a.b.c.d.apple.com"));
  EXPECT_EQ("apple.izzie", cm->GetKey("a.b.c.d.apple.izzie"));

  // Cases where the effective domain is null, so we use the host
  // as the key.
  EXPECT_EQ("co.uk", cm->GetKey("co.uk"));
  const std::string extension_name("iehocdgbbocmkdidlbnnfbmbinnahbae");
  EXPECT_EQ(extension_name, cm->GetKey(extension_name));
  EXPECT_EQ("com", cm->GetKey("com"));
  EXPECT_EQ("hostalias", cm->GetKey("hostalias"));
  EXPECT_EQ("localhost", cm->GetKey("localhost"));
}

// Test that cookies transfer from/to the backing store correctly.
// TODO(crbug.com/40188414): Include partitioned cookies in this test when we
// start saving them in the persistent store.
TEST_F(CookieMonsterTest, BackingStoreCommunication) {
  // Store details for cookies transforming through the backing store interface.

  base::Time current(base::Time::Now());
  auto store = base::MakeRefCounted<MockSimplePersistentCookieStore>();
  base::Time expires(base::Time::Now() + base::Seconds(100));

  const CookiesInputInfo input_info[] = {
      {GURL("https://a.b.foo.com"), "a", "1", "a.b.foo.com", "/path/to/cookie",
       expires, true /* secure */, false, CookieSameSite::NO_RESTRICTION,
       COOKIE_PRIORITY_DEFAULT},
      {GURL("https://www.foo.com"), "b", "2", ".foo.com", "/path/from/cookie",
       expires + base::Seconds(10), true, true, CookieSameSite::NO_RESTRICTION,
       COOKIE_PRIORITY_DEFAULT},
      {GURL("https://foo.com"), "c", "3", "foo.com", "/another/path/to/cookie",
       base::Time::Now() + base::Seconds(100), false, false,
       CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT}};
  const int INPUT_DELETE = 1;

  // Create new cookies and flush them to the store.
  {
    auto cmout =
        std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
    for (const auto& cookie : input_info) {
      EXPECT_TRUE(SetCanonicalCookie(
          cmout.get(),
          CanonicalCookie::CreateUnsafeCookieForTesting(
              cookie.name, cookie.value, cookie.domain, cookie.path,
              base::Time(), cookie.expiration_time, base::Time(), base::Time(),
              cookie.secure, cookie.http_only, cookie.same_site,
              cookie.priority),
          cookie.url, true /*modify_httponly*/));
    }

    EXPECT_TRUE(FindAndDeleteCookie(cmout.get(),
                                    input_info[INPUT_DELETE].domain,
                                    input_info[INPUT_DELETE].name));
  }

  // Create a new cookie monster and make sure that everything is correct
  {
    auto cmin =
        std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
    CookieList cookies(GetAllCookies(cmin.get()));
    ASSERT_EQ(2u, cookies.size());
    // Ordering is path length, then creation time.  So second cookie
    // will come first, and we need to swap them.
    std::swap(cookies[0], cookies[1]);
    for (int output_index = 0; output_index < 2; output_index++) {
      int input_index = output_index * 2;
      const CookiesInputInfo* input = &input_info[input_index];
      const CanonicalCookie* output = &cookies[output_index];

      EXPECT_EQ(input->name, output->Name());
      EXPECT_EQ(input->value, output->Value());
      EXPECT_EQ(input->url.host(), output->Domain());
      EXPECT_EQ(input->path, output->Path());
      EXPECT_LE(current.ToInternalValue(),
                output->CreationDate().ToInternalValue());
      EXPECT_EQ(input->secure, output->SecureAttribute());
      EXPECT_EQ(input->http_only, output->IsHttpOnly());
      EXPECT_EQ(input->same_site, output->SameSite());
      EXPECT_TRUE(output->IsPersistent());
      EXPECT_EQ(input->expiration_time.ToInternalValue(),
                output->ExpiryDate().ToInternalValue());
    }
  }
}

TEST_F(CookieMonsterTest, RestoreDifferentCookieSameCreationTime) {
  // Test that we can restore different cookies with duplicate creation times.
  base::Time current(base::Time::Now());
  scoped_refptr<MockPersistentCookieStore> store =
      base::MakeRefCounted<MockPersistentCookieStore>();

  {
    CookieMonster cmout(store.get(), net::NetLog::Get());
    GURL url("http://www.example.com/");
    EXPECT_TRUE(
        SetCookieWithCreationTime(&cmout, url, "A=1; max-age=600", current));
    EXPECT_TRUE(
        SetCookieWithCreationTime(&cmout, url, "B=2; max-age=600", current));
  }

  // Play back the cookies into store 2.
  scoped_refptr<MockPersistentCookieStore> store2 =
      base::MakeRefCounted<MockPersistentCookieStore>();
  std::vector<std::unique_ptr<CanonicalCookie>> load_expectation;
  EXPECT_EQ(2u, store->commands().size());
  for (const CookieStoreCommand& command : store->commands()) {
    ASSERT_EQ(command.type, CookieStoreCommand::ADD);
    load_expectation.push_back(
        std::make_unique<CanonicalCookie>(command.cookie));
  }
  store2->SetLoadExpectation(true, std::move(load_expectation));

  // Now read them in. Should get two cookies, not one.
  {
    CookieMonster cmin(store2.get(), net::NetLog::Get());
    CookieList cookies(GetAllCookies(&cmin));
    ASSERT_EQ(2u, cookies.size());
  }
}

TEST_F(CookieMonsterTest, CookieListOrdering) {
  // Put a random set of cookies into a monster and make sure
  // they're returned in the right order.
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  EXPECT_TRUE(
      SetCookie(cm.get(), GURL("http://d.c.b.a.foo.com/aa/x.html"), "c=1"));
  EXPECT_TRUE(SetCookie(cm.get(), GURL("http://b.a.foo.com/aa/bb/cc/x.html"),
                        "d=1; domain=b.a.foo.com"));
  EXPECT_TRUE(SetCookie(cm.get(), GURL("http://b.a.foo.com/aa/bb/cc/x.html"),
                        "a=4; domain=b.a.foo.com"));
  EXPECT_TRUE(SetCookie(cm.get(), GURL("http://c.b.a.foo.com/aa/bb/cc/x.html"),
                        "e=1; domain=c.b.a.foo.com"));
  EXPECT_TRUE(
      SetCookie(cm.get(), GURL("http://d.c.b.a.foo.com/aa/bb/x.html"), "b=1"));
  EXPECT_TRUE(SetCookie(cm.get(), GURL("http://news.bbc.co.uk/midpath/x.html"),
                        "g=10"));
  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookiesForURL(
        cm.get(), GURL("http://d.c.b.a.foo.com/aa/bb/cc/dd")));
    ASSERT_EQ(5u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }

  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookies(cm.get()));
    ASSERT_EQ(6u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("g", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }
}

// These garbage collection tests and CookieMonstertest.TestGCTimes (in
// cookie_monster_perftest.cc) are somewhat complementary.  These tests probe
// for whether garbage collection always happens when it should (i.e. that we
// actually get rid of cookies when we should).  The perftest is probing for
// whether garbage collection happens when it shouldn't.  See comments
// before that test for more details.

// Check to make sure that a whole lot of recent cookies doesn't get rid of
// anything after garbage collection is checked for.
TEST_F(CookieMonsterTest, GarbageCollectionKeepsRecentEphemeralCookies) {
  std::unique_ptr<CookieMonster> cm(
      CreateMonsterForGC(CookieMonster::kMaxCookies * 2 /* num_cookies */));
  EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
  // Will trigger GC.
  SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
  EXPECT_EQ(CookieMonster::kMaxCookies * 2 + 1, GetAllCookies(cm.get()).size());
}

// A whole lot of recent cookies; GC shouldn't happen.
TEST_F(CookieMonsterTest, GarbageCollectionKeepsRecentCookies) {
  std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
      CookieMonster::kMaxCookies * 2 /* num_cookies */, 0 /* num_old_cookies */,
      0, 0, CookieMonster::kSafeFromGlobalPurgeDays * 2);
  EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
  // Will trigger GC.
  SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
  EXPECT_EQ(CookieMonster::kMaxCookies * 2 + 1, GetAllCookies(cm.get()).size());
}

// Test case where there are more than kMaxCookies - kPurgeCookies recent
// cookies. All old cookies should be garbage collected, all recent cookies
// kept.
TEST_F(CookieMonsterTest, GarbageCollectionKeepsOnlyRecentCookies) {
  std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
      CookieMonster::kMaxCookies * 2 /* num_cookies */,
      CookieMonster::kMaxCookies / 2 /* num_old_cookies */, 0, 0,
      CookieMonster::kSafeFromGlobalPurgeDays * 2);
  EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
  // Will trigger GC.
  SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
  EXPECT_EQ(CookieMonster::kMaxCookies * 2 - CookieMonster::kMaxCookies / 2 + 1,
            GetAllCookies(cm.get()).size());
}

// Test case where there are exactly kMaxCookies - kPurgeCookies recent cookies.
// All old cookies should be deleted.
TEST_F(CookieMonsterTest, GarbageCollectionExactlyAllOldCookiesDeleted) {
  std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
      CookieMonster::kMaxCookies * 2 /* num_cookies */,
      CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies +
          1 /* num_old_cookies */,
      0, 0, CookieMonster::kSafeFromGlobalPurgeDays * 2);
  EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
  // Will trigger GC.
  SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
  EXPECT_EQ(CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
            GetAllCookies(cm.get()).size());
}

// Test case where there are less than kMaxCookies - kPurgeCookies recent
// cookies. Enough old cookies should be deleted to reach kMaxCookies -
// kPurgeCookies total cookies, but no more. Some old cookies should be kept.
TEST_F(CookieMonsterTest, GarbageCollectionTriggers5) {
  std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
      CookieMonster::kMaxCookies * 2 /* num_cookies */,
      CookieMonster::kMaxCookies * 3 / 2 /* num_old_cookies */, 0, 0,
      CookieMonster::kSafeFromGlobalPurgeDays * 2);
  EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
  // Will trigger GC.
  SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
  EXPECT_EQ(CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
            GetAllCookies(cm.get()).size());
}

// Tests garbage collection when there are only secure cookies.
// See https://crbug/730000
TEST_F(CookieMonsterTest, GarbageCollectWithSecureCookiesOnly) {
  // Create a CookieMonster at its cookie limit. A bit confusing, but the second
  // number is a subset of the first number.
  std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
      CookieMonster::kMaxCookies /* num_secure_cookies */,
      CookieMonster::kMaxCookies /* num_old_secure_cookies */,
      0 /* num_non_secure_cookies */, 0 /* num_old_non_secure_cookies */,
      CookieMonster::kSafeFromGlobalPurgeDays * 2 /* days_old */);
  EXPECT_EQ(CookieMonster::kMaxCookies, GetAllCookies(cm.get()).size());

  // Trigger purge with a secure cookie (So there are still no insecure
  // cookies).
  SetCookie(cm.get(), GURL("https://newdomain.com"), "b=2; Secure");
  EXPECT_EQ(CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
            GetAllCookies(cm.get()).size());
}

// Tests that if the main load event happens before the loaded event for a
// particular key, the tasks for that key run first.
TEST_F(CookieMonsterTest, WhileLoadingLoadCompletesBeforeKeyLoadCompletes) {
  const GURL kUrl = GURL(kTopLevelDomainPlus1);

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  store->set_store_load_commands(true);
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  auto cookie =
      CanonicalCookie::CreateForTesting(kUrl, "a=b", base::Time::Now());
  ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback;
  cm->SetCanonicalCookieAsync(std::move(cookie), kUrl,
                              CookieOptions::MakeAllInclusive(),
                              set_cookie_callback.MakeCallback());

  GetAllCookiesCallback get_cookies_callback1;
  cm->GetAllCookiesAsync(get_cookies_callback1.MakeCallback());

  // Two load events should have been queued.
  ASSERT_EQ(2u, store->commands().size());
  ASSERT_EQ(CookieStoreCommand::LOAD, store->commands()[0].type);
  ASSERT_EQ(CookieStoreCommand::LOAD_COOKIES_FOR_KEY,
            store->commands()[1].type);

  // The main load completes first (With no cookies).
  store->TakeCallbackAt(0).Run(std::vector<std::unique_ptr<CanonicalCookie>>());

  // The tasks should run in order, and the get should see the cookies.

  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result().status.IsInclude());

  get_cookies_callback1.WaitUntilDone();
  EXPECT_EQ(1u, get_cookies_callback1.cookies().size());

  // The loaded for key event completes late, with not cookies (Since they
  // were already loaded).
  store->TakeCallbackAt(1).Run(std::vector<std::unique_ptr<CanonicalCookie>>());

  // The just set cookie should still be in the store.
  GetAllCookiesCallback get_cookies_callback2;
  cm->GetAllCookiesAsync(get_cookies_callback2.MakeCallback());
  get_cookies_callback2.WaitUntilDone();
  EXPECT_EQ(1u, get_cookies_callback2.cookies().size());
}

// Tests that case that DeleteAll is waiting for load to complete, and then a
// get is queued. The get should wait to run until after all the cookies are
// retrieved, and should return nothing, since all cookies were just deleted.
TEST_F(CookieMonsterTest, WhileLoadingDeleteAllGetForURL) {
  const GURL kUrl = GURL(kTopLevelDomainPlus1);

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  store->set_store_load_commands(true);
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  ResultSavingCookieCallback<uint32_t> delete_callback;
  cm->DeleteAllAsync(delete_callback.MakeCallback());

  GetCookieListCallback get_cookie_list_callback;
  cm->GetCookieListWithOptionsAsync(kUrl, CookieOptions::MakeAllInclusive(),
                                    CookiePartitionKeyCollection(),
                                    get_cookie_list_callback.MakeCallback());

  // Only the main load should have been queued.
  ASSERT_EQ(1u, store->commands().size());
  ASSERT_EQ(CookieStoreCommand::LOAD, store->commands()[0].type);

  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  // When passed to the CookieMonster, it takes ownership of the pointed to
  // cookies.
  cookies.push_back(
      CanonicalCookie::CreateForTesting(kUrl, "a=b", base::Time::Now()));
  ASSERT_TRUE(cookies[0]);
  store->TakeCallbackAt(0).Run(std::move(cookies));

  delete_callback.WaitUntilDone();
  EXPECT_EQ(1u, delete_callback.result());

  get_cookie_list_callback.WaitUntilDone();
  EXPECT_EQ(0u, get_cookie_list_callback.cookies().size());
}

// Tests that a set cookie call sandwiched between two get all cookies, all
// before load completes, affects the first but not the second. The set should
// also not trigger a LoadCookiesForKey (As that could complete only after the
// main load for the store).
TEST_F(CookieMonsterTest, WhileLoadingGetAllSetGetAll) {
  const GURL kUrl = GURL(kTopLevelDomainPlus1);

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  store->set_store_load_commands(true);
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  GetAllCookiesCallback get_cookies_callback1;
  cm->GetAllCookiesAsync(get_cookies_callback1.MakeCallback());

  auto cookie =
      CanonicalCookie::CreateForTesting(kUrl, "a=b", base::Time::Now());
  ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback;
  cm->SetCanonicalCookieAsync(std::move(cookie), kUrl,
                              CookieOptions::MakeAllInclusive(),
                              set_cookie_callback.MakeCallback());

  GetAllCookiesCallback get_cookies_callback2;
  cm->GetAllCookiesAsync(get_cookies_callback2.MakeCallback());

  // Only the main load should have been queued.
  ASSERT_EQ(1u, store->commands().size());
  ASSERT_EQ(CookieStoreCommand::LOAD, store->commands()[0].type);

  // The load completes (With no cookies).
  store->TakeCallbackAt(0).Run(std::vector<std::unique_ptr<CanonicalCookie>>());

  get_cookies_callback1.WaitUntilDone();
  EXPECT_EQ(0u, get_cookies_callback1.cookies().size());

  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result().status.IsInclude());

  get_cookies_callback2.WaitUntilDone();
  EXPECT_EQ(1u, get_cookies_callback2.cookies().size());
}

namespace {

void RunClosureOnAllCookiesReceived(base::OnceClosure closure,
                                    const CookieList& cookie_list) {
  std::move(closure).Run();
}

}  // namespace

// Tests that if a single cookie task is queued as a result of a task performed
// on all cookies when loading completes, it will be run after any already
// queued tasks.
TEST_F(CookieMonsterTest, CheckOrderOfCookieTaskQueueWhenLoadingCompletes) {
  const GURL kUrl = GURL(kTopLevelDomainPlus1);

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  store->set_store_load_commands(true);
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Get all cookies task that queues a task to set a cookie when executed.
  auto cookie =
      CanonicalCookie::CreateForTesting(kUrl, "a=b", base::Time::Now());
  ResultSavingCookieCallback<CookieAccessResult> set_cookie_callback;
  cm->GetAllCookiesAsync(base::BindOnce(
      &RunClosureOnAllCookiesReceived,
      base::BindOnce(&CookieStore::SetCanonicalCookieAsync,
                     base::Unretained(cm.get()), std::move(cookie), kUrl,
                     CookieOptions::MakeAllInclusive(),
                     set_cookie_callback.MakeCallback(), std::nullopt)));

  // Get cookie task. Queued before the delete task is executed, so should not
  // see the set cookie.
  GetAllCookiesCallback get_cookies_callback1;
  cm->GetAllCookiesAsync(get_cookies_callback1.MakeCallback());

  // Only the main load should have been queued.
  ASSERT_EQ(1u, store->commands().size());
  ASSERT_EQ(CookieStoreCommand::LOAD, store->commands()[0].type);

  // The load completes.
  store->TakeCallbackAt(0).Run(std::vector<std::unique_ptr<CanonicalCookie>>());

  // The get cookies call should see no cookies set.
  get_cookies_callback1.WaitUntilDone();
  EXPECT_EQ(0u, get_cookies_callback1.cookies().size());

  set_cookie_callback.WaitUntilDone();
  EXPECT_TRUE(set_cookie_callback.result().status.IsInclude());

  // A subsequent get cookies call should see the new cookie.
  GetAllCookiesCallback get_cookies_callback2;
  cm->GetAllCookiesAsync(get_cookies_callback2.MakeCallback());
  get_cookies_callback2.WaitUntilDone();
  EXPECT_EQ(1u, get_cookies_callback2.cookies().size());
}

// Test that FlushStore() is forwarded to the store and callbacks are posted.
TEST_F(CookieMonsterTest, FlushStore) {
  auto counter = base::MakeRefCounted<CallbackCounter>();
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cm = std::make_unique<CookieMonster>(store, net::NetLog::Get());

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(0, counter->callback_count());

  // Before initialization, FlushStore() should just run the callback.
  cm->FlushStore(base::BindOnce(&CallbackCounter::Callback, counter));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // NULL callback is safe.
  cm->FlushStore(base::OnceClosure());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // After initialization, FlushStore() should delegate to the store.
  GetAllCookies(cm.get());  // Force init.
  cm->FlushStore(base::BindOnce(&CallbackCounter::Callback, counter));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // NULL callback is still safe.
  cm->FlushStore(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // If there's no backing store, FlushStore() is always a safe no-op.
  cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());
  GetAllCookies(cm.get());  // Force init.
  cm->FlushStore(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2, counter->callback_count());

  cm->FlushStore(base::BindOnce(&CallbackCounter::Callback, counter));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3, counter->callback_count());
}

TEST_F(CookieMonsterTest, SetAllCookies) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cm->SetPersistSessionCookies(true);

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "U=V; path=/"));
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "W=X; path=/foo"));
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "Y=Z; path=/"));

  CookieList list;
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "." + http_www_foo_.url().host(), "/", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), false, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "C", "D", "." + http_www_foo_.url().host(), "/bar", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), false, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "W", "X", "." + http_www_foo_.url().host(), "/", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), false, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-Y", "Z", https_www_foo_.url().host(), "/", base::Time::Now(),
      base::Time(), base::Time(), base::Time(), true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT,
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"))));
  // Expired cookie, should not be stored.
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "expired", "foobar", https_www_foo_.url().host(), "/",
      base::Time::Now() - base::Days(1), base::Time::Now() - base::Days(2),
      base::Time(), base::Time(), /*secure=*/true, /*httponly=*/false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // SetAllCookies must not flush.
  ASSERT_EQ(0, store->flush_count());
  EXPECT_TRUE(SetAllCookies(cm.get(), list));
  EXPECT_EQ(0, store->flush_count());

  CookieList cookies = GetAllCookies(cm.get());
  size_t expected_size = 4;  // "A", "W" and "Y". "U" is gone.
  EXPECT_EQ(expected_size, cookies.size());
  auto it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("D", it->Value());
  EXPECT_EQ("/bar", it->Path());  // The path has been updated.

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("B", it->Value());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("W", it->Name());
  EXPECT_EQ("X", it->Value());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("__Host-Y", it->Name());
  EXPECT_EQ("Z", it->Value());

  cm = nullptr;
  auto entries = net_log_.GetEntries();
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_ALIVE, NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_STORE_SESSION_PERSISTENCE,
      NetLogEventPhase::NONE);
  pos = ExpectLogContainsSomewhere(entries, pos,
                                   NetLogEventType::COOKIE_STORE_COOKIE_ADDED,
                                   NetLogEventPhase::NONE);
  ExpectLogContainsSomewhere(entries, pos, NetLogEventType::COOKIE_STORE_ALIVE,
                             NetLogEventPhase::END);
}

// Check that DeleteAll does flush (as a quick check that flush_count() works).
TEST_F(CookieMonsterTest, DeleteAll) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cm->SetPersistSessionCookies(true);

  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "X=Y; path=/"));

  ASSERT_EQ(0, store->flush_count());
  EXPECT_EQ(1u, DeleteAll(cm.get()));
  EXPECT_EQ(1, store->flush_count());

  cm = nullptr;
  auto entries = net_log_.GetEntries();
  size_t pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_ALIVE, NetLogEventPhase::BEGIN);
  pos = ExpectLogContainsSomewhere(
      entries, pos, NetLogEventType::COOKIE_STORE_SESSION_PERSISTENCE,
      NetLogEventPhase::NONE);
  pos = ExpectLogContainsSomewhere(entries, pos,
                                   NetLogEventType::COOKIE_STORE_COOKIE_ADDED,
                                   NetLogEventPhase::NONE);
  pos = ExpectLogContainsSomewhere(entries, pos,
                                   NetLogEventType::COOKIE_STORE_COOKIE_DELETED,
                                   NetLogEventPhase::NONE);
  ExpectLogContainsSomewhere(entries, pos, NetLogEventType::COOKIE_STORE_ALIVE,
                             NetLogEventPhase::END);
}

TEST_F(CookieMonsterTest, HistogramCheck) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // Should match call in InitializeHistograms, but doesn't really matter
  // since the histogram should have been initialized by the CM construction
  // above.
  base::HistogramBase* expired_histogram = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutesSecure", 1, 10 * 365 * 24 * 60, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  std::unique_ptr<base::HistogramSamples> samples1(
      expired_histogram->SnapshotSamples());
  auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
      "a", "b", "a.url", "/", base::Time(),
      base::Time::Now() + base::Minutes(59), base::Time(), base::Time(),
      /*secure=*/true,
      /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  GURL source_url = cookie_util::SimulatedCookieSource(*cookie, "https");
  ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cookie), source_url,
                                 /*modify_httponly=*/true));

  std::unique_ptr<base::HistogramSamples> samples2(
      expired_histogram->SnapshotSamples());
  EXPECT_EQ(samples1->TotalCount() + 1, samples2->TotalCount());

  // kValidCookieLine creates a session cookie.
  ASSERT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), kValidCookieLine));

  std::unique_ptr<base::HistogramSamples> samples3(
      expired_histogram->SnapshotSamples());
  EXPECT_EQ(samples2->TotalCount(), samples3->TotalCount());
}

TEST_F(CookieMonsterTest, InvalidExpiryTime) {
  std::string cookie_line =
      std::string(kValidCookieLine) + "; expires=Blarg arg arg";
  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), cookie_line, Time::Now()));
  ASSERT_FALSE(cookie->IsPersistent());
}

// Test that CookieMonster writes session cookies into the underlying
// CookieStore if the "persist session cookies" option is on.
TEST_F(CookieMonsterTest, PersistSessionCookies) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cm->SetPersistSessionCookies(true);

  // All cookies set with SetCookie are session cookies.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm.get(), http_www_foo_.url()));

  // The cookie was written to the backing store.
  EXPECT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  EXPECT_EQ("A", store->commands()[0].cookie.Name());
  EXPECT_EQ("B", store->commands()[0].cookie.Value());

  // Modify the cookie.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=C"));
  EXPECT_EQ("A=C", GetCookies(cm.get(), http_www_foo_.url()));
  EXPECT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ("A", store->commands()[1].cookie.Name());
  EXPECT_EQ("B", store->commands()[1].cookie.Value());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  EXPECT_EQ("A", store->commands()[2].cookie.Name());
  EXPECT_EQ("C", store->commands()[2].cookie.Value());

  // Delete the cookie. Using .host() here since it's a host and not domain
  // cookie.
  EXPECT_TRUE(FindAndDeleteCookie(cm.get(), http_www_foo_.host(), "A"));
  EXPECT_EQ("", GetCookies(cm.get(), http_www_foo_.url()));
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ("A", store->commands()[3].cookie.Name());
  EXPECT_EQ("C", store->commands()[3].cookie.Value());
}

// Test the commands sent to the persistent cookie store.
TEST_F(CookieMonsterTest, PersisentCookieStorageTest) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Add a cookie.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(),
                        "A=B" + FutureCookieExpirationString()));
  this->MatchCookieLines("A=B", GetCookies(cm.get(), http_www_foo_.url()));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  // Remove it.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "A=B; max-age=0"));
  this->MatchCookieLines(std::string(),
                         GetCookies(cm.get(), http_www_foo_.url()));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  // Add a cookie.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(),
                        "A=B" + FutureCookieExpirationString()));
  this->MatchCookieLines("A=B", GetCookies(cm.get(), http_www_foo_.url()));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  // Overwrite it.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(),
                        "A=Foo" + FutureCookieExpirationString()));
  this->MatchCookieLines("A=Foo", GetCookies(cm.get(), http_www_foo_.url()));
  ASSERT_EQ(5u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[4].type);

  // Create some non-persistent cookies and check that they don't go to the
  // persistent storage.
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "B=Bar"));
  this->MatchCookieLines("A=Foo; B=Bar",
                         GetCookies(cm.get(), http_www_foo_.url()));
  EXPECT_EQ(5u, store->commands().size());
}

// Test to assure that cookies with control characters are purged appropriately.
// See http://crbug.com/238041 for background.
TEST_F(CookieMonsterTest, ControlCharacterPurge) {
  const Time now1(Time::Now());
  const Time now2(Time::Now() + base::Seconds(1));
  const Time now3(Time::Now() + base::Seconds(2));
  const Time now4(Time::Now() + base::Seconds(3));
  const Time later(now1 + base::Days(1));
  const GURL url("https://host/path");
  const std::string domain("host");
  const std::string path("/path");

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();

  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;

  AddCookieToList(url, "foo=bar; path=" + path, now1, &initial_cookies);

  // We have to manually build these cookies because they contain control
  // characters, and our cookie line parser rejects control characters.
  std::unique_ptr<CanonicalCookie> cc =
      CanonicalCookie::CreateUnsafeCookieForTesting(
          "baz",
          "\x05"
          "boo",
          "." + domain, path, now2, later, base::Time(), base::Time(),
          true /* secure */, false /* httponly */,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);
  initial_cookies.push_back(std::move(cc));

  std::unique_ptr<CanonicalCookie> cc2 =
      CanonicalCookie::CreateUnsafeCookieForTesting(
          "baz",
          "\x7F"
          "boo",
          "." + domain, path, now3, later, base::Time(), base::Time(),
          true /* secure */, false /* httponly */,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);
  initial_cookies.push_back(std::move(cc2));

  // Partitioned cookies with control characters should not be loaded.
  auto cookie_partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));
  std::unique_ptr<CanonicalCookie> cc3 =
      CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-baz",
          "\x7F"
          "boo",
          domain, "/", now3, later, base::Time(), base::Time(),
          true /* secure */, false /* httponly */,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT,
          cookie_partition_key);
  initial_cookies.push_back(std::move(cc3));

  AddCookieToList(url, "hello=world; path=" + path, now4, &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, std::move(initial_cookies));

  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  EXPECT_EQ("foo=bar; hello=world",
            GetCookies(cm.get(), url,
                       CookiePartitionKeyCollection(cookie_partition_key)));
}

// Test that inserting the first cookie for a key and deleting the last cookie
// for a key correctly reflected in the Cookie.NumKeys histogram.
TEST_F(CookieMonsterTest, NumKeysHistogram) {
  const char kHistogramName[] = "Cookie.NumKeys";

  // Test loading cookies from store.
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      GURL("http://domain1.test"), "A=1", base::Time::Now()));
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      GURL("http://domain2.test"), "A=1", base::Time::Now()));
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      GURL("http://sub.domain2.test"), "A=1", base::Time::Now()));
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      GURL("http://domain3.test"), "A=1", base::Time::Now()));
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      GURL("http://domain3.test"), "B=1", base::Time::Now()));
  store->SetLoadExpectation(true /* return_value */,
                            std::move(initial_cookies));
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  {
    base::HistogramTester histogram_tester;
    // Access the cookies to trigger loading from the persistent store.
    EXPECT_EQ(5u, this->GetAllCookies(cm.get()).size());
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    // There should be 3 keys: "domain1.test", "domain2.test", and
    // "domain3.test".
    histogram_tester.ExpectUniqueSample(kHistogramName, 3 /* sample */,
                                        1 /* count */);
  }

  // Test adding cookies for already existing key.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain1.test"),
                                   "B=1", CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("http://sub.domain1.test"),
                                   "B=1", CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 3 /* sample */,
                                        1 /* count */);
  }

  // Test adding a cookie for a new key.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain4.test"),
                                   "A=1", CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 4 /* sample */,
                                        1 /* count */);
  }

  // Test overwriting the only cookie for a key. (Deletes and inserts, so the
  // total doesn't change.)
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain4.test"),
                                   "A=2", CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 4 /* sample */,
                                        1 /* count */);
  }

  // Test deleting cookie for a key with more than one cookie.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain2.test"),
                                   "A=1; Max-Age=0",
                                   CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 4 /* sample */,
                                        1 /* count */);
  }

  // Test deleting cookie for a key with only one cookie.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain4.test"),
                                   "A=1; Max-Age=0",
                                   CookieOptions::MakeAllInclusive()));
    EXPECT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 3 /* sample */,
                                        1 /* count */);
  }
}

TEST_F(CookieMonsterTest, CookieCount2Histogram) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample("Cookie.Count2",
                                        /*sample=*/0,
                                        /*expected_bucket_count=*/1);
  }

  {
    base::HistogramTester histogram_tester;

    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "a", "b", "a.url", "/", base::Time(),
        base::Time::Now() + base::Minutes(59), base::Time(), base::Time(),
        /*secure=*/true,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT);
    GURL source_url = cookie_util::SimulatedCookieSource(*cookie, "https");
    ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cookie), source_url,
                                   /*modify_httponly=*/true));

    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    histogram_tester.ExpectUniqueSample("Cookie.Count2", /*sample=*/1,
                                        /*expected_bucket_count=*/1);
  }
}

TEST_F(CookieMonsterTest, CookieJarSizeHistograms) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample("Cookie.CookieJarSize",
                                        /*sample=*/0,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.AvgCookieJarSizePerKey2",
                                        /*sample=*/0,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.MaxCookieJarSizePerKey",
                                        /*sample=*/0,
                                        /*expected_bucket_count=*/1);
  }

  auto set_cookie =
      [&](const std::string& name, int cookie_value_size_kb,
          const std::string& domain, CookieSameSite same_site,
          const std::optional<CookiePartitionKey>& partition_key) {
        auto cc = CanonicalCookie::CreateUnsafeCookieForTesting(
            name, std::string(cookie_value_size_kb * 1024, '0'), domain, "/",
            base::Time(), base::Time::Now() + base::Minutes(59), base::Time(),
            base::Time(),
            /*secure=*/true,
            /*httponly=*/false, same_site, COOKIE_PRIORITY_DEFAULT,
            partition_key);
        GURL source_url = cookie_util::SimulatedCookieSource(*cc, "https");
        ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cc), source_url,
                                       /*can_modify_httponly=*/true));
      };

  {  // Add unpartitioned cookie.
    base::HistogramTester histogram_tester;
    set_cookie("a", 2, "a.url", CookieSameSite::NO_RESTRICTION, std::nullopt);
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    histogram_tester.ExpectUniqueSample("Cookie.CookieJarSize",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/1);
    // Recorded in bytes.
    histogram_tester.ExpectUniqueSample("Cookie.AvgCookieJarSizePerKey2",
                                        /*sample=*/2049,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.MaxCookieJarSizePerKey",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/1);
  }

  {  // Add partitioned cookie, should not impact the counter.
    base::HistogramTester histogram_tester;
    set_cookie("b", 3, "a.url", CookieSameSite::NO_RESTRICTION,
               CookiePartitionKey::FromURLForTesting(
                   GURL("https://toplevelsite.com")));
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    histogram_tester.ExpectUniqueSample("Cookie.CookieJarSize",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/1);
    // Recorded in bytes.
    histogram_tester.ExpectUniqueSample("Cookie.AvgCookieJarSizePerKey2",
                                        /*sample=*/2049,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.MaxCookieJarSizePerKey",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/1);
  }

  {  // Add unpartitioned cookie from another domain. Is also SameSite=Lax to
     // ensure the counter includes SameSite cookies.
    base::HistogramTester histogram_tester;
    set_cookie("c", 4, "c.url", CookieSameSite::LAX_MODE, std::nullopt);
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    histogram_tester.ExpectUniqueSample("Cookie.CookieJarSize",
                                        /*sample=*/6,
                                        /*expected_bucket_count=*/1);
    // Recorded in bytes.
    histogram_tester.ExpectUniqueSample("Cookie.AvgCookieJarSizePerKey2",
                                        /*sample=*/3073,
                                        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.MaxCookieJarSizePerKey",
                                        /*sample=*/4,
                                        /*expected_bucket_count=*/1);
  }
}

TEST_F(CookieMonsterTest, PartitionedCookieHistograms) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    // Cookie counters.
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount",
                                        /*sample=*/0,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount.Nonced",
                                        /*sample=*/0,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieCount.Unnonced", /*sample=*/0,
        /*count=*/1);

    // Partitioned cookie jar size.
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes",
        /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Nonced", /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Unnonced", /*sample=*/0,
        /*count=*/1);

    // Partitioned cookie jar size per partition.
    histogram_tester.ExpectUniqueSample("Cookie.CookiePartitionSizeKibibytes",
                                        /*sample=*/0,
                                        /*count=*/0);
  }

  {  // Add unpartitioned cookie.
    base::HistogramTester histogram_tester;
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "a", "b", "a.url", "/", base::Time(),
        base::Time::Now() + base::Minutes(59), base::Time(), base::Time(),
        /*secure=*/true,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT);
    GURL source_url = cookie_util::SimulatedCookieSource(*cookie, "https");
    ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cookie), source_url,
                                   /*modify_httponly=*/true));
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    // Cookie counters.
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount",
                                        /*sample=*/0,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount.Nonced",
                                        /*sample=*/0,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieCount.Unnonced", /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.Count2", /*sample=*/1,
                                        /*count=*/1);

    // Partitioned cookie jar size.
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes",
        /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Nonced", /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Unnonced", /*sample=*/0,
        /*count=*/1);

    // Partitioned cookie jar size per partition.
    histogram_tester.ExpectUniqueSample("Cookie.CookiePartitionSizeKibibytes",
                                        /*sample=*/0,
                                        /*count=*/0);
  }

  {  // Add unnonced partitioned cookie.
    base::HistogramTester histogram_tester;
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "a", std::string(2 * 1024, '0'), "a.url", "/", base::Time(),
        base::Time::Now() + base::Minutes(59), base::Time(), base::Time(),
        /*secure=*/true,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT,
        CookiePartitionKey::FromURLForTesting(GURL("https://example.com")));
    GURL source_url = cookie_util::SimulatedCookieSource(*cookie, "https");
    ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cookie), source_url,
                                   /*modify_httponly=*/true));
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    // Cookie counters.
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount",
                                        /*sample=*/1,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount.Nonced",
                                        /*sample=*/0,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieCount.Unnonced", /*sample=*/1,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.Count2", /*sample=*/1,
                                        /*count=*/1);

    // Partitioned cookie jar size.
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes",
        /*sample=*/2,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Nonced", /*sample=*/0,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Unnonced", /*sample=*/2,
        /*count=*/1);

    // Partitioned cookie jar size per partition.
    histogram_tester.ExpectUniqueSample("Cookie.CookiePartitionSizeKibibytes",
                                        /*sample=*/2,
                                        /*count=*/1);
  }

  {  // Add nonced partitioned cookie.
    base::HistogramTester histogram_tester;
    auto cookie = CanonicalCookie::CreateUnsafeCookieForTesting(
        "a", std::string(3 * 1024, '0'), "a.url", "/", base::Time(),
        base::Time::Now() + base::Minutes(59), base::Time(), base::Time(),
        /*secure=*/true,
        /*httponly=*/false, CookieSameSite::NO_RESTRICTION,
        COOKIE_PRIORITY_DEFAULT,
        CookiePartitionKey::FromURLForTesting(
            GURL("https://example.com"),
            CookiePartitionKey::AncestorChainBit::kCrossSite,
            base::UnguessableToken::Create()));
    GURL source_url = cookie_util::SimulatedCookieSource(*cookie, "https");
    ASSERT_TRUE(SetCanonicalCookie(cm.get(), std::move(cookie), source_url,
                                   /*modify_httponly=*/true));
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());

    // Cookie counts.
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount",
                                        /*sample=*/2,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.PartitionedCookieCount.Nonced",
                                        /*sample=*/1,
                                        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieCount.Unnonced", /*sample=*/1,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample("Cookie.Count2", /*sample=*/1,
                                        /*count=*/1);

    // Partitioned cookie jar size.
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes",
        /*sample=*/5,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Nonced", /*sample=*/3,
        /*count=*/1);
    histogram_tester.ExpectUniqueSample(
        "Cookie.PartitionedCookieJarSizeKibibytes.Unnonced", /*sample=*/2,
        /*count=*/1);

    // Partitioned cookie jar size per partition.
    histogram_tester.ExpectBucketCount("Cookie.CookiePartitionSizeKibibytes",
                                       /*sample=*/2,
                                       /*count=*/1);
    histogram_tester.ExpectBucketCount("Cookie.CookiePartitionSizeKibibytes",
                                       /*sample=*/3,
                                       /*count=*/1);
  }
}

TEST_F(CookieMonsterTest, MaxSameSiteNoneCookiesPerKey) {
  const char kHistogramName[] = "Cookie.MaxSameSiteNoneCookiesPerKey";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  ASSERT_EQ(0u, GetAllCookies(cm.get()).size());

  {  // Only SameSite cookies should not log a sample.
    base::HistogramTester histogram_tester;

    ASSERT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain1.test"),
                                   "A=1;SameSite=Lax",
                                   CookieOptions::MakeAllInclusive()));
    ASSERT_EQ(1u, GetAllCookies(cm.get()).size());
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 0 /* sample */,
                                        1 /* count */);
  }

  {  // SameSite=None cookie should log a sample.
    base::HistogramTester histogram_tester;

    ASSERT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain1.test"),
                                   "B=2;SameSite=None;Secure",
                                   CookieOptions::MakeAllInclusive()));
    ASSERT_EQ(2u, GetAllCookies(cm.get()).size());
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 1 /* sample */,
                                        1 /* count */);
  }

  {  // Should log the maximum number of SameSite=None cookies.
    base::HistogramTester histogram_tester;

    ASSERT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain2.test"),
                                   "A=1;SameSite=None;Secure",
                                   CookieOptions::MakeAllInclusive()));
    ASSERT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain2.test"),
                                   "B=2;SameSite=None;Secure",
                                   CookieOptions::MakeAllInclusive()));
    ASSERT_TRUE(CreateAndSetCookie(cm.get(), GURL("https://domain3.test"),
                                   "A=1;SameSite=None;Secure",
                                   CookieOptions::MakeAllInclusive()));
    ASSERT_EQ(5u, GetAllCookies(cm.get()).size());
    ASSERT_TRUE(cm->DoRecordPeriodicStatsForTesting());
    histogram_tester.ExpectUniqueSample(kHistogramName, 2 /* sample */,
                                        1 /* count */);
  }
}

// Test that localhost URLs can set and get secure cookies, even if
// non-cryptographic.
TEST_F(CookieMonsterTest, SecureCookieLocalhost) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);

  GURL insecure_localhost("http://localhost");
  GURL secure_localhost("https://localhost");

  // Insecure localhost can set secure cookie, and warning is attached to
  // status.
  {
    auto cookie = CanonicalCookie::CreateForTesting(
        insecure_localhost, "from_insecure_localhost=1; Secure",
        base::Time::Now());
    ASSERT_TRUE(cookie);
    CookieInclusionStatus status =
        SetCanonicalCookieReturnAccessResult(cm.get(), std::move(cookie),
                                             insecure_localhost,
                                             true /* can_modify_httponly */)
            .status;
    EXPECT_TRUE(status.IsInclude());
    EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
        {CookieInclusionStatus::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC}));
  }
  // Secure localhost can set secure cookie, and warning is not attached to
  // status.
  {
    auto cookie = CanonicalCookie::CreateForTesting(
        secure_localhost, "from_secure_localhost=1; Secure", base::Time::Now());
    ASSERT_TRUE(cookie);
    CookieInclusionStatus status =
        SetCanonicalCookieReturnAccessResult(cm.get(), std::move(cookie),
                                             secure_localhost,
                                             true /* can_modify_httponly */)
            .status;
    EXPECT_EQ(CookieInclusionStatus(), status);
  }

  // Insecure localhost can get secure cookies, and warning is attached to
  // status.
  {
    GetCookieListCallback callback;
    cm->GetCookieListWithOptionsAsync(
        insecure_localhost, CookieOptions::MakeAllInclusive(),
        CookiePartitionKeyCollection(), callback.MakeCallback());
    callback.WaitUntilDone();
    EXPECT_EQ(2u, callback.cookies_with_access_results().size());
    for (const auto& cookie_item : callback.cookies_with_access_results()) {
      EXPECT_TRUE(cookie_item.cookie.SecureAttribute());
      EXPECT_TRUE(cookie_item.access_result.status.IsInclude());
      EXPECT_TRUE(
          cookie_item.access_result.status.HasExactlyWarningReasonsForTesting(
              {CookieInclusionStatus::
                   WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC}));
    }
  }
  // Secure localhost can get secure cookies, and warning is not attached to
  // status.
  {
    GetCookieListCallback callback;
    cm->GetCookieListWithOptionsAsync(
        secure_localhost, CookieOptions::MakeAllInclusive(),
        CookiePartitionKeyCollection(), callback.MakeCallback());
    callback.WaitUntilDone();
    EXPECT_EQ(2u, callback.cookies_with_access_results().size());
    for (const auto& cookie_item : callback.cookies_with_access_results()) {
      EXPECT_TRUE(cookie_item.cookie.SecureAttribute());
      EXPECT_EQ(CookieInclusionStatus(), cookie_item.access_result.status);
    }
  }
}

TEST_F(CookieMonsterTest, MaybeDeleteEquivalentCookieAndUpdateStatus) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Set a secure, httponly cookie from a secure origin
  auto preexisting_cookie = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "A=B;Secure;HttpOnly", base::Time::Now());
  CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(preexisting_cookie), https_www_foo_.url(),
      true /* can_modify_httponly */);
  ASSERT_TRUE(access_result.status.IsInclude());

  // Set a new cookie with a different name. Should work because cookies with
  // different names are not considered equivalent nor "equivalent for secure
  // cookie matching".
  // Same origin:
  EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), "B=A;"));
  // Different scheme, same domain:
  EXPECT_TRUE(SetCookie(cm.get(), http_www_foo_.url(), "C=A;"));

  // Set a non-Secure cookie from an insecure origin that is
  // equivalent to the pre-existing Secure cookie.
  auto bad_cookie = CanonicalCookie::CreateForTesting(http_www_foo_.url(),
                                                      "A=D", base::Time::Now());
  // Allow modifying HttpOnly, so that we don't skip preexisting cookies for
  // being HttpOnly.
  access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(bad_cookie), http_www_foo_.url(),
      true /* can_modify_httponly */);
  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  // The preexisting cookie should still be there.
  EXPECT_THAT(GetCookiesWithOptions(cm.get(), https_www_foo_.url(),
                                    CookieOptions::MakeAllInclusive()),
              ::testing::HasSubstr("A=B"));

  auto entries = net_log_.GetEntries();
  size_t skipped_secure_netlog_index = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE,
      NetLogEventPhase::NONE);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY));
  ExpectLogContainsSomewhereAfter(
      entries, skipped_secure_netlog_index,
      NetLogEventType::COOKIE_STORE_COOKIE_PRESERVED_SKIPPED_SECURE,
      NetLogEventPhase::NONE);

  net_log_.Clear();

  // Set a non-secure cookie from an insecure origin that matches the name of an
  // already existing cookie but is not equivalent. This should fail since it's
  // trying to shadow a secure cookie.
  bad_cookie = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), "A=E; path=/some/path", base::Time::Now());
  // Allow modifying HttpOnly, so that we don't skip preexisting cookies for
  // being HttpOnly.
  access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(bad_cookie), http_www_foo_.url(),
      true /* can_modify_httponly */);
  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  // The preexisting cookie should still be there.
  EXPECT_THAT(GetCookiesWithOptions(cm.get(), https_www_foo_.url(),
                                    CookieOptions::MakeAllInclusive()),
              ::testing::HasSubstr("A=B"));

  entries = net_log_.GetEntries();
  skipped_secure_netlog_index = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE,
      NetLogEventPhase::NONE);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY));
  // There wasn't actually a strictly equivalent cookie that we would have
  // deleted.
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, skipped_secure_netlog_index,
      NetLogEventType::COOKIE_STORE_COOKIE_PRESERVED_SKIPPED_SECURE));

  net_log_.Clear();

  // Test skipping equivalent cookie for HttpOnly only.
  bad_cookie = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "A=E; Secure", base::Time::Now());
  access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(bad_cookie), https_www_foo_.url(),
      false /* can_modify_httponly */);
  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY}));

  entries = net_log_.GetEntries();
  ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY,
      NetLogEventPhase::NONE);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE));
}

TEST_F(CookieMonsterTest,
       MaybeDeleteEquivalentCookieAndUpdateStatus_PartitionedCookies) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Test adding two cookies with the same name, domain, and path but different
  // partition keys.
  auto cookie_partition_key1 =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite1.com"));

  auto preexisting_cookie = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "__Host-A=B; Secure; Path=/; Partitioned; HttpOnly",
      base::Time::Now(), std::nullopt /* server_time */,
      cookie_partition_key1 /* cookie_partition_key */);
  CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(preexisting_cookie), https_www_foo_.url(),
      true /* can_modify_httponly */);
  ASSERT_TRUE(access_result.status.IsInclude());

  // Should be able to set a cookie with a different partition key.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(),
                        "__Host-A=C; Secure; Path=/; Partitioned",
                        CookiePartitionKey::FromURLForTesting(
                            GURL("https://toplevelsite2.com"))));

  // Should not overwrite HttpOnly cookie.
  auto bad_cookie = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "__Host-A=D; Secure; Path=/; Partitioned",
      base::Time::Now(), std::nullopt /* server_time */, cookie_partition_key1);
  access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(bad_cookie), https_www_foo_.url(),
      false /* can_modify_httponly */);
  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY}));
  EXPECT_THAT(
      GetCookiesWithOptions(
          cm.get(), https_www_foo_.url(), CookieOptions::MakeAllInclusive(),
          CookiePartitionKeyCollection(cookie_partition_key1)),
      ::testing::HasSubstr("A=B"));
}

// Tests whether cookies that vary based on their source scheme/port are
// overwritten correctly depending on the state of the origin-bound feature
// flags.
class CookieMonsterTest_MaybeDeleteEquivalentCookieAndUpdateStatus
    : public CookieMonsterTest {
 public:
  // Creates a store, CookieMonster, and inserts a single cookie, created on an
  // https/443 origin.
  void InitializeTest() {
    store_ = base::MakeRefCounted<MockPersistentCookieStore>();
    cm_ = std::make_unique<CookieMonster>(store_.get(), net::NetLog::Get());

    auto preexisting_cookie_https = CanonicalCookie::CreateForTesting(
        https_www_foo_.url(), "A=PreexistingHttps443", base::Time::Now());

    CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
        cm_.get(), std::move(preexisting_cookie_https), https_www_foo_.url(),
        /*can_modify_httponly=*/true);
    ASSERT_TRUE(access_result.status.IsInclude());

    auto preexisting_domain_cookie_https = CanonicalCookie::CreateForTesting(
        https_www_foo_.url(),
        "A=PreexistingDomainHttps443; Domain=" + https_www_foo_.domain(),
        base::Time::Now());

    access_result = SetCanonicalCookieReturnAccessResult(
        cm_.get(), std::move(preexisting_domain_cookie_https),
        https_www_foo_.url(),
        /*can_modify_httponly=*/true);
    ASSERT_TRUE(access_result.status.IsInclude());

    ASSERT_EQ(GetAllCookies(cm_.get()).size(), 2UL);
  }

  // Inserts a single cookie that differs from "PreexistingHttps443" by scheme
  // only.
  void AddHttpPort443Cookie() {
    GURL::Replacements replace_scheme;
    replace_scheme.SetSchemeStr("http");
    // We need to explicitly set the existing port, otherwise GURL will
    // implicitly take the port of the new scheme. I.e.: We'll inadvertently
    // change the port to 80.
    replace_scheme.SetPortStr("443");
    GURL foo_made_http = https_www_foo_.url().ReplaceComponents(replace_scheme);

    auto differ_by_scheme_only = CanonicalCookie::CreateForTesting(
        foo_made_http, "A=InsertedHttp443", base::Time::Now());

    CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
        cm_.get(), std::move(differ_by_scheme_only), foo_made_http,
        /*can_modify_httponly=*/true);
    ASSERT_TRUE(access_result.status.IsInclude());
  }

  // Inserts a single cookie that differs from "PreexistingHttps443" by port
  // only.
  void AddHttpsPort80Cookie() {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("80");
    GURL foo_made_80 = https_www_foo_.url().ReplaceComponents(replace_port);

    auto differ_by_port_only = CanonicalCookie::CreateForTesting(
        foo_made_80, "A=InsertedHttps80", base::Time::Now());

    CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
        cm_.get(), std::move(differ_by_port_only), foo_made_80,
        /*can_modify_httponly=*/true);
    ASSERT_TRUE(access_result.status.IsInclude());
  }

  // Inserts a single Domain cookie that differs from
  // "PreexistingDomainHttps443" by port only.
  void AddDomainHttpsPort80Cookie() {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("80");
    GURL foo_made_80 = https_www_foo_.url().ReplaceComponents(replace_port);

    auto differ_by_port_only = CanonicalCookie::CreateForTesting(
        foo_made_80,
        "A=InsertedDomainHttps80; Domain=" + https_www_foo_.domain(),
        base::Time::Now());

    CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
        cm_.get(), std::move(differ_by_port_only), foo_made_80,
        /*can_modify_httponly=*/true);
    ASSERT_TRUE(access_result.status.IsInclude());
  }

  scoped_refptr<net::MockPersistentCookieStore> store_;
  std::unique_ptr<CookieMonster> cm_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Scheme binding disabled.
// Port binding disabled.
// Cookies that differ only in their scheme and/or port should overwrite the
// preexisting cookies.
TEST_F(CookieMonsterTest_MaybeDeleteEquivalentCookieAndUpdateStatus,
       NoSchemeNoPort) {
  scoped_feature_list_.InitWithFeatures(
      {}, {net::features::kEnableSchemeBoundCookies,
           net::features::kEnablePortBoundCookies});

  InitializeTest();

  AddHttpPort443Cookie();

  auto cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddDomainHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "InsertedDomainHttps80")));
}

// Scheme binding enabled.
// Port binding disabled.
// Cookies that differ in scheme are separate, cookies that differ only by
// port should be overwritten.
TEST_F(CookieMonsterTest_MaybeDeleteEquivalentCookieAndUpdateStatus,
       YesSchemeNoPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnableSchemeBoundCookies},
      {net::features::kEnablePortBoundCookies});

  InitializeTest();

  AddHttpPort443Cookie();

  auto cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "PreexistingHttps443"),
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddDomainHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "InsertedDomainHttps80")));
}

// Scheme binding disabled.
// Port binding enabled.
// Cookies that differ only by scheme and Domain cookies that differ only by
// port should be overwritten. Host cookies that differ only by port are
// separate.
TEST_F(CookieMonsterTest_MaybeDeleteEquivalentCookieAndUpdateStatus,
       NoSchemeYesPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnablePortBoundCookies},
      {net::features::kEnableSchemeBoundCookies});

  InitializeTest();

  AddHttpPort443Cookie();

  auto cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddDomainHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "InsertedDomainHttps80")));
}

// Scheme binding enabled.
// Port binding enabled.
// Cookies that differ by port or scheme are separate. Except for Domain cookies
// which will be overwritten if they differ only by port.
TEST_F(CookieMonsterTest_MaybeDeleteEquivalentCookieAndUpdateStatus,
       YesSchemeYesPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnableSchemeBoundCookies,
       net::features::kEnablePortBoundCookies},
      {});

  InitializeTest();

  AddHttpPort443Cookie();

  auto cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "PreexistingHttps443"),
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "PreexistingHttps443"),
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "PreexistingDomainHttps443")));

  AddDomainHttpsPort80Cookie();

  cookies = GetAllCookies(cm_.get());
  EXPECT_THAT(cookies,
              testing::UnorderedElementsAre(
                  MatchesCookieNameValue("A", "PreexistingHttps443"),
                  MatchesCookieNameValue("A", "InsertedHttp443"),
                  MatchesCookieNameValue("A", "InsertedHttps80"),
                  MatchesCookieNameValue("A", "InsertedDomainHttps80")));
}

// Tests that only the correct set of (potentially duplicate) cookies are loaded
// from the backend store depending on the state of the origin-bound feature
// flags.
class CookieMonsterTest_StoreLoadedCookies : public CookieMonsterTest {
 public:
  void InitializeTest() {
    store_ = base::MakeRefCounted<MockPersistentCookieStore>();
    cm_ = std::make_unique<CookieMonster>(store_.get(), net::NetLog::Get());

    base::Time most_recent_time = base::Time::Now();
    base::Time middle_time = most_recent_time - base::Minutes(1);
    base::Time least_recent_time = middle_time - base::Minutes(1);

    auto basic_cookie = CanonicalCookie::CreateForTesting(
        https_www_foo_.url(), "A=basic", base::Time::Now());

    // When there are duplicate cookies the most recent one is kept. So, this
    // one.
    basic_cookie->SetCreationDate(most_recent_time);
    starting_list_.push_back(std::move(basic_cookie));

    GURL::Replacements replace_scheme;
    replace_scheme.SetSchemeStr("http");
    // We need to explicitly set the existing port, otherwise GURL will
    // implicitly take the port of the new scheme. I.e.: We'll inadvertently
    // change the port to 80.
    replace_scheme.SetPortStr("443");
    GURL foo_with_http = https_www_foo_.url().ReplaceComponents(replace_scheme);

    auto http_cookie = CanonicalCookie::CreateForTesting(
        foo_with_http, "A=http", base::Time::Now());

    http_cookie->SetCreationDate(middle_time);
    starting_list_.push_back(std::move(http_cookie));

    GURL::Replacements replace_port;
    replace_port.SetPortStr("450");
    GURL foo_with_450 = https_www_foo_.url().ReplaceComponents(replace_port);

    auto port_450_cookie = CanonicalCookie::CreateForTesting(
        foo_with_450, "A=port450", base::Time::Now());
    port_450_cookie->SetCreationDate(least_recent_time);
    starting_list_.push_back(std::move(port_450_cookie));

    auto basic_domain_cookie = CanonicalCookie::CreateForTesting(
        https_www_foo_.url(),
        "A=basic_domain; Domain=" + https_www_foo_.domain(), base::Time::Now());

    // When there are duplicate domain cookies the most recent one is kept. So,
    // this one.
    basic_domain_cookie->SetCreationDate(most_recent_time);
    starting_list_.push_back(std::move(basic_domain_cookie));

    auto http_domain_cookie = CanonicalCookie::CreateForTesting(
        foo_with_http, "A=http_domain; Domain=" + https_www_foo_.domain(),
        base::Time::Now());

    http_domain_cookie->SetCreationDate(middle_time);
    starting_list_.push_back(std::move(http_domain_cookie));

    // Domain cookies don't consider the port, so this cookie should always be
    // considered a duplicate.
    auto port_450_domain_cookie = CanonicalCookie::CreateForTesting(
        foo_with_450, "A=port450_domain; Domain=" + https_www_foo_.domain(),
        base::Time::Now());
    port_450_domain_cookie->SetCreationDate(least_recent_time);
    starting_list_.push_back(std::move(port_450_domain_cookie));

    ASSERT_EQ(starting_list_.size(), 6UL);
  }

  scoped_refptr<net::MockPersistentCookieStore> store_;
  std::unique_ptr<CookieMonster> cm_;
  std::vector<std::unique_ptr<CanonicalCookie>> starting_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Scheme binding disabled.
// Port binding disabled.
// Only 2 cookies, the most recently created, should exist.
TEST_F(CookieMonsterTest_StoreLoadedCookies, NoSchemeNoPort) {
  scoped_feature_list_.InitWithFeatures(
      {}, {net::features::kEnableSchemeBoundCookies,
           net::features::kEnablePortBoundCookies});
  InitializeTest();
  cm_->StoreLoadedCookies(std::move(starting_list_));
  auto cookies = GetAllCookies(cm_.get());

  EXPECT_THAT(cookies, testing::UnorderedElementsAre(
                           MatchesCookieNameValue("A", "basic"),
                           MatchesCookieNameValue("A", "basic_domain")));
}

// Scheme binding enabled.
// Port binding disabled.
// 4 Cookies should exist.
TEST_F(CookieMonsterTest_StoreLoadedCookies, YesSchemeNoPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnableSchemeBoundCookies},
      {net::features::kEnablePortBoundCookies});
  InitializeTest();
  cm_->StoreLoadedCookies(std::move(starting_list_));
  auto cookies = GetAllCookies(cm_.get());

  EXPECT_THAT(cookies, testing::UnorderedElementsAre(
                           MatchesCookieNameValue("A", "basic"),
                           MatchesCookieNameValue("A", "http"),
                           MatchesCookieNameValue("A", "basic_domain"),
                           MatchesCookieNameValue("A", "http_domain")));
}

// Scheme binding disabled.
// Port binding enabled.
// 3 Cookies should exist.
TEST_F(CookieMonsterTest_StoreLoadedCookies, NoSchemeYesPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnablePortBoundCookies},
      {net::features::kEnableSchemeBoundCookies});
  InitializeTest();
  cm_->StoreLoadedCookies(std::move(starting_list_));
  auto cookies = GetAllCookies(cm_.get());

  // Domain cookies aren't bound to a port by design, so duplicates across ports
  // should still be removed. I.e.: "A=port450_domain"
  EXPECT_THAT(cookies, testing::UnorderedElementsAre(
                           MatchesCookieNameValue("A", "basic"),
                           MatchesCookieNameValue("A", "port450"),
                           MatchesCookieNameValue("A", "basic_domain")));
}

// Scheme binding enabled.
// Port binding enabled.
// 5 Cookies should exist.
TEST_F(CookieMonsterTest_StoreLoadedCookies, YesSchemeYesPort) {
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnablePortBoundCookies,
       net::features::kEnableSchemeBoundCookies},
      {});

  InitializeTest();
  cm_->StoreLoadedCookies(std::move(starting_list_));
  auto cookies = GetAllCookies(cm_.get());

  // Domain cookies aren't bound to a port by design, so duplicates across ports
  // should still be removed. I.e.: "A=port450_domain"
  EXPECT_THAT(cookies, testing::UnorderedElementsAre(
                           MatchesCookieNameValue("A", "basic"),
                           MatchesCookieNameValue("A", "http"),
                           MatchesCookieNameValue("A", "port450"),
                           MatchesCookieNameValue("A", "basic_domain"),
                           MatchesCookieNameValue("A", "http_domain")));
}

// Test skipping a cookie in MaybeDeleteEquivalentCookieAndUpdateStatus for
// multiple reasons (Secure and HttpOnly).
TEST_F(CookieMonsterTest, SkipDontOverwriteForMultipleReasons) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  // Set a secure, httponly cookie from a secure origin
  auto preexisting_cookie = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "A=B;Secure;HttpOnly", base::Time::Now());
  CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(preexisting_cookie), https_www_foo_.url(),
      true /* can_modify_httponly */);
  ASSERT_TRUE(access_result.status.IsInclude());

  // Attempt to set a new cookie with the same name that is not Secure or
  // Httponly from an insecure scheme.
  auto cookie = CanonicalCookie::CreateForTesting(http_www_foo_.url(), "A=B",
                                                  base::Time::Now());
  access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(cookie), http_www_foo_.url(),
      false /* can_modify_httponly */);
  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE,
       CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY}));

  auto entries = net_log_.GetEntries();
  ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_SECURE,
      NetLogEventPhase::NONE);
  ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY,
      NetLogEventPhase::NONE);
}

// Test that when we check for equivalent cookies, we don't remove any if the
// cookie should not be set.
TEST_F(CookieMonsterTest, DontDeleteEquivalentCookieIfSetIsRejected) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  auto preexisting_cookie = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), "cookie=foo", base::Time::Now());
  CookieAccessResult access_result = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(preexisting_cookie), http_www_foo_.url(),
      false /* can_modify_httponly */);
  ASSERT_TRUE(access_result.status.IsInclude());

  auto bad_cookie = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), "cookie=bar;secure", base::Time::Now());
  CookieAccessResult access_result2 = SetCanonicalCookieReturnAccessResult(
      cm.get(), std::move(bad_cookie), http_www_foo_.url(),
      false /* can_modify_httponly */);
  EXPECT_TRUE(access_result2.status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // Check that the original cookie is still there.
  EXPECT_EQ("cookie=foo", GetCookies(cm.get(), https_www_foo_.url()));
}

TEST_F(CookieMonsterTest, SetSecureCookies) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  GURL http_url("http://www.foo.com");
  GURL http_superdomain_url("http://foo.com");
  GURL https_url("https://www.foo.com");
  GURL https_foo_url("https://www.foo.com/foo");
  GURL http_foo_url("http://www.foo.com/foo");

  // A non-secure cookie can be created from either a URL with a secure or
  // insecure scheme.
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=C;").IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B;").IsInclude());

  // A secure cookie cannot be set from a URL with an insecure scheme.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=B; Secure")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // A secure cookie can be set from a URL with a secure scheme.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());

  // If a non-secure cookie is created from a URL with an insecure scheme, and a
  // secure cookie with the same name already exists, do not update the cookie.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=C;")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));

  // If a non-secure cookie is created from a URL with an secure scheme, and a
  // secure cookie with the same name already exists, update the cookie.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=C;").IsInclude());

  // If a non-secure cookie is created from a URL with an insecure scheme, and
  // a secure cookie with the same name already exists, do not update the cookie
  // if the new cookie's path matches the existing cookie's path.
  //
  // With an existing cookie whose path is '/', a cookie with the same name
  // cannot be set on the same domain, regardless of path:
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=C; path=/")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=C; path=/my/path")
          .HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));

  // But if the existing cookie has a path somewhere under the root, cookies
  // with the same name may be set for paths which don't overlap the existing
  // cookie.
  EXPECT_TRUE(
      SetCookie(cm.get(), https_url, "WITH_PATH=B; Secure; path=/my/path"));
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "WITH_PATH=C")
                  .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "WITH_PATH=C; path=/")
          .IsInclude());
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url,
                                             "WITH_PATH=C; path=/your/path")
                  .IsInclude());
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url,
                                             "WITH_PATH=C; path=/my/path")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url,
                                             "WITH_PATH=C; path=/my/path/sub")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));

  DeleteAll(cm.get());

  // If a secure cookie is set on top of an existing insecure cookie but with a
  // different path, both are retained.
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=B; path=/foo")
          .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=C; Secure; path=/")
          .IsInclude());

  // Querying from an insecure url gets only the insecure cookie, but querying
  // from a secure url returns both.
  EXPECT_EQ("A=B", GetCookies(cm.get(), http_foo_url));
  EXPECT_THAT(GetCookies(cm.get(), https_foo_url), testing::HasSubstr("A=B"));
  EXPECT_THAT(GetCookies(cm.get(), https_foo_url), testing::HasSubstr("A=C"));

  // Attempting to set an insecure cookie (from an insecure scheme) that domain-
  // matches and path-matches the secure cookie fails i.e. the secure cookie is
  // left alone...
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=D; path=/foo")
          .HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=D; path=/")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_THAT(GetCookies(cm.get(), https_foo_url), testing::HasSubstr("A=C"));

  // ...but the original insecure cookie is still retained.
  EXPECT_THAT(GetCookies(cm.get(), https_foo_url), testing::HasSubstr("A=B"));
  EXPECT_THAT(GetCookies(cm.get(), https_foo_url),
              testing::Not(testing::HasSubstr("A=D")));

  // Deleting the secure cookie leaves only the original insecure cookie.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                  cm.get(), https_url,
                  "A=C; path=/; Expires=Thu, 01-Jan-1970 00:00:01 GMT")
                  .IsInclude());
  EXPECT_EQ("A=B", GetCookies(cm.get(), https_foo_url));

  // If a non-secure cookie is created from a URL with an insecure scheme, and
  // a secure cookie with the same name already exists, if the domain strings
  // domain-match, do not update the cookie.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=C; domain=foo.com")
          .HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url,
                                             "A=C; domain=www.foo.com")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));

  // Since A=B was set above with no domain string, set a different cookie here
  // so the insecure examples aren't trying to overwrite the one above.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url,
                                             "B=C; Secure; domain=foo.com")
                  .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_url, "B=D; domain=foo.com")
          .HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "B=D")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), http_superdomain_url, "B=D")
          .HasExactlyExclusionReasonsForTesting(
              {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));

  // Verify that if an httponly version of the cookie exists, adding a Secure
  // version of the cookie still does not overwrite it.
  CookieOptions include_httponly = CookieOptions::MakeAllInclusive();
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), https_url, "C=D; httponly",
                                 include_httponly));
  // Note that the lack of an explicit options object below uses the default,
  // which in this case includes "exclude_httponly = true".
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "C=E; Secure")
                  .HasExactlyExclusionReasonsForTesting(
                      {CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY}));

  auto entries = net_log_.GetEntries();
  ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::COOKIE_STORE_COOKIE_REJECTED_HTTPONLY,
      NetLogEventPhase::NONE);
}

// Tests the behavior of "Leave Secure Cookies Alone" in
// MaybeDeleteEquivalentCookieAndUpdateStatus().
// Check domain-match criterion: If either cookie domain matches the other,
// don't set the insecure cookie.
TEST_F(CookieMonsterTest, LeaveSecureCookiesAlone_DomainMatch) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // These domains will domain-match each other.
  const char* kRegistrableDomain = "foo.com";
  const char* kSuperdomain = "a.foo.com";
  const char* kDomain = "b.a.foo.com";
  const char* kSubdomain = "c.b.a.foo.com";
  // This domain does not match any, aside from the registrable domain.
  const char* kAnotherDomain = "z.foo.com";

  for (const char* preexisting_cookie_host :
       {kRegistrableDomain, kSuperdomain, kDomain, kSubdomain}) {
    GURL preexisting_cookie_url(
        base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                      preexisting_cookie_host}));
    for (const char* new_cookie_host :
         {kRegistrableDomain, kSuperdomain, kDomain, kSubdomain}) {
      GURL https_url(base::StrCat(
          {url::kHttpsScheme, url::kStandardSchemeSeparator, new_cookie_host}));
      GURL http_url(base::StrCat(
          {url::kHttpScheme, url::kStandardSchemeSeparator, new_cookie_host}));

      // Preexisting Secure host and domain cookies.
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), preexisting_cookie_url, "A=0; Secure")
                      .IsInclude());
      EXPECT_TRUE(
          CreateAndSetCookieReturnStatus(
              cm.get(), preexisting_cookie_url,
              base::StrCat({"B=0; Secure; Domain=", preexisting_cookie_host}))
              .IsInclude());

      // Don't set insecure cookie from an insecure URL if equivalent secure
      // cookie exists.
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=1")
                      .HasExactlyExclusionReasonsForTesting(
                          {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}))
          << "Insecure host cookie from " << http_url
          << " should not be set if equivalent secure host cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), http_url,
                      base::StrCat({"A=2; Domain=", new_cookie_host}))
                      .HasExactlyExclusionReasonsForTesting(
                          {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}))
          << "Insecure domain cookie from " << http_url
          << " should not be set if equivalent secure host cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), http_url, "B=1")
                      .HasExactlyExclusionReasonsForTesting(
                          {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}))
          << "Insecure host cookie from " << http_url
          << " should not be set if equivalent secure domain cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), http_url,
                      base::StrCat({"B=2; Domain=", new_cookie_host}))
                      .HasExactlyExclusionReasonsForTesting(
                          {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}))
          << "Insecure domain cookie from " << http_url
          << " should not be set if equivalent secure domain cookie from "
          << preexisting_cookie_url << " exists.";

      // Allow setting insecure cookie from a secure URL even if equivalent
      // secure cookie exists.
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=3;")
                      .IsInclude())
          << "Insecure host cookie from " << https_url
          << " can be set even if equivalent secure host cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), https_url,
                      base::StrCat({"A=4; Domain=", new_cookie_host}))
                      .IsInclude())
          << "Insecure domain cookie from " << https_url
          << " can be set even if equivalent secure host cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "B=3;")
                      .IsInclude())
          << "Insecure host cookie from " << https_url
          << " can be set even if equivalent secure domain cookie from "
          << preexisting_cookie_url << " exists.";
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), https_url,
                      base::StrCat({"B=4; Domain=", new_cookie_host}))
                      .IsInclude())
          << "Insecure domain cookie from " << https_url
          << " can be set even if equivalent secure domain cookie from "
          << preexisting_cookie_url << " exists.";

      DeleteAll(cm.get());
    }
  }

  // Test non-domain-matching case. These sets should all be allowed because the
  // cookie is not equivalent.
  GURL nonmatching_https_url(base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, kAnotherDomain}));

  for (const char* host : {kSuperdomain, kDomain, kSubdomain}) {
    GURL https_url(
        base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator, host}));
    GURL http_url(
        base::StrCat({url::kHttpScheme, url::kStandardSchemeSeparator, host}));

    // Preexisting Secure host and domain cookies.
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), nonmatching_https_url,
                                               "A=0; Secure")
                    .IsInclude());
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), nonmatching_https_url,
                    base::StrCat({"B=0; Secure; Domain=", kAnotherDomain}))
                    .IsInclude());

    // New cookie from insecure URL is set.
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(cm.get(), http_url, "A=1;").IsInclude())
        << "Insecure host cookie from " << http_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), http_url, base::StrCat({"A=2; Domain=", host}))
                    .IsInclude())
        << "Insecure domain cookie from " << http_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(cm.get(), http_url, "B=1;").IsInclude())
        << "Insecure host cookie from " << http_url
        << " can be set even if equivalent secure domain cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), http_url, base::StrCat({"B=2; Domain=", host}))
                    .IsInclude())
        << "Insecure domain cookie from " << http_url
        << " can be set even if equivalent secure domain cookie from "
        << nonmatching_https_url << " exists.";

    // New cookie from secure URL is set.
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=3;").IsInclude())
        << "Insecure host cookie from " << https_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), https_url, base::StrCat({"A=4; Domain=", host}))
                    .IsInclude())
        << "Insecure domain cookie from " << https_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(cm.get(), https_url, "B=3;").IsInclude())
        << "Insecure host cookie from " << https_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), https_url, base::StrCat({"B=4; Domain=", host}))
                    .IsInclude())
        << "Insecure domain cookie from " << https_url
        << " can be set even if equivalent secure host cookie from "
        << nonmatching_https_url << " exists.";

    DeleteAll(cm.get());
  }
}

// Tests the behavior of "Leave Secure Cookies Alone" in
// MaybeDeleteEquivalentCookieAndUpdateStatus().
// Check path-match criterion: If the new cookie is for the same path or a
// subdirectory of the preexisting cookie's path, don't set the new cookie.
TEST_F(CookieMonsterTest, LeaveSecureCookiesAlone_PathMatch) {
  auto cm = std::make_unique<CookieMonster>(nullptr, net::NetLog::Get());

  // A path that is later in this list will path-match all the paths before it.
  const char* kPaths[] = {"/", "/1", "/1/2", "/1/2/3"};
  // This path does not match any, aside from the root path.
  const char* kOtherDirectory = "/9";

  for (int preexisting_cookie_path_index = 0; preexisting_cookie_path_index < 4;
       ++preexisting_cookie_path_index) {
    const char* preexisting_cookie_path = kPaths[preexisting_cookie_path_index];
    GURL preexisting_cookie_url(
        base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                      "a.foo.com", preexisting_cookie_path}));
    for (int new_cookie_path_index = 0; new_cookie_path_index < 4;
         ++new_cookie_path_index) {
      const char* new_cookie_path = kPaths[new_cookie_path_index];
      bool should_path_match =
          new_cookie_path_index >= preexisting_cookie_path_index;
      GURL https_url(
          base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                        "a.foo.com", new_cookie_path}));
      GURL http_url(
          base::StrCat({url::kHttpScheme, url::kStandardSchemeSeparator,
                        "a.foo.com", new_cookie_path}));

      // Preexisting Secure cookie.
      EXPECT_TRUE(
          CreateAndSetCookieReturnStatus(
              cm.get(), preexisting_cookie_url,
              base::StrCat({"A=0; Secure; Path=", preexisting_cookie_path}))
              .IsInclude());

      // Don't set insecure cookie from an insecure URL if equivalent secure
      // cookie exists.
      CookieInclusionStatus set = CreateAndSetCookieReturnStatus(
          cm.get(), http_url, base::StrCat({"A=1; Path=", new_cookie_path}));
      EXPECT_TRUE(should_path_match
                      ? set.HasExactlyExclusionReasonsForTesting(
                            {CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE})
                      : set.IsInclude())
          << "Insecure cookie from " << http_url << " should "
          << (should_path_match ? "not " : "")
          << "be set if equivalent secure cookie from "
          << preexisting_cookie_url << " exists.";

      // Allow setting insecure cookie from a secure URL even if equivalent
      // secure cookie exists.
      EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                      cm.get(), https_url,
                      base::StrCat({"A=2; Path=", new_cookie_path}))
                      .IsInclude())
          << "Insecure cookie from " << http_url
          << " can be set even if equivalent secure cookie from "
          << preexisting_cookie_url << " exists.";

      DeleteAll(cm.get());
    }
  }

  // Test non-matching-path case. These sets should all be allowed because the
  // cookie is not equivalent.
  GURL nonmatching_https_url(
      base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                    "a.foo.com", kOtherDirectory}));

  for (int new_cookie_path_index = 1; new_cookie_path_index < 4;
       ++new_cookie_path_index) {
    const char* new_cookie_path = kPaths[new_cookie_path_index];
    GURL https_url(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, new_cookie_path}));
    GURL http_url(base::StrCat(
        {url::kHttpScheme, url::kStandardSchemeSeparator, new_cookie_path}));

    // Preexisting Secure cookie.
    EXPECT_TRUE(CreateAndSetCookieReturnStatus(
                    cm.get(), nonmatching_https_url,
                    base::StrCat({"A=0; Secure; Path=", kOtherDirectory}))
                    .IsInclude());

    // New cookie from insecure URL is set.
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(
            cm.get(), http_url, base::StrCat({"A=1; Path=", new_cookie_path}))
            .IsInclude())
        << "Insecure cookie from " << http_url
        << " can be set even if equivalent secure cookie from "
        << nonmatching_https_url << " exists.";

    // New cookie from secure URL is set.
    EXPECT_TRUE(
        CreateAndSetCookieReturnStatus(
            cm.get(), https_url, base::StrCat({"A=1; Path=", new_cookie_path}))
            .IsInclude())
        << "Insecure cookie from " << https_url
        << " can be set even if equivalent secure cookie from "
        << nonmatching_https_url << " exists.";
  }
}

// Tests for behavior for strict secure cookies.
TEST_F(CookieMonsterTest, EvictSecureCookies) {
  // Hard-coding limits in the test, but use DCHECK_EQ to enforce constraint.
  DCHECK_EQ(180U, CookieMonster::kDomainMaxCookies);
  DCHECK_EQ(150U, CookieMonster::kDomainMaxCookies -
                      CookieMonster::kDomainPurgeCookies);
  DCHECK_EQ(3300U, CookieMonster::kMaxCookies);
  DCHECK_EQ(30, CookieMonster::kSafeFromGlobalPurgeDays);

  // If secure cookies for one domain hit the per domain limit (180), a
  // non-secure cookie will not evict them (and, in fact, the non-secure cookie
  // will be removed right after creation).
  const CookiesEntry test1[] = {{180U, true}, {1U, false}};
  TestSecureCookieEviction(test1, 150U, 0U, nullptr);

  // If non-secure cookies for one domain hit the per domain limit (180), the
  // creation of secure cookies will evict the non-secure cookies first, making
  // room for the secure cookies.
  const CookiesEntry test2[] = {{180U, false}, {20U, true}};
  TestSecureCookieEviction(test2, 20U, 149U, nullptr);

  // If secure cookies for one domain go past the per domain limit (180), they
  // will be evicted as normal by the per domain purge amount (30) down to a
  // lower amount (150), and then will continue to create the remaining cookies
  // (19 more to 169).
  const CookiesEntry test3[] = {{200U, true}};
  TestSecureCookieEviction(test3, 169U, 0U, nullptr);

  // If a non-secure cookie is created, and a number of secure cookies exceeds
  // the per domain limit (18), the total cookies will be evicted down to a
  // lower amount (150), enforcing the eviction of the non-secure cookie, and
  // the remaining secure cookies will be created (another 19 to 169).
  const CookiesEntry test4[] = {{1U, false}, {199U, true}};
  TestSecureCookieEviction(test4, 169U, 0U, nullptr);

  // If an even number of non-secure and secure cookies are created below the
  // per-domain limit (180), all will be created and none evicted.
  const CookiesEntry test5[] = {{75U, false}, {75U, true}};
  TestSecureCookieEviction(test5, 75U, 75U, nullptr);

  // If the same number of secure and non-secure cookies are created (50 each)
  // below the per domain limit (180), and then another set of secure cookies
  // are created to bring the total above the per-domain limit, all secure
  // cookies will be retained, and the non-secure cookies will be culled down
  // to the limit.
  const CookiesEntry test6[] = {{50U, true}, {50U, false}, {81U, true}};
  TestSecureCookieEviction(test6, 131U, 19U, nullptr);

  // If the same number of non-secure and secure cookies are created (50 each)
  // below the per domain limit (180), and then another set of non-secure
  // cookies are created to bring the total above the per-domain limit, all
  // secure cookies will be retained, and the non-secure cookies will be culled
  // down to the limit.
  const CookiesEntry test7[] = {{50U, false}, {50U, true}, {81U, false}};
  TestSecureCookieEviction(test7, 50U, 100U, nullptr);

  // If the same number of non-secure and secure cookies are created (50 each)
  // below the per domain limit (180), and then another set of non-secure
  // cookies are created to bring the total above the per-domain limit, all
  // secure cookies will be retained, and the non-secure cookies will be culled
  // down to the limit, then the remaining non-secure cookies will be created
  // (9).
  const CookiesEntry test8[] = {{50U, false}, {50U, true}, {90U, false}};
  TestSecureCookieEviction(test8, 50U, 109U, nullptr);

  // If a number of non-secure cookies are created on other hosts (20) and are
  // past the global 'safe' date, and then the number of non-secure cookies for
  // a single domain are brought to the per-domain limit (180), followed by
  // another set of secure cookies on that same domain (20), all the secure
  // cookies for that domain should be retained, while the non-secure should be
  // culled down to the per-domain limit. The non-secure cookies for other
  // domains should remain untouched.
  const CookiesEntry test9[] = {{180U, false}, {20U, true}};
  const AltHosts test9_alt_hosts(0, 20);
  TestSecureCookieEviction(test9, 20U, 169U, &test9_alt_hosts);

  // If a number of secure cookies are created on other hosts and hit the global
  // cookie limit (3300) and are past the global 'safe' date, and then a single
  // non-secure cookie is created now, the secure cookies are removed so that
  // the global total number of cookies is at the global purge goal (3000), but
  // the non-secure cookie is not evicted since it is too young.
  const CookiesEntry test10[] = {{1U, false}};
  const AltHosts test10_alt_hosts(3300, 0);
  TestSecureCookieEviction(test10, 2999U, 1U, &test10_alt_hosts);

  // If a number of non-secure cookies are created on other hosts and hit the
  // global cookie limit (3300) and are past the global 'safe' date, and then a
  // single non-secure cookie is created now, the non-secure cookies are removed
  // so that the global total number of cookies is at the global purge goal
  // (3000).
  const CookiesEntry test11[] = {{1U, false}};
  const AltHosts test11_alt_hosts(0, 3300);
  TestSecureCookieEviction(test11, 0U, 3000U, &test11_alt_hosts);

  // If a number of non-secure cookies are created on other hosts and hit the
  // global cookie limit (3300) and are past the global 'safe' date, and then a
  // single ecure cookie is created now, the non-secure cookies are removed so
  // that the global total number of cookies is at the global purge goal (3000),
  // but the secure cookie is not evicted.
  const CookiesEntry test12[] = {{1U, true}};
  const AltHosts test12_alt_hosts(0, 3300);
  TestSecureCookieEviction(test12, 1U, 2999U, &test12_alt_hosts);

  // If a total number of secure and non-secure cookies are created on other
  // hosts and hit the global cookie limit (3300) and are past the global 'safe'
  // date, and then a single non-secure cookie is created now, the global
  // non-secure cookies are removed so that the global total number of cookies
  // is at the global purge goal (3000), but the secure cookies are not evicted.
  const CookiesEntry test13[] = {{1U, false}};
  const AltHosts test13_alt_hosts(1500, 1800);
  TestSecureCookieEviction(test13, 1500U, 1500, &test13_alt_hosts);

  // If a total number of secure and non-secure cookies are created on other
  // hosts and hit the global cookie limit (3300) and are past the global 'safe'
  // date, and then a single secure cookie is created now, the global non-secure
  // cookies are removed so that the global total number of cookies is at the
  // global purge goal (3000), but the secure cookies are not evicted.
  const CookiesEntry test14[] = {{1U, true}};
  const AltHosts test14_alt_hosts(1500, 1800);
  TestSecureCookieEviction(test14, 1501U, 1499, &test14_alt_hosts);
}

// Tests that strict secure cookies doesn't trip equivalent cookie checks
// accidentally. Regression test for https://crbug.com/569943.
TEST_F(CookieMonsterTest, EquivalentCookies) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  GURL http_url("http://www.foo.com");
  GURL http_superdomain_url("http://foo.com");
  GURL https_url("https://www.foo.com");

  // Tests that non-equivalent cookies because of the path attribute can be set
  // successfully.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url,
                                             "A=C; path=/some/other/path")
                  .IsInclude());
  EXPECT_FALSE(SetCookie(cm.get(), http_url, "A=D; path=/some/other/path"));

  // Tests that non-equivalent cookies because of the domain attribute can be
  // set successfully.
  EXPECT_TRUE(CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=B; Secure")
                  .IsInclude());
  EXPECT_TRUE(
      CreateAndSetCookieReturnStatus(cm.get(), https_url, "A=C; domain=foo.com")
          .IsInclude());
  EXPECT_FALSE(SetCookie(cm.get(), http_url, "A=D; domain=foo.com"));
}

TEST_F(CookieMonsterTest, SetCanonicalCookieDoesNotBlockForLoadAll) {
  scoped_refptr<MockPersistentCookieStore> persistent_store =
      base::MakeRefCounted<MockPersistentCookieStore>();
  // Collect load commands so we have control over their execution.
  persistent_store->set_store_load_commands(true);
  CookieMonster cm(persistent_store.get(), nullptr);

  // Start of a canonical cookie set.
  ResultSavingCookieCallback<CookieAccessResult> callback_set;
  GURL cookie_url("http://a.com/");
  cm.SetCanonicalCookieAsync(
      CanonicalCookie::CreateForTesting(cookie_url, "A=B", base::Time::Now()),
      cookie_url, CookieOptions::MakeAllInclusive(),
      callback_set.MakeCallback());

  // Get cookies for a different URL.
  GetCookieListCallback callback_get;
  cm.GetCookieListWithOptionsAsync(
      GURL("http://b.com/"), CookieOptions::MakeAllInclusive(),
      CookiePartitionKeyCollection(), callback_get.MakeCallback());

  // Now go through the store commands, and execute individual loads.
  const auto& commands = persistent_store->commands();
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].type == CookieStoreCommand::LOAD_COOKIES_FOR_KEY)
      persistent_store->TakeCallbackAt(i).Run(
          std::vector<std::unique_ptr<CanonicalCookie>>());
  }

  // This should be enough for both individual commands.
  callback_set.WaitUntilDone();
  callback_get.WaitUntilDone();

  // Now execute full-store loads as well.
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].type == CookieStoreCommand::LOAD)
      persistent_store->TakeCallbackAt(i).Run(
          std::vector<std::unique_ptr<CanonicalCookie>>());
  }
}

TEST_F(CookieMonsterTest, DeleteDuplicateCTime) {
  const char* const kNames[] = {"A", "B", "C"};

  // Tests that DeleteCanonicalCookie properly distinguishes different cookies
  // (e.g. different name or path) with identical ctime on same domain.
  // This gets tested a few times with different deletion target, to make sure
  // that the implementation doesn't just happen to pick the right one because
  // of implementation details.
  for (const auto* name : kNames) {
    CookieMonster cm(nullptr, nullptr);
    Time now = Time::Now();
    GURL url("http://www.example.com");

    for (size_t i = 0; i < std::size(kNames); ++i) {
      std::string cookie_string =
          base::StrCat({kNames[i], "=", base::NumberToString(i)});
      EXPECT_TRUE(SetCookieWithCreationTime(&cm, url, cookie_string, now));
    }

    // Delete the run'th cookie.
    CookieList all_cookies = GetAllCookiesForURLWithOptions(
        &cm, url, CookieOptions::MakeAllInclusive());
    ASSERT_EQ(all_cookies.size(), std::size(kNames));
    for (size_t i = 0; i < std::size(kNames); ++i) {
      const CanonicalCookie& cookie = all_cookies[i];
      if (cookie.Name() == name) {
        EXPECT_TRUE(DeleteCanonicalCookie(&cm, cookie));
      }
    }

    // Check that the right cookie got removed.
    all_cookies = GetAllCookiesForURLWithOptions(
        &cm, url, CookieOptions::MakeAllInclusive());
    ASSERT_EQ(all_cookies.size(), std::size(kNames) - 1);
    for (size_t i = 0; i < std::size(kNames) - 1; ++i) {
      const CanonicalCookie& cookie = all_cookies[i];
      EXPECT_NE(cookie.Name(), name);
    }
  }
}

TEST_F(CookieMonsterTest, DeleteCookieWithInheritedTimestamps) {
  Time t1 = Time::Now();
  Time t2 = t1 + base::Seconds(1);
  GURL url("http://www.example.com");
  std::string cookie_line = "foo=bar";
  CookieOptions options = CookieOptions::MakeAllInclusive();
  std::optional<base::Time> server_time = std::nullopt;
  std::optional<CookiePartitionKey> partition_key = std::nullopt;
  CookieMonster cm(nullptr, nullptr);

  // Write a cookie created at |t1|.
  auto cookie = CanonicalCookie::CreateForTesting(url, cookie_line, t1,
                                                  server_time, partition_key);
  ResultSavingCookieCallback<CookieAccessResult> set_callback_1;
  cm.SetCanonicalCookieAsync(std::move(cookie), url, options,
                             set_callback_1.MakeCallback());
  set_callback_1.WaitUntilDone();

  // Overwrite the cookie at |t2|.
  cookie = CanonicalCookie::CreateForTesting(url, cookie_line, t2, server_time,
                                             partition_key);
  ResultSavingCookieCallback<CookieAccessResult> set_callback_2;
  cm.SetCanonicalCookieAsync(std::move(cookie), url, options,
                             set_callback_2.MakeCallback());
  set_callback_2.WaitUntilDone();

  // The second cookie overwrites the first one but it will inherit the creation
  // timestamp |t1|. Test that deleting the new cookie still works.
  cookie = CanonicalCookie::CreateForTesting(url, cookie_line, t2, server_time,
                                             partition_key);
  ResultSavingCookieCallback<unsigned int> delete_callback;
  cm.DeleteCanonicalCookieAsync(*cookie, delete_callback.MakeCallback());
  delete_callback.WaitUntilDone();
  EXPECT_EQ(1U, delete_callback.result());
}

TEST_F(CookieMonsterTest, RejectCreatedSameSiteCookieOnSet) {
  GURL url("http://www.example.com");
  std::string cookie_line = "foo=bar; SameSite=Lax";

  CookieMonster cm(nullptr, nullptr);
  CookieOptions env_cross_site;
  env_cross_site.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));

  CookieInclusionStatus status;
  // Cookie can be created successfully; SameSite is not checked on Creation.
  auto cookie =
      CanonicalCookie::CreateForTesting(url, cookie_line, base::Time::Now(),
                                        /*server_time=*/std::nullopt,
                                        /*cookie_partition_key=*/std::nullopt,
                                        CookieSourceType::kUnknown, &status);
  ASSERT_TRUE(cookie != nullptr);
  ASSERT_TRUE(status.IsInclude());

  // ... but the environment is checked on set, so this may be rejected then.
  ResultSavingCookieCallback<CookieAccessResult> callback;
  cm.SetCanonicalCookieAsync(std::move(cookie), url, env_cross_site,
                             callback.MakeCallback());
  callback.WaitUntilDone();
  EXPECT_TRUE(callback.result().status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX}));
}

TEST_F(CookieMonsterTest, RejectCreatedSecureCookieOnSet) {
  GURL http_url("http://www.example.com");
  std::string cookie_line = "foo=bar; Secure";

  CookieMonster cm(nullptr, nullptr);
  CookieInclusionStatus status;
  // Cookie can be created successfully from an any url. Secure is not checked
  // on Create.
  auto cookie = CanonicalCookie::CreateForTesting(
      http_url, cookie_line, base::Time::Now(), /*server_time=*/std::nullopt,
      /*cookie_partition_key=*/std::nullopt, CookieSourceType::kUnknown,
      &status);

  ASSERT_TRUE(cookie != nullptr);
  ASSERT_TRUE(status.IsInclude());

  // Cookie is rejected when attempting to set from a non-secure scheme.
  ResultSavingCookieCallback<CookieAccessResult> callback;
  cm.SetCanonicalCookieAsync(std::move(cookie), http_url,
                             CookieOptions::MakeAllInclusive(),
                             callback.MakeCallback());
  callback.WaitUntilDone();
  EXPECT_TRUE(callback.result().status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
}

TEST_F(CookieMonsterTest, RejectCreatedHttpOnlyCookieOnSet) {
  GURL url("http://www.example.com");
  std::string cookie_line = "foo=bar; HttpOnly";

  CookieMonster cm(nullptr, nullptr);
  CookieInclusionStatus status;
  // Cookie can be created successfully; HttpOnly is not checked on Create.
  auto cookie =
      CanonicalCookie::CreateForTesting(url, cookie_line, base::Time::Now(),
                                        /*server_time=*/std::nullopt,
                                        /*cookie_partition_key=*/std::nullopt,
                                        CookieSourceType::kUnknown, &status);

  ASSERT_TRUE(cookie != nullptr);
  ASSERT_TRUE(status.IsInclude());

  // Cookie is rejected when attempting to set with a CookieOptions that does
  // not allow httponly.
  CookieOptions options_no_httponly;
  options_no_httponly.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext(
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT));
  options_no_httponly.set_exclude_httponly();  // Default, but make it explicit.
  ResultSavingCookieCallback<CookieAccessResult> callback;
  cm.SetCanonicalCookieAsync(std::move(cookie), url, options_no_httponly,
                             callback.MakeCallback());
  callback.WaitUntilDone();
  EXPECT_TRUE(callback.result().status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
}

// Test that SameSite=None requires Secure.
TEST_F(CookieMonsterTest, CookiesWithoutSameSiteMustBeSecure) {
  const base::TimeDelta kLongAge = kLaxAllowUnsafeMaxAge * 4;
  const base::TimeDelta kShortAge = kLaxAllowUnsafeMaxAge / 4;

  struct TestCase {
    bool is_url_secure;
    std::string cookie_line;
    CookieInclusionStatus expected_set_cookie_result;
    // Only makes sense to check if result is INCLUDE:
    CookieEffectiveSameSite expected_effective_samesite =
        CookieEffectiveSameSite::NO_RESTRICTION;
    base::TimeDelta creation_time_delta = base::TimeDelta();
  } test_cases[] = {
      // Feature enabled:
      // Cookie set from a secure URL with SameSite enabled is not rejected.
      {true, "A=B; SameSite=Lax", CookieInclusionStatus(),
       CookieEffectiveSameSite::LAX_MODE},
      // Cookie set from a secure URL which is defaulted into Lax is not
      // rejected.
      {true, "A=B",  // recently-set session cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       kShortAge},
      {true, "A=B",  // not-recently-set session cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE, kLongAge},
      // Cookie set from a secure URL with SameSite=None and Secure is set.
      {true, "A=B; SameSite=None; Secure", CookieInclusionStatus(),
       CookieEffectiveSameSite::NO_RESTRICTION},
      // Cookie set from a secure URL with SameSite=None but not specifying
      // Secure is rejected.
      {true, "A=B; SameSite=None",
       CookieInclusionStatus(
           CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE,
           CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE)},
      // Cookie set from an insecure URL which defaults into LAX_MODE is not
      // rejected.
      {false, "A=B",  // recently-set session cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       kShortAge},
      {false, "A=B",  // not-recently-set session cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE, kLongAge},
      {false, "A=B; Max-Age=1000000",  // recently-set persistent cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
       kShortAge},
      {false,
       "A=B; Max-Age=1000000",  // not-recently-set persistent cookie.
       CookieInclusionStatus(), CookieEffectiveSameSite::LAX_MODE, kLongAge},
  };

  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  GURL secure_url("https://www.example1.test");
  GURL insecure_url("http://www.example2.test");

  int length = sizeof(test_cases) / sizeof(test_cases[0]);
  for (int i = 0; i < length; ++i) {
    TestCase test = test_cases[i];

    GURL url = test.is_url_secure ? secure_url : insecure_url;
    base::Time creation_time = base::Time::Now() - test.creation_time_delta;
    auto cookie =
        CanonicalCookie::CreateForTesting(url, test.cookie_line, creation_time);
    // Make a copy so we can delete it after the test.
    CanonicalCookie cookie_copy = *cookie;
    CookieAccessResult result = SetCanonicalCookieReturnAccessResult(
        cm.get(), std::move(cookie), url,
        true /* can_modify_httponly (irrelevant) */);
    EXPECT_EQ(test.expected_set_cookie_result, result.status)
        << "Test case " << i << " failed.";
    if (result.status.IsInclude()) {
      auto cookies = GetAllCookiesForURL(cm.get(), url);
      ASSERT_EQ(1u, cookies.size());
      EXPECT_EQ(test.expected_effective_samesite, result.effective_same_site)
          << "Test case " << i << " failed.";
      DeleteCanonicalCookie(cm.get(), cookie_copy);
    }
  }
}

class CookieMonsterNotificationTest : public CookieMonsterTest {
 public:
  CookieMonsterNotificationTest()
      : test_url_("http://www.foo.com/foo"),
        store_(base::MakeRefCounted<MockPersistentCookieStore>()),
        monster_(std::make_unique<CookieMonster>(store_.get(), nullptr)) {}

  ~CookieMonsterNotificationTest() override = default;

  CookieMonster* monster() { return monster_.get(); }

 protected:
  const GURL test_url_;

 private:
  scoped_refptr<MockPersistentCookieStore> store_;
  std::unique_ptr<CookieMonster> monster_;
};

void RecordCookieChanges(std::vector<CanonicalCookie>* out_cookies,
                         std::vector<CookieChangeCause>* out_causes,
                         const CookieChangeInfo& change) {
  DCHECK(out_cookies);
  out_cookies->push_back(change.cookie);
  if (out_causes)
    out_causes->push_back(change.cause);
}

// Tests that there are no changes emitted for cookie loading, but there are
// changes emitted for other operations.
TEST_F(CookieMonsterNotificationTest, NoNotificationOnLoad) {
  // Create a persistent store that will not synchronously satisfy the
  // loading requirement.
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  store->set_store_load_commands(true);

  // Bind it to a CookieMonster
  auto monster = std::make_unique<CookieMonster>(store.get(), nullptr);

  // Trigger load dispatch and confirm it.
  monster->GetAllCookiesAsync(CookieStore::GetAllCookiesCallback());
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::LOAD, store->commands()[0].type);

  // Attach a change subscription.
  std::vector<CanonicalCookie> cookies;
  std::vector<CookieChangeCause> causes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      monster->GetChangeDispatcher().AddCallbackForAllChanges(
          base::BindRepeating(&RecordCookieChanges, &cookies, &causes));

  // Set up some initial cookies, including duplicates.
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;
  GURL url("http://www.foo.com");
  initial_cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "X=1; path=/", base::Time::Now()));
  initial_cookies.push_back(
      CanonicalCookie::CreateForTesting(url, "Y=1; path=/", base::Time::Now()));
  initial_cookies.push_back(CanonicalCookie::CreateForTesting(
      url, "Y=2; path=/", base::Time::Now() + base::Days(1)));

  // Execute the load
  store->TakeCallbackAt(0).Run(std::move(initial_cookies));
  base::RunLoop().RunUntilIdle();

  // We should see no insertions (because loads do not cause notifications to be
  // dispatched), no deletions (because overwriting a duplicate cookie on load
  // does not trigger a notification), and two cookies in the monster.
  EXPECT_EQ(0u, cookies.size());
  EXPECT_EQ(0u, causes.size());
  EXPECT_EQ(2u, this->GetAllCookies(monster.get()).size());

  // Change the cookies again to make sure that other changes do emit
  // notifications.
  this->CreateAndSetCookie(monster.get(), url, "X=2; path=/",
                           CookieOptions::MakeAllInclusive());
  this->CreateAndSetCookie(monster.get(), url, "Y=3; path=/; max-age=0",
                           CookieOptions::MakeAllInclusive());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, cookies.size());
  ASSERT_EQ(3u, causes.size());
  EXPECT_EQ("X", cookies[0].Name());
  EXPECT_EQ("1", cookies[0].Value());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, causes[0]);
  EXPECT_EQ("X", cookies[1].Name());
  EXPECT_EQ("2", cookies[1].Value());
  EXPECT_EQ(CookieChangeCause::INSERTED, causes[1]);
  EXPECT_EQ("Y", cookies[2].Name());
  EXPECT_EQ("2", cookies[2].Value());
  EXPECT_EQ(CookieChangeCause::EXPIRED_OVERWRITE, causes[2]);
}

class CookieMonsterLegacyCookieAccessTest : public CookieMonsterTest {
 public:
  CookieMonsterLegacyCookieAccessTest()
      : cm_(std::make_unique<CookieMonster>(nullptr /* store */,
                                            nullptr /* netlog */
                                            )) {
    // Need to reset first because there cannot be two TaskEnvironments at the
    // same time.
    task_environment_.reset();
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME);

    std::unique_ptr<TestCookieAccessDelegate> access_delegate =
        std::make_unique<TestCookieAccessDelegate>();
    access_delegate_ = access_delegate.get();
    cm_->SetCookieAccessDelegate(std::move(access_delegate));
  }

  ~CookieMonsterLegacyCookieAccessTest() override = default;

 protected:
  const std::string kDomain = "example.test";
  const GURL kHttpsUrl = GURL("https://example.test");
  const GURL kHttpUrl = GURL("http://example.test");
  std::unique_ptr<CookieMonster> cm_;
  raw_ptr<TestCookieAccessDelegate> access_delegate_;
};

TEST_F(CookieMonsterLegacyCookieAccessTest, SetLegacyNoSameSiteCookie) {
  // Check that setting unspecified-SameSite cookie from cross-site context
  // fails if not set to Legacy semantics, but succeeds if set to legacy.
  EXPECT_FALSE(CreateAndSetCookie(cm_.get(), kHttpUrl, "cookie=chocolate_chip",
                                  CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::UNKNOWN);
  EXPECT_FALSE(CreateAndSetCookie(cm_.get(), kHttpUrl, "cookie=chocolate_chip",
                                  CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::NONLEGACY);
  EXPECT_FALSE(CreateAndSetCookie(cm_.get(), kHttpUrl, "cookie=chocolate_chip",
                                  CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::LEGACY);
  EXPECT_TRUE(CreateAndSetCookie(cm_.get(), kHttpUrl, "cookie=chocolate_chip",
                                 CookieOptions()));
}

TEST_F(CookieMonsterLegacyCookieAccessTest, GetLegacyNoSameSiteCookie) {
  // Set a cookie with no SameSite attribute.
  ASSERT_TRUE(CreateAndSetCookie(cm_.get(), kHttpUrl, "cookie=chocolate_chip",
                                 CookieOptions::MakeAllInclusive()));

  // Getting the cookie fails unless semantics is legacy.
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::UNKNOWN);
  EXPECT_EQ("", GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::NONLEGACY);
  EXPECT_EQ("", GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::LEGACY);
  EXPECT_EQ("cookie=chocolate_chip",
            GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
}

TEST_F(CookieMonsterLegacyCookieAccessTest,
       SetLegacySameSiteNoneInsecureCookie) {
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::UNKNOWN);
  EXPECT_FALSE(CreateAndSetCookie(cm_.get(), kHttpsUrl,
                                  "cookie=oatmeal_raisin; SameSite=None",
                                  CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::NONLEGACY);
  EXPECT_FALSE(CreateAndSetCookie(cm_.get(), kHttpsUrl,
                                  "cookie=oatmeal_raisin; SameSite=None",
                                  CookieOptions()));
  // Setting the access semantics to legacy allows setting the cookie.
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::LEGACY);
  EXPECT_TRUE(CreateAndSetCookie(cm_.get(), kHttpsUrl,
                                 "cookie=oatmeal_raisin; SameSite=None",
                                 CookieOptions()));
  EXPECT_EQ("cookie=oatmeal_raisin",
            GetCookiesWithOptions(cm_.get(), kHttpsUrl, CookieOptions()));
}

TEST_F(CookieMonsterLegacyCookieAccessTest,
       GetLegacySameSiteNoneInsecureCookie) {
  // Need to inject such a cookie under legacy semantics.
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::LEGACY);
  ASSERT_TRUE(CreateAndSetCookie(cm_.get(), kHttpUrl,
                                 "cookie=oatmeal_raisin; SameSite=None",
                                 CookieOptions::MakeAllInclusive()));
  // Getting a SameSite=None but non-Secure cookie fails unless semantics is
  // legacy.
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::UNKNOWN);
  EXPECT_EQ("", GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::NONLEGACY);
  EXPECT_EQ("", GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
  access_delegate_->SetExpectationForCookieDomain(
      kDomain, CookieAccessSemantics::LEGACY);
  EXPECT_EQ("cookie=oatmeal_raisin",
            GetCookiesWithOptions(cm_.get(), kHttpUrl, CookieOptions()));
}

TEST_F(CookieMonsterTest, IsCookieSentToSamePortThatSetIt) {
  // Note: `IsCookieSentToSamePortThatSetIt()` only uses the source_scheme if
  // the port is valid, specified, and doesn't match the url's port. So for test
  // cases where the above aren't true the value of source_scheme is irreleant.

  // Test unspecified.
  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("https://foo.com"), url::PORT_UNSPECIFIED,
                CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kSourcePortUnspecified);

  // Test invalid.
  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("https://foo.com"), url::PORT_INVALID,
                CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kInvalid);

  // Test same.
  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("https://foo.com"), 443, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kYes);

  ASSERT_EQ(
      CookieMonster::IsCookieSentToSamePortThatSetIt(
          GURL("https://foo.com:1234"), 1234, CookieSourceScheme::kSecure),
      CookieMonster::CookieSentToSamePort::kYes);

  // Test different but default.
  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("https://foo.com"), 80, CookieSourceScheme::kNonSecure),
            CookieMonster::CookieSentToSamePort::kNoButDefault);

  ASSERT_EQ(
      CookieMonster::IsCookieSentToSamePortThatSetIt(
          GURL("https://foo.com:443"), 80, CookieSourceScheme::kNonSecure),
      CookieMonster::CookieSentToSamePort::kNoButDefault);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("wss://foo.com"), 80, CookieSourceScheme::kNonSecure),
            CookieMonster::CookieSentToSamePort::kNoButDefault);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("http://foo.com"), 443, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNoButDefault);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("ws://foo.com"), 443, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNoButDefault);

  // Test different.
  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("http://foo.com:9000"), 85, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNo);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("https://foo.com"), 80, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNo);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("wss://foo.com"), 80, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNo);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("http://foo.com"), 443, CookieSourceScheme::kNonSecure),
            CookieMonster::CookieSentToSamePort::kNo);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("ws://foo.com"), 443, CookieSourceScheme::kNonSecure),
            CookieMonster::CookieSentToSamePort::kNo);

  ASSERT_EQ(CookieMonster::IsCookieSentToSamePortThatSetIt(
                GURL("http://foo.com:444"), 443, CookieSourceScheme::kSecure),
            CookieMonster::CookieSentToSamePort::kNo);
}

TEST_F(CookieMonsterTest, CookieDomainSetHistogram) {
  base::HistogramTester histograms;
  const char kHistogramName[] = "Cookie.DomainSet";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  histograms.ExpectTotalCount(kHistogramName, 0);

  // Set a host only cookie (non-Domain).
  EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName, false, 1);

  // Set a domain cookie.
  EXPECT_TRUE(SetCookie(cm.get(), https_www_foo_.url(),
                        "A=B; Domain=" + https_www_foo_.host()));
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName, true, 1);

  // Invalid cookies don't count toward the histogram.
  EXPECT_FALSE(
      SetCookie(cm.get(), https_www_foo_.url(), "A=B; Domain=other.com"));
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName, false, 1);
}

TEST_F(CookieMonsterTest, CookiePortReadHistogram) {
  base::HistogramTester histograms;
  const char kHistogramName[] = "Cookie.Port.Read.RemoteHost";
  const char kHistogramNameLocal[] = "Cookie.Port.Read.Localhost";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  histograms.ExpectTotalCount(kHistogramName, 0);

  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com"), "A=B"));

  // May as well check that it didn't change the histogram...
  histograms.ExpectTotalCount(kHistogramName, 0);

  // Now read it from some different ports. This requires some knowledge of how
  // `ReducePortRangeForCookieHistogram` maps ports, but that's probably fine.
  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com")), "A=B");
  // https default is 443, so check that.
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(443), 1);

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com:82")), "A=B");
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(82), 1);

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com:8080")), "A=B");
  histograms.ExpectTotalCount(kHistogramName, 3);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(8080), 1);

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com:1234")), "A=B");
  histograms.ExpectTotalCount(kHistogramName, 4);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(1234), 1);

  // Histogram should not increment if nothing is read.
  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.other.com")), "");
  histograms.ExpectTotalCount(kHistogramName, 4);

  // Make sure the correct histogram is chosen for localhost.
  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://localhost"), "local=host"));

  histograms.ExpectTotalCount(kHistogramNameLocal, 0);

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://localhost:82")), "local=host");
  histograms.ExpectTotalCount(kHistogramNameLocal, 1);
  histograms.ExpectBucketCount(kHistogramNameLocal,
                               ReducePortRangeForCookieHistogram(82), 1);
}

TEST_F(CookieMonsterTest, CookiePortSetHistogram) {
  base::HistogramTester histograms;
  const char kHistogramName[] = "Cookie.Port.Set.RemoteHost";
  const char kHistogramNameLocal[] = "Cookie.Port.Set.Localhost";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  histograms.ExpectTotalCount(kHistogramName, 0);

  // Set some cookies. This requires some knowledge of how
  // ReducePortRangeForCookieHistogram maps ports, but that's probably fine.

  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com"), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(443), 1);

  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com:80"), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(80), 1);

  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com:9000"), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, 3);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(9000), 1);

  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com:1234"), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, 4);
  histograms.ExpectBucketCount(kHistogramName,
                               ReducePortRangeForCookieHistogram(1234), 1);

  // Histogram should not increment for invalid cookie.
  EXPECT_FALSE(SetCookie(cm.get(), GURL("https://www.foo.com"),
                         "A=B; Domain=malformedcookie.com"));
  histograms.ExpectTotalCount(kHistogramName, 4);

  // Nor should it increment for a read operation
  EXPECT_NE(GetCookies(cm.get(), GURL("https://www.foo.com")), "");
  histograms.ExpectTotalCount(kHistogramName, 4);

  // Make sure the correct histogram is chosen for localhost.
  histograms.ExpectTotalCount(kHistogramNameLocal, 0);

  EXPECT_TRUE(
      SetCookie(cm.get(), GURL("https://localhost:1234"), "local=host"));
  histograms.ExpectTotalCount(kHistogramNameLocal, 1);
  histograms.ExpectBucketCount(kHistogramNameLocal,
                               ReducePortRangeForCookieHistogram(1234), 1);
}

TEST_F(CookieMonsterTest, CookiePortReadDiffersFromSetHistogram) {
  base::HistogramTester histograms;
  const char kHistogramName[] = "Cookie.Port.ReadDiffersFromSet.RemoteHost";
  const char kHistogramNameLocal[] = "Cookie.Port.ReadDiffersFromSet.Localhost";
  const char kHistogramNameDomainSet[] =
      "Cookie.Port.ReadDiffersFromSet.DomainSet";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  histograms.ExpectTotalCount(kHistogramName, 0);

  // Set some cookies. One with a port, one without, and one with an invalid
  // port.
  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com/withport"),
                        "A=B; Path=/withport"));  // Port 443

  auto unspecified_cookie = CanonicalCookie::CreateForTesting(
      GURL("https://www.foo.com/withoutport"), "C=D; Path=/withoutport",
      base::Time::Now());
  // Force to be unspecified.
  unspecified_cookie->SetSourcePort(url::PORT_UNSPECIFIED);
  EXPECT_TRUE(SetCanonicalCookieReturnAccessResult(
                  cm.get(), std::move(unspecified_cookie),
                  GURL("https://www.foo.com/withoutport"),
                  false /*can_modify_httponly*/)
                  .status.IsInclude());

  auto invalid_cookie = CanonicalCookie::CreateForTesting(
      GURL("https://www.foo.com/invalidport"), "E=F; Path=/invalidport",
      base::Time::Now());
  // Force to be invalid.
  invalid_cookie->SetSourcePort(99999);
  EXPECT_TRUE(SetCanonicalCookieReturnAccessResult(
                  cm.get(), std::move(invalid_cookie),
                  GURL("https://www.foo.com/invalidport"),
                  false /*can_modify_httponly*/)
                  .status.IsInclude());

  // Try same port.
  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com/withport")), "A=B");
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName,
                               CookieMonster::CookieSentToSamePort::kYes, 1);

  // Try different port.
  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com:8080/withport")),
            "A=B");
  histograms.ExpectTotalCount(kHistogramName, 2);
  histograms.ExpectBucketCount(kHistogramName,
                               CookieMonster::CookieSentToSamePort::kNo, 1);

  // Try different port, but it's the default for a different scheme.
  EXPECT_EQ(GetCookies(cm.get(), GURL("http://www.foo.com/withport")), "A=B");
  histograms.ExpectTotalCount(kHistogramName, 3);
  histograms.ExpectBucketCount(
      kHistogramName, CookieMonster::CookieSentToSamePort::kNoButDefault, 1);

  // Now try it with an unspecified port cookie.
  EXPECT_EQ(GetCookies(cm.get(), GURL("http://www.foo.com/withoutport")),
            "C=D");
  histograms.ExpectTotalCount(kHistogramName, 4);
  histograms.ExpectBucketCount(
      kHistogramName,
      CookieMonster::CookieSentToSamePort::kSourcePortUnspecified, 1);

  // Finally try it with an invalid port cookie.
  EXPECT_EQ(GetCookies(cm.get(), GURL("http://www.foo.com/invalidport")),
            "E=F");
  histograms.ExpectTotalCount(kHistogramName, 5);
  histograms.ExpectBucketCount(
      kHistogramName, CookieMonster::CookieSentToSamePort::kInvalid, 1);

  // Make sure the correct histogram is chosen for localhost.
  histograms.ExpectTotalCount(kHistogramNameLocal, 0);
  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://localhost"), "local=host"));

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://localhost")), "local=host");
  histograms.ExpectTotalCount(kHistogramNameLocal, 1);
  histograms.ExpectBucketCount(kHistogramNameLocal,
                               CookieMonster::CookieSentToSamePort::kYes, 1);

  // Make sure the Domain set version works.
  EXPECT_TRUE(SetCookie(cm.get(), GURL("https://www.foo.com/withDomain"),
                        "W=D; Domain=foo.com; Path=/withDomain"));

  histograms.ExpectTotalCount(kHistogramNameDomainSet, 0);

  EXPECT_EQ(GetCookies(cm.get(), GURL("https://www.foo.com/withDomain")),
            "W=D");
  histograms.ExpectTotalCount(kHistogramNameDomainSet, 1);
  histograms.ExpectBucketCount(kHistogramNameDomainSet,
                               CookieMonster::CookieSentToSamePort::kYes, 1);
  // The RemoteHost histogram should also increase with this cookie. Domain
  // cookies aren't special insofar as this metric is concerned.
  histograms.ExpectTotalCount(kHistogramName, 6);
  histograms.ExpectBucketCount(kHistogramName,
                               CookieMonster::CookieSentToSamePort::kYes, 2);
}

TEST_F(CookieMonsterTest, CookieSourceSchemeNameHistogram) {
  base::HistogramTester histograms;
  const char kHistogramName[] = "Cookie.CookieSourceSchemeName";

  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());

  histograms.ExpectTotalCount(kHistogramName, 0);

  struct TestCase {
    CookieSourceSchemeName enum_value;
    std::string scheme;
  };

  // Test the usual and a smattering of some other types including a kOther.
  // It doesn't matter if we add this to the scheme registry or not because we
  // don't actually need the whole url to parse, we just need GURL to pick up on
  // the scheme correctly (which it does). What the rest of the cookie code does
  // with the oddly formed GURL is out of scope of this test (i.e. we don't
  // care).
  const TestCase kTestCases[] = {
      {CookieSourceSchemeName::kHttpsScheme, url::kHttpsScheme},
      {CookieSourceSchemeName::kHttpScheme, url::kHttpScheme},
      {CookieSourceSchemeName::kWssScheme, url::kWssScheme},
      {CookieSourceSchemeName::kWsScheme, url::kWsScheme},
      {CookieSourceSchemeName::kChromeExtensionScheme, "chrome-extension"},
      {CookieSourceSchemeName::kFileScheme, url::kFileScheme},
      {CookieSourceSchemeName::kOther, "abcd1234"}};

  // Make sure all the schemes are considered cookieable.
  std::vector<std::string> schemes;
  for (auto test_case : kTestCases) {
    schemes.push_back(test_case.scheme);
  }
  ResultSavingCookieCallback<bool> cookie_scheme_callback;
  cm->SetCookieableSchemes(schemes, cookie_scheme_callback.MakeCallback());
  cookie_scheme_callback.WaitUntilDone();
  ASSERT_TRUE(cookie_scheme_callback.result());

  const char kUrl[] = "://www.foo.com";
  int count = 0;

  // Test all the cases.
  for (auto test_case : kTestCases) {
    histograms.ExpectBucketCount(kHistogramName, test_case.enum_value, 0);

    EXPECT_TRUE(SetCookie(cm.get(), GURL(test_case.scheme + kUrl), "A=B"));

    histograms.ExpectBucketCount(kHistogramName, test_case.enum_value, 1);
    histograms.ExpectTotalCount(kHistogramName, ++count);
  }

  // This metric is only for cookies that are actually set. Make sure the
  // histogram doesn't increment for cookies that fail to set.

  // Try to set an invalid cookie, for instance: a non-cookieable scheme will be
  // rejected.
  EXPECT_FALSE(SetCookie(cm.get(), GURL("invalidscheme://foo.com"), "A=B"));
  histograms.ExpectTotalCount(kHistogramName, count);
}

class FirstPartySetEnabledCookieMonsterTest : public CookieMonsterTest {
 public:
  FirstPartySetEnabledCookieMonsterTest()
      : cm_(nullptr /* store */, nullptr /* netlog */
        ) {
    std::unique_ptr<TestCookieAccessDelegate> access_delegate =
        std::make_unique<TestCookieAccessDelegate>();
    access_delegate_ = access_delegate.get();
    cm_.SetCookieAccessDelegate(std::move(access_delegate));
  }

  ~FirstPartySetEnabledCookieMonsterTest() override = default;

  CookieMonster* cm() { return &cm_; }

 protected:
  CookieMonster cm_;
  raw_ptr<TestCookieAccessDelegate> access_delegate_;
};

TEST_F(FirstPartySetEnabledCookieMonsterTest, RecordsPeriodicFPSSizes) {
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));
  net::SchemefulSite member4(GURL("https://member4.test"));

  access_delegate_->SetFirstPartySets({
      {owner1,
       net::FirstPartySetEntry(owner1, net::SiteType::kPrimary, std::nullopt)},
      {member1, net::FirstPartySetEntry(owner1, net::SiteType::kAssociated, 0)},
      {member2, net::FirstPartySetEntry(owner1, net::SiteType::kAssociated, 1)},
      {owner2,
       net::FirstPartySetEntry(owner2, net::SiteType::kPrimary, std::nullopt)},
      {member3, net::FirstPartySetEntry(owner2, net::SiteType::kAssociated, 0)},
      {member4, net::FirstPartySetEntry(owner2, net::SiteType::kAssociated, 1)},
  });

  ASSERT_TRUE(SetCookie(cm(), GURL("https://owner1.test"), kValidCookieLine));
  ASSERT_TRUE(SetCookie(cm(), GURL("https://subdomain.member1.test"),
                        kValidCookieLine));
  ASSERT_TRUE(SetCookie(cm(), GURL("https://member2.test"), kValidCookieLine));
  ASSERT_TRUE(
      SetCookie(cm(), GURL("https://subdomain.owner2.test"), kValidCookieLine));
  ASSERT_TRUE(SetCookie(cm(), GURL("https://member3.test"), kValidCookieLine));
  // No cookie set for member4.test.
  ASSERT_TRUE(
      SetCookie(cm(), GURL("https://unrelated1.test"), kValidCookieLine));
  ASSERT_TRUE(
      SetCookie(cm(), GURL("https://unrelated2.test"), kValidCookieLine));
  ASSERT_TRUE(
      SetCookie(cm(), GURL("https://unrelated3.test"), kValidCookieLine));

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(cm()->DoRecordPeriodicStatsForTesting());
  EXPECT_THAT(histogram_tester.GetAllSamples("Cookie.PerFirstPartySetCount"),
              testing::ElementsAre(  //
                                     // owner2.test & member3.test
                  base::Bucket(2 /* min */, 1 /* samples */),
                  // owner1.test, member1.test, & member2.test
                  base::Bucket(3 /* min */, 1 /* samples */)));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURLNonce) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  CookieOptions options = CookieOptions::MakeAllInclusive();

  auto anonymous_iframe_key = CookiePartitionKey::FromURLForTesting(
      GURL("https://anonymous-iframe.test"),
      CookiePartitionKey::AncestorChainBit::kCrossSite,
      base::UnguessableToken::Create());

  // Define cookies from outside an anonymous iframe:
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), https_www_foo_.url(),
                                 "A=0; Secure; HttpOnly; Path=/;", options));
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), https_www_foo_.url(),
                                 "__Host-B=0; Secure; HttpOnly; Path=/;",
                                 options));

  // Define cookies from inside an anonymous iframe:
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_foo_.url(),
      "__Host-B=1; Secure; HttpOnly; Path=/; Partitioned", options,
      std::nullopt, std::nullopt, anonymous_iframe_key));
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), https_www_foo_.url(),
      "__Host-C=0; Secure; HttpOnly; Path=/; Partitioned", options,
      std::nullopt, std::nullopt, anonymous_iframe_key));

  // Check cookies from outside the anonymous iframe:
  EXPECT_THAT(GetAllCookiesForURL(cm.get(), https_www_foo_.url()),
              ElementsAre(MatchesCookieNameValue("A", "0"),
                          MatchesCookieNameValue("__Host-B", "0")));

  // Check cookies from inside the anonymous iframe:
  EXPECT_THAT(
      GetAllCookiesForURL(cm.get(), https_www_foo_.url(),
                          CookiePartitionKeyCollection(anonymous_iframe_key)),
      ElementsAre(MatchesCookieNameValue("__Host-B", "1"),
                  MatchesCookieNameValue("__Host-C", "0")));
}

TEST_F(CookieMonsterTest, SiteHasCookieInOtherPartition) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  CookieOptions options = CookieOptions::MakeAllInclusive();

  GURL url("https://subdomain.example.com/");
  net::SchemefulSite site(url);
  auto partition_key =
      CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"));

  // At first it should return nullopt...
  EXPECT_FALSE(cm->SiteHasCookieInOtherPartition(site, partition_key));

  // ...until we load cookies for that domain.
  GetAllCookiesForURL(cm.get(), url,
                      CookiePartitionKeyCollection::ContainsAll());
  EXPECT_THAT(cm->SiteHasCookieInOtherPartition(site, partition_key),
              testing::Optional(false));

  // Set partitioned cookie.
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), url, "foo=bar; Secure; SameSite=None; Partitioned", options,
      std::nullopt, std::nullopt, partition_key));

  // Should return false with that cookie's partition key.
  EXPECT_THAT(cm->SiteHasCookieInOtherPartition(site, partition_key),
              testing::Optional(false));

  auto other_partition_key = CookiePartitionKey::FromURLForTesting(
      GURL("https://nottoplevelsite.com"));

  // Should return true with another partition key.
  EXPECT_THAT(cm->SiteHasCookieInOtherPartition(site, other_partition_key),
              testing::Optional(true));

  // Set a nonced partitioned cookie with a different partition key.
  EXPECT_TRUE(CreateAndSetCookie(
      cm.get(), url, "foo=bar; Secure; SameSite=None; Partitioned", options,
      std::nullopt, std::nullopt,
      CookiePartitionKey::FromURLForTesting(
          GURL("https://nottoplevelsite.com"),
          CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create())));

  // Should still return false with the original partition key.
  EXPECT_THAT(cm->SiteHasCookieInOtherPartition(site, partition_key),
              testing::Optional(false));

  // Set unpartitioned cookie.
  EXPECT_TRUE(CreateAndSetCookie(cm.get(), url,
                                 "bar=baz; Secure; SameSite=None;", options,
                                 std::nullopt, std::nullopt));

  // Should still return false with the original cookie's partition key. This
  // method only considers partitioned cookies.
  EXPECT_THAT(cm->SiteHasCookieInOtherPartition(site, partition_key),
              testing::Optional(false));

  // Should return nullopt when the partition key is nullopt.
  EXPECT_FALSE(
      cm->SiteHasCookieInOtherPartition(site, /*partition_key=*/std::nullopt));
}

// Test that domain cookies which shadow origin cookies are excluded when scheme
// binding is enabled.
TEST_F(CookieMonsterTest, FilterCookiesWithOptionsExcludeShadowingDomains) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options = CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();

  auto CookieListsMatch = [](const CookieAccessResultList& actual,
                             const CookieList& expected) {
    if (actual.size() != expected.size()) {
      return false;
    }

    for (size_t i = 0; i < actual.size(); i++) {
      if (!actual[i].cookie.IsEquivalent(expected[i])) {
        return false;
      }
    }

    return true;
  };

  // We only exclude shadowing domain cookies when scheme binding is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {net::features::kEnableSchemeBoundCookies},
      {net::features::kEnablePortBoundCookies});

  std::vector<CanonicalCookie*> cookie_ptrs;
  CookieAccessResultList included;
  CookieAccessResultList excluded;

  auto reset = [&cookie_ptrs, &included, &excluded]() {
    cookie_ptrs.clear();
    included.clear();
    excluded.clear();
  };

  auto origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=origin", creation_time, server_time);
  auto origin_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo2=origin", creation_time, server_time);

  auto domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Shadowing domain cookie after the origin cookie.
  cookie_ptrs = {origin_cookie1.get(), origin_cookie2.get(),
                 domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*origin_cookie1, *origin_cookie2}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*domain_cookie1}));
  reset();

  // Shadowing domain cookie before the origin cookie.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*origin_cookie2, *origin_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*domain_cookie1}));
  reset();

  auto domain_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo2=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Multiple different shadowing domain cookies.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), domain_cookie2.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*origin_cookie2, *origin_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*domain_cookie1, *domain_cookie2}));
  reset();

  auto domain_cookie3 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo3=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Non-shadowing domain cookie should be included.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), domain_cookie2.get(),
                 domain_cookie3.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(
      included, {*origin_cookie2, *origin_cookie1, *domain_cookie3}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*domain_cookie1, *domain_cookie2}));
  reset();

  auto sub_domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=subdomain; Domain=" + https_www_foo_.host(),
      creation_time, server_time);

  // If there are multiple domain cookies that shadow the same cookie, they
  // should all be excluded.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), sub_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*origin_cookie2, *origin_cookie1}));
  EXPECT_TRUE(
      CookieListsMatch(excluded, {*domain_cookie1, *sub_domain_cookie1}));
  reset();

  // Domain cookies may shadow each other.
  cookie_ptrs = {domain_cookie1.get(), sub_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(
      CookieListsMatch(included, {*domain_cookie1, *sub_domain_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {}));
  reset();

  auto path_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=pathorigin; Path=/bar", creation_time,
      server_time);

  // Origin cookies on different paths may not be shadowed, even if the
  // origin cookie wouldn't be included on this request.
  cookie_ptrs = {path_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {}));
  EXPECT_TRUE(
      CookieListsMatch(excluded, {*path_origin_cookie1, *domain_cookie1}));
  reset();

  auto insecure_origin_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), "foo1=insecureorigin", creation_time, server_time);
  EXPECT_EQ(insecure_origin_cookie1->SourceScheme(),
            CookieSourceScheme::kNonSecure);

  // Origin cookies that are excluded due to scheme binding don't affect domain
  // cookies.
  cookie_ptrs = {insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*domain_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*insecure_origin_cookie1}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH}));
  reset();

  auto insecure_domain_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(),
      "foo1=insecuredomain; Domain=" + http_www_foo_.domain(), creation_time,
      server_time);

  // Domain cookies that are excluded due to scheme binding shouldn't also be
  // exclude because of shadowing.
  cookie_ptrs = {origin_cookie1.get(), insecure_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*origin_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*insecure_domain_cookie1}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH}));
  reset();

  // If both domain and origin cookie are excluded due to scheme binding then
  // domain cookie shouldn't get shadowing exclusion.
  cookie_ptrs = {insecure_origin_cookie1.get(), insecure_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {}));
  EXPECT_TRUE(CookieListsMatch(
      excluded, {*insecure_origin_cookie1, *insecure_domain_cookie1}));
  EXPECT_TRUE(
      excluded[1].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH}));
  reset();

  cm->SetCookieAccessDelegate(std::make_unique<TestCookieAccessDelegate>());

  CookieURLHelper http_www_trustworthy =
      CookieURLHelper("http://www.trustworthysitefortestdelegate.example");
  CookieURLHelper https_www_trustworthy =
      CookieURLHelper("https://www.trustworthysitefortestdelegate.example");

  auto trust_origin_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(), "foo1=trustorigin", creation_time,
      server_time);

  auto secure_trust_domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(),
      "foo1=securetrustdomain; Domain=" + https_www_trustworthy.domain(),
      creation_time, server_time);
  auto secure_trust_domain_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(),
      "foo2=securetrustdomain; Domain=" + https_www_trustworthy.domain(),
      creation_time, server_time);

  // Securely set domain cookies are excluded when shadowing trustworthy-ly set
  // origin cookies.
  cookie_ptrs = {trust_origin_cookie1.get(), secure_trust_domain_cookie1.get(),
                 secure_trust_domain_cookie2.get()};
  cm->FilterCookiesWithOptions(http_www_trustworthy.url(), options,
                               &cookie_ptrs, &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(
      included, {*trust_origin_cookie1, *secure_trust_domain_cookie2}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*secure_trust_domain_cookie1}));
  reset();

  auto trust_domain_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(),
      "foo1=trustdomain; Domain=" + http_www_trustworthy.domain(),
      creation_time, server_time);
  auto trust_domain_cookie2 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(),
      "foo2=trustdomain; Domain=" + http_www_trustworthy.domain(),
      creation_time, server_time);
  auto secure_trust_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(), "foo1=securetrustorigin", creation_time,
      server_time);

  // Trustworthy-ly set domain cookies are excluded when shadowing securely set
  // origin cookies.
  cookie_ptrs = {secure_trust_origin_cookie1.get(), trust_domain_cookie1.get(),
                 trust_domain_cookie2.get()};
  cm->FilterCookiesWithOptions(http_www_trustworthy.url(), options,
                               &cookie_ptrs, &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(
      included, {*secure_trust_origin_cookie1, *trust_domain_cookie2}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*trust_domain_cookie1}));
  reset();

  auto port_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=differentportorigin", creation_time,
      server_time);
  port_origin_cookie1->SetSourcePort(123);

  // Origin cookies that have warnings due to port binding don't affect domain
  // cookies.
  cookie_ptrs = {port_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(
      CookieListsMatch(included, {*port_origin_cookie1, *domain_cookie1}));
  EXPECT_TRUE(included[0].access_result.status.HasWarningReason(
      CookieInclusionStatus::WARN_PORT_MISMATCH));
  reset();

  auto port_insecure_origin_cookie1 =
      std::make_unique<CanonicalCookie>(*insecure_origin_cookie1);
  port_insecure_origin_cookie1->SetSourcePort(123);

  // Origin cookies that have excluded due to scheme binding and have a port
  // binding warning don't affect domain cookies.
  cookie_ptrs = {port_insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*domain_cookie1}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyWarningReasonsForTesting(
          {CookieInclusionStatus::WARN_PORT_MISMATCH}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH}));
  reset();

  // Enable port binding to test with port exclusions.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {net::features::kEnableSchemeBoundCookies,
       net::features::kEnablePortBoundCookies},
      {});

  // Origin cookies that are excluded due to port binding don't affect domain
  // cookies.
  cookie_ptrs = {port_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*domain_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*port_origin_cookie1}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_PORT_MISMATCH}));
  reset();

  // Origin cookies that are excluded due to scheme and port binding don't
  // affect domain cookies.
  cookie_ptrs = {port_insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {*domain_cookie1}));
  EXPECT_TRUE(CookieListsMatch(excluded, {*port_insecure_origin_cookie1}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_SCHEME_MISMATCH,
           CookieInclusionStatus::EXCLUDE_PORT_MISMATCH}));
  reset();
}

// Test that domain cookies which shadow origin cookies have warnings when
// scheme binding is disabled.
TEST_F(CookieMonsterTest, FilterCookiesWithOptionsWarnShadowingDomains) {
  auto store = base::MakeRefCounted<MockPersistentCookieStore>();
  auto cm = std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  CookieOptions options = CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();

  auto CookieListsMatch = [](const CookieAccessResultList& actual,
                             const std::vector<CanonicalCookie*>& expected) {
    if (actual.size() != expected.size()) {
      return false;
    }

    for (size_t i = 0; i < actual.size(); i++) {
      if (!actual[i].cookie.IsEquivalent(*expected[i])) {
        return false;
      }
    }

    return true;
  };

  // Confirms that of all the cookies in `actual` only the ones also in
  // `expected` have WARN_SHADOWING_DOMAIN.
  auto DomainCookiesHaveWarnings =
      [](const CookieAccessResultList& actual,
         const std::vector<CanonicalCookie>& expected) {
        std::map<CanonicalCookie, CookieInclusionStatus> cookie_result_map;
        for (const auto& cookie_result : actual) {
          cookie_result_map.insert(
              {cookie_result.cookie, cookie_result.access_result.status});
        }

        for (const auto& cookie : expected) {
          // This is a touch hacky but will always work because if the
          // cookie_result_map doesn't contain `cookie` it'll create a default
          // entry with an empty status which will always fail the check. I.e.:
          // return false.
          if (!cookie_result_map[cookie].HasWarningReason(
                  CookieInclusionStatus::WARN_SHADOWING_DOMAIN)) {
            return false;
          }

          // Remove cookies that were part of `expected`.
          cookie_result_map.erase(cookie);
        }

        // If any of the remaining cookies have the warning, return false.
        for (const auto& item : cookie_result_map) {
          if (item.second.HasWarningReason(
                  CookieInclusionStatus::WARN_SHADOWING_DOMAIN)) {
            return false;
          }
        }

        return true;
      };

  // We only apply warnings to shadowing domain cookies when scheme binding is
  // disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {net::features::kEnableSchemeBoundCookies,
           net::features::kEnablePortBoundCookies});

  std::vector<CanonicalCookie*> cookie_ptrs;
  CookieAccessResultList included;
  CookieAccessResultList excluded;

  auto reset = [&cookie_ptrs, &included, &excluded]() {
    cookie_ptrs.clear();
    included.clear();
    excluded.clear();
  };

  auto origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=origin", creation_time, server_time);
  auto origin_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo2=origin", creation_time, server_time);

  auto domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Shadowing domain cookie after the origin cookie.
  cookie_ptrs = {origin_cookie1.get(), origin_cookie2.get(),
                 domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {*domain_cookie1}));
  reset();

  // Shadowing domain cookie before the origin cookie.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {*domain_cookie1}));
  reset();

  auto domain_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo2=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Multiple different shadowing domain cookies.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), domain_cookie2.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(
      DomainCookiesHaveWarnings(included, {*domain_cookie1, *domain_cookie2}));
  reset();

  auto domain_cookie3 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo3=domain; Domain=" + https_www_foo_.domain(),
      creation_time, server_time);

  // Non-shadowing domain cookie shouldn't have a warning.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), domain_cookie2.get(),
                 domain_cookie3.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(
      DomainCookiesHaveWarnings(included, {*domain_cookie1, *domain_cookie2}));
  reset();

  auto sub_domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=subdomain; Domain=" + https_www_foo_.host(),
      creation_time, server_time);

  // If there are multiple domain cookies that shadow the same cookie, they
  // should all have a warning.
  cookie_ptrs = {domain_cookie1.get(), origin_cookie2.get(),
                 origin_cookie1.get(), sub_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(
      included, {*domain_cookie1, *sub_domain_cookie1}));
  reset();

  // Domain cookies may shadow each other.
  cookie_ptrs = {domain_cookie1.get(), sub_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  reset();

  auto path_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=pathorigin; Path=/bar", creation_time,
      server_time);

  // Origin cookies on different paths may not be shadowed, even if the
  // origin cookie wouldn't be included on this request.
  cookie_ptrs = {path_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {domain_cookie1.get()}));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {*domain_cookie1}));
  reset();

  auto insecure_origin_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(), "foo1=insecureorigin", creation_time, server_time);
  EXPECT_EQ(insecure_origin_cookie1->SourceScheme(),
            CookieSourceScheme::kNonSecure);

  // Origin cookies that have a warning for scheme binding don't affect domain
  // cookies.
  cookie_ptrs = {insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(included[0].access_result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SCHEME_MISMATCH));
  reset();

  auto insecure_domain_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_foo_.url(),
      "foo1=insecuredomain; Domain=" + http_www_foo_.domain(), creation_time,
      server_time);

  // Domain cookies that are excluded due to scheme binding shouldn't also get a
  // shadow warning.
  cookie_ptrs = {origin_cookie1.get(), insecure_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(
      included[1].access_result.status.HasExactlyWarningReasonsForTesting(
          {CookieInclusionStatus::WARN_SCHEME_MISMATCH}));
  reset();

  // If both domain and origin cookie have warnings due to scheme binding then
  // domain cookie shouldn't get shadowing warning.
  cookie_ptrs = {insecure_origin_cookie1.get(), insecure_domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(included[0].access_result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SCHEME_MISMATCH));
  EXPECT_TRUE(
      included[1].access_result.status.HasExactlyWarningReasonsForTesting(
          {CookieInclusionStatus::WARN_SCHEME_MISMATCH}));
  reset();

  cm->SetCookieAccessDelegate(std::make_unique<TestCookieAccessDelegate>());

  CookieURLHelper http_www_trustworthy =
      CookieURLHelper("http://www.trustworthysitefortestdelegate.example");
  CookieURLHelper https_www_trustworthy =
      CookieURLHelper("https://www.trustworthysitefortestdelegate.example");

  auto trust_origin_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(), "foo1=trustorigin", creation_time,
      server_time);

  auto secure_trust_domain_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(),
      "foo1=securetrustdomain; Domain=" + https_www_trustworthy.domain(),
      creation_time, server_time);
  auto secure_trust_domain_cookie2 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(),
      "foo2=securetrustdomain; Domain=" + https_www_trustworthy.domain(),
      creation_time, server_time);

  // Securely set domain cookie has warning when shadowing trustworthy-ly set
  // origin cookies.
  cookie_ptrs = {trust_origin_cookie1.get(), secure_trust_domain_cookie1.get(),
                 secure_trust_domain_cookie2.get()};
  cm->FilterCookiesWithOptions(http_www_trustworthy.url(), options,
                               &cookie_ptrs, &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(
      DomainCookiesHaveWarnings(included, {*secure_trust_domain_cookie1}));
  reset();

  auto trust_domain_cookie1 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(),
      "foo1=trustdomain; Domain=" + http_www_trustworthy.domain(),
      creation_time, server_time);
  auto trust_domain_cookie2 = CanonicalCookie::CreateForTesting(
      http_www_trustworthy.url(),
      "foo2=trustdomain; Domain=" + http_www_trustworthy.domain(),
      creation_time, server_time);
  auto secure_trust_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_trustworthy.url(), "foo1=securetrustorigin", creation_time,
      server_time);

  // Trustworthy-ly set domain cookies are excluded when shadowing securely set
  // origin cookies.
  cookie_ptrs = {secure_trust_origin_cookie1.get(), trust_domain_cookie1.get(),
                 trust_domain_cookie2.get()};
  cm->FilterCookiesWithOptions(http_www_trustworthy.url(), options,
                               &cookie_ptrs, &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {*trust_domain_cookie1}));
  reset();

  auto port_origin_cookie1 = CanonicalCookie::CreateForTesting(
      https_www_foo_.url(), "foo1=differentportorigin", creation_time,
      server_time);
  port_origin_cookie1->SetSourcePort(123);

  // Origin cookies that have warnings due to port binding don't affect domain
  // cookies.
  cookie_ptrs = {port_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(included[0].access_result.status.HasWarningReason(
      CookieInclusionStatus::WARN_PORT_MISMATCH));
  reset();

  auto port_insecure_origin_cookie1 =
      std::make_unique<CanonicalCookie>(*insecure_origin_cookie1);
  port_insecure_origin_cookie1->SetSourcePort(123);

  // Origin cookies that have warnings due to scheme and port binding don't
  // affect domain cookies.
  cookie_ptrs = {port_insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, cookie_ptrs));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(
      included[0].access_result.status.HasExactlyWarningReasonsForTesting(
          {CookieInclusionStatus::WARN_SCHEME_MISMATCH,
           CookieInclusionStatus::WARN_PORT_MISMATCH}));
  reset();

  // Enable port binding to test with port exclusions.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {net::features::kEnablePortBoundCookies},
      {net::features::kEnableSchemeBoundCookies});

  // Origin cookies that are excluded due to port binding don't affect domain
  // cookies.
  cookie_ptrs = {port_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {domain_cookie1.get()}));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(CookieListsMatch(excluded, {port_origin_cookie1.get()}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_PORT_MISMATCH}));
  reset();

  // Origin cookies that are excluded due to port binding and have a scheme
  // binding warning don't affect domain cookies.
  cookie_ptrs = {port_insecure_origin_cookie1.get(), domain_cookie1.get()};
  cm->FilterCookiesWithOptions(https_www_foo_.url(), options, &cookie_ptrs,
                               &included, &excluded);
  EXPECT_TRUE(CookieListsMatch(included, {domain_cookie1.get()}));
  EXPECT_TRUE(DomainCookiesHaveWarnings(included, {}));
  EXPECT_TRUE(CookieListsMatch(excluded, {port_insecure_origin_cookie1.get()}));
  EXPECT_TRUE(
      excluded[0].access_result.status.HasExactlyExclusionReasonsForTesting(
          {CookieInclusionStatus::EXCLUDE_PORT_MISMATCH}));
  EXPECT_TRUE(excluded[0].access_result.status.HasWarningReason(
      CookieInclusionStatus::WARN_SCHEME_MISMATCH));
  reset();
}

// This test sets a cookie (only checked using IsCanonicalForFromStorage)
// that's 300 days old and expires in 800 days. It checks that this cookie was
// stored, and then update it. It checks that the updated cookie has the
// creation and expiry dates expected.
TEST_F(CookieMonsterTest, FromStorageCookieCreated300DaysAgoThenUpdatedNow) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cookie_monster =
      std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cookie_monster->SetPersistSessionCookies(true);
  EXPECT_TRUE(GetAllCookies(cookie_monster.get()).empty());

  // Bypass IsCanonical and store a 300 day old cookie to bypass clamping.
  base::Time original_creation = base::Time::Now() - base::Days(300);
  base::Time original_expiry = original_creation + base::Days(800);
  CookieList list;
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "." + https_www_foo_.url().host(), "/", original_creation,
      original_expiry, base::Time(), base::Time(), true, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(SetAllCookies(cookie_monster.get(), list));

  // Verify the cookie exists and was not clamped, even if clamping is on.
  EXPECT_THAT(GetAllCookies(cookie_monster.get()),
              ElementsAre(MatchesCookieNameValueCreationExpiry(
                  "A", "B", original_creation, original_expiry)));

  // Update the cookie without bypassing clamping.
  base::Time new_creation = base::Time::Now();
  base::Time new_expiry = new_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          new_creation, new_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_THAT(
      GetAllCookies(cookie_monster.get()),
      ElementsAre(MatchesCookieNameValueCreationExpiry(
          "A", "B", original_creation, new_creation + base::Days(400))));
}

// This test sets a cookie (only checked using IsCanonicalForFromStorage)
// that's 500 days old and expires in 800 days. It checks that this cookie was
// stored, and then update it. It checks that the updated cookie has the
// creation and expiry dates expected.
TEST_F(CookieMonsterTest, FromStorageCookieCreated500DaysAgoThenUpdatedNow) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cookie_monster =
      std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cookie_monster->SetPersistSessionCookies(true);
  EXPECT_TRUE(GetAllCookies(cookie_monster.get()).empty());

  // Bypass IsCanonical and store a 500 day old cookie to bypass clamping.
  base::Time original_creation = base::Time::Now() - base::Days(500);
  base::Time original_expiry = original_creation + base::Days(800);
  CookieList list;
  list.push_back(*CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "." + https_www_foo_.url().host(), "/", original_creation,
      original_expiry, base::Time(), base::Time(), true, false,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(SetAllCookies(cookie_monster.get(), list));

  // Verify the cookie exists and was not clamped, even if clamping is on.
  EXPECT_THAT(GetAllCookies(cookie_monster.get()),
              ElementsAre(MatchesCookieNameValueCreationExpiry(
                  "A", "B", original_creation, original_expiry)));

  // Update the cookie without bypassing clamping.
  base::Time new_creation = base::Time::Now();
  base::Time new_expiry = new_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          new_creation, new_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_THAT(
      GetAllCookies(cookie_monster.get()),
      ElementsAre(MatchesCookieNameValueCreationExpiry(
          "A", "B", original_creation, new_creation + base::Days(400))));
}

// This test sets a cookie (checked using IsCanonical) that's 300 days old and
// expires in 800 days. It checks that this cookie was stored, and then update
// it. It checks that the updated cookie has the creation and expiry dates
// expected.
TEST_F(CookieMonsterTest, SanitizedCookieCreated300DaysAgoThenUpdatedNow) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cookie_monster =
      std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cookie_monster->SetPersistSessionCookies(true);
  EXPECT_TRUE(GetAllCookies(cookie_monster.get()).empty());

  // Store a 300 day old cookie without bypassing clamping.
  base::Time original_creation = base::Time::Now() - base::Days(300);
  base::Time original_expiry = original_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          original_creation, original_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_THAT(
      GetAllCookies(cookie_monster.get()),
      ElementsAre(MatchesCookieNameValueCreationExpiry(
          "A", "B", original_creation, original_creation + base::Days(400))));

  // Update the cookie without bypassing clamping.
  base::Time new_creation = base::Time::Now();
  base::Time new_expiry = new_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          new_creation, new_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_THAT(
      GetAllCookies(cookie_monster.get()),
      ElementsAre(MatchesCookieNameValueCreationExpiry(
          "A", "B", original_creation, new_creation + base::Days(400))));
}

// This test sets a cookie (checked using IsCanonical) that's 500 days old and
// expires in 800 days. It checks that this cookie was stored, and then update
// it. It checks that the updated cookie has the creation and expiry dates
// expected.
TEST_F(CookieMonsterTest, SanitizedCookieCreated500DaysAgoThenUpdatedNow) {
  auto store = base::MakeRefCounted<FlushablePersistentStore>();
  auto cookie_monster =
      std::make_unique<CookieMonster>(store.get(), net::NetLog::Get());
  cookie_monster->SetPersistSessionCookies(true);
  EXPECT_TRUE(GetAllCookies(cookie_monster.get()).empty());

  // Store a 500 day old cookie without bypassing clamping.
  base::Time original_creation = base::Time::Now() - base::Days(500);
  base::Time original_expiry = original_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          original_creation, original_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_TRUE(GetAllCookies(cookie_monster.get()).empty());

  // Update the cookie without bypassing clamping.
  base::Time new_creation = base::Time::Now();
  base::Time new_expiry = new_creation + base::Days(800);
  EXPECT_TRUE(SetCanonicalCookie(
      cookie_monster.get(),
      CanonicalCookie::CreateSanitizedCookie(
          https_www_foo_.url(), "A", "B", https_www_foo_.url().host(), "/",
          new_creation, new_expiry, base::Time(), true, false,
          CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT, std::nullopt,
          /*status=*/nullptr),
      https_www_foo_.url(), false));
  EXPECT_THAT(GetAllCookies(cookie_monster.get()),
              ElementsAre(MatchesCookieNameValueCreationExpiry(
                  "A", "B", new_creation, new_creation + base::Days(400))));
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookieMonsterTestPriorityGarbageCollectionObc,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookieMonsterTestGarbageCollectionObc,
                         testing::ValuesIn(std::vector<std::tuple<bool, bool>>{
                             {true, false},
                             {false, true},
                             {true, true}}));

}  // namespace net
