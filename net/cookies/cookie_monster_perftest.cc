// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_monster_store_test.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "url/gurl.h"

namespace net {

namespace {

const int kNumCookies = 20000;
const char kCookieLine[] = "A  = \"b=;\\\"\"  ;secure;;;";
const char kGoogleURL[] = "http://www.foo.com";

static constexpr char kMetricPrefixParsedCookie[] = "ParsedCookie.";
static constexpr char kMetricPrefixCookieMonster[] = "CookieMonster.";
static constexpr char kMetricParseTimeMs[] = "parse_time";
static constexpr char kMetricAddTimeMs[] = "add_time";
static constexpr char kMetricQueryTimeMs[] = "query_time";
static constexpr char kMetricDeleteAllTimeMs[] = "delete_all_time";
static constexpr char kMetricQueryDomainTimeMs[] = "query_domain_time";
static constexpr char kMetricImportTimeMs[] = "import_time";
static constexpr char kMetricGetKeyTimeMs[] = "get_key_time";
static constexpr char kMetricGCTimeMs[] = "gc_time";

perf_test::PerfResultReporter SetUpParseReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixParsedCookie, story);
  reporter.RegisterImportantMetric(kMetricParseTimeMs, "ms");
  return reporter;
}

perf_test::PerfResultReporter SetUpCookieMonsterReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixCookieMonster, story);
  reporter.RegisterImportantMetric(kMetricAddTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricQueryTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricDeleteAllTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricQueryDomainTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricImportTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricGetKeyTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricGCTimeMs, "ms");
  return reporter;
}

class CookieMonsterTest : public testing::Test {
 public:
  CookieMonsterTest() {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

class CookieTestCallback {
 public:
  CookieTestCallback() : has_run_(false) {}

 protected:
  void WaitForCallback() {
    // Note that the performance tests currently all operate on a loaded cookie
    // store (or, more precisely, one that has no backing persistent store).
    // Therefore, callbacks will actually always complete synchronously. If the
    // tests get more advanced we need to add other means of signaling
    // completion.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(has_run_);
    has_run_ = false;
  }

  void Run() { has_run_ = true; }

  bool has_run_;
};

class SetCookieCallback : public CookieTestCallback {
 public:
  void SetCookie(CookieMonster* cm,
                 const GURL& gurl,
                 const std::string& cookie_line) {
    auto cookie = CanonicalCookie::Create(gurl, cookie_line, base::Time::Now(),
                                          base::nullopt /* server_time */);
    cm->SetCanonicalCookieAsync(
        std::move(cookie), gurl.scheme(), options_,
        base::BindOnce(&SetCookieCallback::Run, base::Unretained(this)));
    WaitForCallback();
  }

 private:
  void Run(CanonicalCookie::CookieInclusionStatus status) {
    EXPECT_TRUE(status.IsInclude());
    CookieTestCallback::Run();
  }
  CookieOptions options_;
};

class GetCookieListCallback : public CookieTestCallback {
 public:
  const CookieList& GetCookieList(CookieMonster* cm, const GURL& gurl) {
    cm->GetCookieListWithOptionsAsync(
        gurl, options_,
        base::BindOnce(&GetCookieListCallback::Run, base::Unretained(this)));
    WaitForCallback();
    return cookie_list_;
  }

 private:
  void Run(const CookieStatusList& cookie_list,
           const CookieStatusList& excluded_cookies) {
    cookie_list_ = cookie_util::StripStatuses(cookie_list);
    CookieTestCallback::Run();
  }
  CookieList cookie_list_;
  CookieOptions options_;
};

class GetAllCookiesCallback : public CookieTestCallback {
 public:
  CookieList GetAllCookies(CookieMonster* cm) {
    cm->GetAllCookiesAsync(
        base::BindOnce(&GetAllCookiesCallback::Run, base::Unretained(this)));
    WaitForCallback();
    return cookies_;
  }

 private:
  void Run(const CookieList& cookies) {
    cookies_ = cookies;
    CookieTestCallback::Run();
  }
  CookieList cookies_;
};

}  // namespace

TEST(ParsedCookieTest, TestParseCookies) {
  std::string cookie(kCookieLine);
  auto reporter = SetUpParseReporter("parse_cookies");
  base::ElapsedTimer timer;
  for (int i = 0; i < kNumCookies; ++i) {
    ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  reporter.AddResult(kMetricParseTimeMs, timer.Elapsed().InMillisecondsF());
}

TEST(ParsedCookieTest, TestParseBigCookies) {
  std::string cookie(3800, 'z');
  cookie += kCookieLine;
  auto reporter = SetUpParseReporter("parse_big_cookies");
  base::ElapsedTimer timer;
  for (int i = 0; i < kNumCookies; ++i) {
    ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  reporter.AddResult(kMetricParseTimeMs, timer.Elapsed().InMillisecondsF());
}

TEST_F(CookieMonsterTest, TestAddCookiesOnSingleHost) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  std::vector<std::string> cookies;
  for (int i = 0; i < kNumCookies; i++) {
    cookies.push_back(base::StringPrintf("a%03d=b", i));
  }

  SetCookieCallback setCookieCallback;

  // Add a bunch of cookies on a single host
  auto reporter = SetUpCookieMonsterReporter("single_host");
  base::ElapsedTimer add_timer;

  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    setCookieCallback.SetCookie(cm.get(), GURL(kGoogleURL), *it);
  }
  reporter.AddResult(kMetricAddTimeMs, add_timer.Elapsed().InMillisecondsF());

  GetCookieListCallback getCookieListCallback;

  base::ElapsedTimer query_timer;
  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    getCookieListCallback.GetCookieList(cm.get(), GURL(kGoogleURL));
  }
  reporter.AddResult(kMetricQueryTimeMs,
                     query_timer.Elapsed().InMillisecondsF());

  base::ElapsedTimer delete_all_timer;
  cm->DeleteAllAsync(CookieMonster::DeleteCallback());
  base::RunLoop().RunUntilIdle();
  reporter.AddResult(kMetricDeleteAllTimeMs,
                     delete_all_timer.Elapsed().InMillisecondsF());
}

TEST_F(CookieMonsterTest, TestAddCookieOnManyHosts) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  std::string cookie(kCookieLine);
  std::vector<GURL> gurls;  // just wanna have ffffuunnn
  for (int i = 0; i < kNumCookies; ++i) {
    gurls.push_back(GURL(base::StringPrintf("https://a%04d.izzle", i)));
  }

  SetCookieCallback setCookieCallback;

  // Add a cookie on a bunch of host
  auto reporter = SetUpCookieMonsterReporter("many_hosts");
  base::ElapsedTimer add_timer;
  for (std::vector<GURL>::const_iterator it = gurls.begin(); it != gurls.end();
       ++it) {
    setCookieCallback.SetCookie(cm.get(), *it, cookie);
  }
  reporter.AddResult(kMetricAddTimeMs, add_timer.Elapsed().InMillisecondsF());

  GetCookieListCallback getCookieListCallback;

  base::ElapsedTimer query_timer;
  for (std::vector<GURL>::const_iterator it = gurls.begin(); it != gurls.end();
       ++it) {
    getCookieListCallback.GetCookieList(cm.get(), *it);
  }
  reporter.AddResult(kMetricQueryTimeMs,
                     query_timer.Elapsed().InMillisecondsF());

  base::ElapsedTimer delete_all_timer;
  cm->DeleteAllAsync(CookieMonster::DeleteCallback());
  base::RunLoop().RunUntilIdle();
  reporter.AddResult(kMetricDeleteAllTimeMs,
                     delete_all_timer.Elapsed().InMillisecondsF());
}

TEST_F(CookieMonsterTest, TestDomainTree) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  GetCookieListCallback getCookieListCallback;
  SetCookieCallback setCookieCallback;
  const char domain_cookie_format_tree[] = "a=b; domain=%s";
  const std::string domain_base("top.com");

  std::vector<std::string> domain_list;

  // Create a balanced binary tree of domains on which the cookie is set.
  domain_list.push_back(domain_base);
  for (int i1 = 0; i1 < 2; i1++) {
    std::string domain_base_1((i1 ? "a." : "b.") + domain_base);
    EXPECT_EQ("top.com", cm->GetKey(domain_base_1));
    domain_list.push_back(domain_base_1);
    for (int i2 = 0; i2 < 2; i2++) {
      std::string domain_base_2((i2 ? "a." : "b.") + domain_base_1);
      EXPECT_EQ("top.com", cm->GetKey(domain_base_2));
      domain_list.push_back(domain_base_2);
      for (int i3 = 0; i3 < 2; i3++) {
        std::string domain_base_3((i3 ? "a." : "b.") + domain_base_2);
        EXPECT_EQ("top.com", cm->GetKey(domain_base_3));
        domain_list.push_back(domain_base_3);
        for (int i4 = 0; i4 < 2; i4++) {
          std::string domain_base_4((i4 ? "a." : "b.") + domain_base_3);
          EXPECT_EQ("top.com", cm->GetKey(domain_base_4));
          domain_list.push_back(domain_base_4);
        }
      }
    }
  }

  EXPECT_EQ(31u, domain_list.size());
  for (std::vector<std::string>::const_iterator it = domain_list.begin();
       it != domain_list.end(); it++) {
    GURL gurl("https://" + *it + "/");
    const std::string cookie =
        base::StringPrintf(domain_cookie_format_tree, it->c_str());
    setCookieCallback.SetCookie(cm.get(), gurl, cookie);
  }

  GetAllCookiesCallback getAllCookiesCallback;
  EXPECT_EQ(31u, getAllCookiesCallback.GetAllCookies(cm.get()).size());

  GURL probe_gurl("https://b.a.b.a.top.com/");
  const CookieList& cookie_list =
      getCookieListCallback.GetCookieList(cm.get(), probe_gurl);
  EXPECT_EQ(5u, cookie_list.size())
      << CanonicalCookie::BuildCookieLine(cookie_list);
  auto reporter = SetUpCookieMonsterReporter("tree");
  base::ElapsedTimer query_domain_timer;
  for (int i = 0; i < kNumCookies; i++) {
    getCookieListCallback.GetCookieList(cm.get(), probe_gurl);
  }
  reporter.AddResult(kMetricQueryDomainTimeMs,
                     query_domain_timer.Elapsed().InMillisecondsF());
}

TEST_F(CookieMonsterTest, TestDomainLine) {
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  SetCookieCallback setCookieCallback;
  GetCookieListCallback getCookieListCallback;
  std::vector<std::string> domain_list;
  GURL probe_gurl("https://b.a.b.a.top.com/");

  // Create a line of 32 domain cookies such that all cookies stored
  // by effective TLD+1 will apply to probe GURL.
  // (TLD + 1 is the level above .com/org/net/etc, e.g. "top.com"
  // or "google.com".  "Effective" is added to include sites like
  // bbc.co.uk, where the effetive TLD+1 is more than one level
  // below the top level.)
  domain_list.push_back("a.top.com");
  domain_list.push_back("b.a.top.com");
  domain_list.push_back("a.b.a.top.com");
  domain_list.push_back("b.a.b.a.top.com");
  EXPECT_EQ(4u, domain_list.size());

  const char domain_cookie_format_line[] = "a%03d=b; domain=%s";
  for (int i = 0; i < 8; i++) {
    for (std::vector<std::string>::const_iterator it = domain_list.begin();
         it != domain_list.end(); it++) {
      GURL gurl("https://" + *it + "/");
      const std::string cookie =
          base::StringPrintf(domain_cookie_format_line, i, it->c_str());
      setCookieCallback.SetCookie(cm.get(), gurl, cookie);
    }
  }

  const CookieList& cookie_list =
      getCookieListCallback.GetCookieList(cm.get(), probe_gurl);
  EXPECT_EQ(32u, cookie_list.size());
  auto reporter = SetUpCookieMonsterReporter("line");
  base::ElapsedTimer query_domain_timer;
  for (int i = 0; i < kNumCookies; i++) {
    getCookieListCallback.GetCookieList(cm.get(), probe_gurl);
  }
  reporter.AddResult(kMetricQueryDomainTimeMs,
                     query_domain_timer.Elapsed().InMillisecondsF());
}

TEST_F(CookieMonsterTest, TestImport) {
  scoped_refptr<MockPersistentCookieStore> store(new MockPersistentCookieStore);
  std::vector<std::unique_ptr<CanonicalCookie>> initial_cookies;
  GetCookieListCallback getCookieListCallback;

  // We want to setup a fairly large backing store, with 300 domains of 50
  // cookies each.  Creation times must be unique.
  int64_t time_tick(base::Time::Now().ToInternalValue());

  for (int domain_num = 0; domain_num < 300; domain_num++) {
    GURL gurl(base::StringPrintf("http://www.Domain_%d.com", domain_num));
    for (int cookie_num = 0; cookie_num < 50; cookie_num++) {
      std::string cookie_line(
          base::StringPrintf("Cookie_%d=1; Path=/", cookie_num));
      AddCookieToList(gurl, cookie_line,
                      base::Time::FromInternalValue(time_tick++),
                      &initial_cookies);
    }
  }

  store->SetLoadExpectation(true, std::move(initial_cookies));

  std::unique_ptr<CookieMonster> cm(new CookieMonster(store.get(), nullptr));

  // Import will happen on first access.
  GURL gurl("www.foo.com");
  CookieOptions options;
  auto reporter = SetUpCookieMonsterReporter("from_store");
  base::ElapsedTimer import_timer;
  getCookieListCallback.GetCookieList(cm.get(), gurl);
  reporter.AddResult(kMetricImportTimeMs,
                     import_timer.Elapsed().InMillisecondsF());

  // Just confirm keys were set as expected.
  EXPECT_EQ("domain_1.com", cm->GetKey("www.Domain_1.com"));
}

TEST_F(CookieMonsterTest, TestGetKey) {
  std::unique_ptr<CookieMonster> cm(new CookieMonster(nullptr, nullptr));
  auto reporter = SetUpCookieMonsterReporter("baseline_story");
  base::ElapsedTimer get_key_timer;
  for (int i = 0; i < kNumCookies; i++)
    cm->GetKey("www.foo.com");
  reporter.AddResult(kMetricGetKeyTimeMs,
                     get_key_timer.Elapsed().InMillisecondsF());
}

// This test is probing for whether garbage collection happens when it
// shouldn't.  This will not in general be visible functionally, since
// if GC runs twice in a row without any change to the store, the second
// GC run will not do anything the first one didn't.  That's why this is
// a performance test.  The test should be considered to pass if all the
// times reported are approximately the same--this indicates that no GC
// happened repeatedly for any case.
TEST_F(CookieMonsterTest, TestGCTimes) {
  SetCookieCallback setCookieCallback;

  const struct TestCase {
    const char* const name;
    size_t num_cookies;
    size_t num_old_cookies;
  } test_cases[] = {
      {
       // A whole lot of recent cookies; gc shouldn't happen.
       "all_recent",
       CookieMonster::kMaxCookies * 2,
       0,
      },
      {
       // Some old cookies, but still overflowing max.
       "mostly_recent",
       CookieMonster::kMaxCookies * 2,
       CookieMonster::kMaxCookies / 2,
      },
      {
       // Old cookies enough to bring us right down to our purge line.
       "balanced",
       CookieMonster::kMaxCookies * 2,
       CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies + 1,
      },
      {
       "mostly_old",
       // Old cookies enough to bring below our purge line (which we
       // shouldn't do).
       CookieMonster::kMaxCookies * 2,
       CookieMonster::kMaxCookies * 3 / 4,
      },
      {
       "less_than_gc_thresh",
       // Few enough cookies that gc shouldn't happen at all.
       CookieMonster::kMaxCookies - 5,
       0,
      },
  };
  for (const auto& test_case : test_cases) {
    std::unique_ptr<CookieMonster> cm = CreateMonsterFromStoreForGC(
        test_case.num_cookies, test_case.num_old_cookies, 0, 0,
        CookieMonster::kSafeFromGlobalPurgeDays * 2);

    GURL gurl("http://foo.com");
    std::string cookie_line("z=3");
    // Trigger the Garbage collection we're allowed.
    setCookieCallback.SetCookie(cm.get(), gurl, cookie_line);

    auto reporter = SetUpCookieMonsterReporter(test_case.name);
    base::ElapsedTimer gc_timer;
    for (int i = 0; i < kNumCookies; i++)
      setCookieCallback.SetCookie(cm.get(), gurl, cookie_line);
    reporter.AddResult(kMetricGCTimeMs, gc_timer.Elapsed().InMillisecondsF());
  }
}

}  // namespace net
