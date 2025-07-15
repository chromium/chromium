// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/back_forward_cache_browsertest.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

class BackgroundForegroundProcessLimitBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableCacheSize(kBackForwardCacheSize, kForegroundBackForwardCacheSize);
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  void ExpectCached(const RenderFrameHostImplWrapper& rfh,
                    bool cached,
                    bool backgrounded) {
    EXPECT_FALSE(rfh.IsDestroyed());
    EXPECT_EQ(cached, rfh->IsInBackForwardCache());
    EXPECT_EQ(backgrounded, rfh->GetProcess()->GetPriority() ==
                                base::Process::Priority::kBestEffort);
  }
  // The number of pages the BackForwardCache can hold per tab.
  const size_t kBackForwardCacheSize = 4;
  const size_t kForegroundBackForwardCacheSize = 2;
  const size_t kPruneSize = 1u;
  const NotRestoredReason kPruneReason =
      NotRestoredReason::kCacheLimitPrunedOnModerateMemoryPressure;
};

// Test that a series of same-site navigations (which use the same process)
// uses the foreground limit.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    CacheEvictionSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(
        "a.com", base::StringPrintf("/title1.html?i=%zu", i)));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kForegroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kForegroundBackForwardCacheSize) {
        // All of the processes should be in the foreground.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/false);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kForegroundBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored({NotRestoredReason::kForegroundCacheLimit}, {}, {}, {},
                        {}, FROM_HERE);
    }
  }
}

// Test that a series of cross-site navigations (which use different processes)
// use the background limit.
//
// TODO(crbug.com/40179515): This test is flaky. It has been re-enabled with
// improved failure output (https://crrev.com/c/2862346). It's OK to disable it
// again when it fails.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    CacheEvictionCrossSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    // Note: do NOT use .com domains here because a4.com is on the HSTS preload
    // list, which will cause our test requests to timeout.
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.test", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kBackgroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kBackForwardCacheSize) {
        EXPECT_FALSE(rfhs[j].IsDestroyed());
        // Pages except the active one should be cached and in the background.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/i != j);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored({NotRestoredReason::kCacheLimit}, {}, {}, {}, {},
                        FROM_HERE);
    }
  }
}

// Test that pruning a series of cross-site navigations (which use different
// processes) evicts the right entries with the right reason.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    PruneCrossSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i < kBackForwardCacheSize; ++i) {
    SCOPED_TRACE(i);
    // Note: do NOT use .com domains here because a4.com is on the HSTS preload
    // list, which will cause our test requests to timeout.
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.test", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);
  }

  CHECK_LE(kPruneSize, kBackForwardCacheSize);

  // Prune the BFCache entries.
  web_contents()->GetController().GetBackForwardCache().Prune(kPruneSize,
                                                              kPruneReason);

  for (int i = kBackForwardCacheSize - 1 - 1 - kPruneSize; i >= 0; --i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(rfhs[i].WaitUntilRenderFrameDeleted());
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i < kBackForwardCacheSize - 1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    // The first `kPruneSize` navigation should be restored from the cache. The
    // rest should not.
    if (i < kPruneSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
    }
  }
}

namespace {

const char kPrioritizedPageURL[] = "search.result";

}  // namespace

class BackForwardCacheLimitForPrioritizedPagesBrowserTest
    : public BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
      public testing::WithParamInterface<std::string> {
 protected:
  // Mock subclass of ContentBrowserClient that will determine if the url is
  // prioritized by checking against `kPrioritizedPageURL`.
  class MockContentBrowserClientWithPrioritizedBackForwardCacheEntry
      : public ContentBrowserTestContentBrowserClient {
   public:
    // ContentBrowserClient overrides:
    bool ShouldPrioritizeForBackForwardCache(BrowserContext* browser_context,
                                             const GURL& url) override {
      return url.DomainIs(kPrioritizedPageURL);
    }
  };

  void SetUpOnMainThread() override {
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest::
        SetUpOnMainThread();
    test_client_ = std::make_unique<
        MockContentBrowserClientWithPrioritizedBackForwardCacheEntry>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(kBackForwardCachePrioritizedEntry, "level",
                              GetParam());
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest::
        SetUpCommandLine(command_line);
  }

  bool ShouldPrioritizeWhenClearAllUnlessNoEviction() {
    return GetParam() == "prioritize-unless-should-clear-all-and-no-eviction";
  }

 private:
  std::unique_ptr<MockContentBrowserClientWithPrioritizedBackForwardCacheEntry>
      test_client_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheLimitForPrioritizedPagesBrowserTest,
    testing::Values("prioritize-unless-should-clear-all",
                    "prioritize-unless-should-clear-all-and-no-eviction"));

// Test that both when pruning with size 0, if no other eviction happens, the
// prioritized entry would be evicted.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       PruneToZero_NoOtherEvictionHappens) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 1 entries in the BFCache list for this test.
  CHECK_GE(kBackForwardCacheSize, 1u);

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));

  // Now the BFCache entry list is: [pp, b].
  // Prune the BFCache entries to 0.
  web_contents()->GetController().GetBackForwardCache().Prune(0, kPruneReason);
  // All the entries should be evicted
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
}

// Test that both when pruning with size 0, if there is another entry evicted,
// the prioritized entry would be:
// - evicted if the level is prioritize-unless-should-clear-all
// - not evicted if the level is
// prioritize-unless-should-clear-all-and-no-eviction.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       PruneToZero_OtherEvictionHappens) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 2 entries in the BFCache list for this test.
  CHECK_GE(kBackForwardCacheSize, 2u);

  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));

  // Now the BFCache entry list is: [a, pp, b].
  // Prune the BFCache entries to 0.
  web_contents()->GetController().GetBackForwardCache().Prune(0, kPruneReason);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  if (ShouldPrioritizeWhenClearAllUnlessNoEviction()) {
    // If the level is prioritize-unless-should-clear-all-and-no-eviction, the
    // prioritized entry should be restored since the some other eviction
    // happens.
    ExpectRestored(FROM_HERE);
  } else {
    // Otherwise the prioritized entry should be evicted as well.
    ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
  }
  // The non-prioritized entry should always be evicted.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
}

// Test that when pruning with a positive number size, the last prioritized
// entry outside the limit will not be evicted.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       PruneToNonZero_PrioritizedEntryOutsideLimit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 4 entries in the BFCache list for this test.
  CHECK_GE(kBackForwardCacheSize, 4u);

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title2.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Now the BFCache entry list is: [pe1, a, pe2, b].
  // Prune the BFCache entries to 1, the result should be:
  // [pe1(evicted), a(evicted), pe2(prioritized entry special rule), b].
  web_contents()->GetController().GetBackForwardCache().Prune(1, kPruneReason);

  // The last non-prioritized entry should be restored because it's within the
  // cache limit.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // The last prioritized entry should be restored since it's the special
  // prioritized entry.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // The other entries (including the prioritized one) should not be restored.
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
  }
}

// Test that when pruning with a positive number size, the last prioritized
// entry inside the limit should be counted as the regular cache.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       PruneToNonZero_PrioritizedEntryInsideLimit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 2 entries in the BFCache list for this test.
  CHECK_GE(kBackForwardCacheSize, 2u);

  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title2.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));

  // Now the BFCache entry list is: [a, pe].
  // Prune the BFCache entries to 1, the result should be:
  // [a(evicted), pe].
  web_contents()->GetController().GetBackForwardCache().Prune(1, kPruneReason);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
}

// Test that when pruning with a positive number size while there is already an
// old prioritized entry kept in cache before, it will be replaced by the newer
// prioritized entry.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       PruneToNonZeroTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 4 entries in the BFCache list for this test.
  CHECK_LE(kBackForwardCacheSize, 4u);

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));

  // Now the BFCache entry list is: [pe1, a].
  // Prune the BFCache entries to 1, the result should still be
  // [pe1(prioritized entry special rule), a].
  web_contents()->GetController().GetBackForwardCache().Prune(1, kPruneReason);

  // The last non-prioritized entry should be restored because it's within the
  // cache limit.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // The last prioritized entry should be restored since it's the special
  // prioritized entry.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);

  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("c.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title2.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("d.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("e.test", "/title1.html")));

  // Now the BFCache entry list is: [pe1, c, pe2, d].
  // Prune the BFCache entries to 1, the result should still be
  // [pe1(evicted), a(evicted), pe2(prioritized entry special rule), d].
  web_contents()->GetController().GetBackForwardCache().Prune(1, kPruneReason);

  // The last non-prioritized entry should be restored because it's within the
  // cache limit.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // The last prioritized entry should be restored since it's the special
  // prioritized entry.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // The other entries (including the prioritized one) should not be restored.
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    ExpectNotRestored({kPruneReason}, {}, {}, {}, {}, FROM_HERE);
  }
}

// Test that the prioritized BFCache entry will not be evicted even when another
// entry is stored and exceeds the limit.
IN_PROC_BROWSER_TEST_P(BackForwardCacheLimitForPrioritizedPagesBrowserTest,
                       CacheLimitReached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // We need at least 1 entry in the BFCache list for this test.
  CHECK_GE(kBackForwardCacheSize, 1u);

  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kPrioritizedPageURL, "/title1.html")));

  // Fill the BFCache with more entry and make it just exceeds the limit, the
  // result should be:
  // [pe(prioritized entry special rule), a0, a1, ...].
  for (size_t i = 0; i <= kBackForwardCacheSize; ++i) {
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     base::StringPrintf("a%zu.test", i), "/title1.html")));
  }
  // For the entries within cache size limit, they should be restored.
  for (size_t i = 0; i < kBackForwardCacheSize; ++i) {
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    ExpectRestored(FROM_HERE);
  }
  // The prioritized entry should be restored as well even if it's outside the
  // limit.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that the cache responds to processes switching from background to
// foreground. We set things up so that we have
// Cached sites:
//   a0.test
//   a1.test
//   a2.test
//   a3.test
// and the active page is a4.test. Then set the process for a[1-3] to
// foregrounded so that there are 3 entries whose processes are foregrounded.
// BFCache should evict the eldest (a1) leaving a0 because despite being older,
// it is backgrounded. Setting the priority directly is not ideal but there is
// no reliable way to cause the processes to go into the foreground just by
// navigating because proactive browsing instance swap makes it impossible to
// reliably create a new a1.test renderer in the same process as the old
// a1.test.
//
// Note that we do NOT use .com domains because a4.com is on the HSTS preload
// list.  Since our test server doesn't use HTTPS, using a4.com results in the
// test timing out.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    ChangeToForeground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  // Navigate through a[0-3].com.
  for (size_t i = 0; i < kBackForwardCacheSize; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.test", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);
  }
  // Check that a0-2 are cached and backgrounded.
  for (size_t i = 0; i < kBackForwardCacheSize - 1; ++i) {
    SCOPED_TRACE(i);
    ExpectCached(rfhs[i], /*cached=*/true, /*backgrounded=*/true);
  }

  // Navigate to a page which causes the processes for a[1-3] to be
  // foregrounded.
  GURL url(embedded_test_server()->GetURL("a4.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Assert that we really have set up the situation we want where the processes
  // are shared and in the foreground.
  RenderFrameHostImpl* rfh = current_frame_host();
  ASSERT_NE(rfh->GetProcess()->GetPriority(),
            base::Process::Priority::kBestEffort);

  rfhs[1]->GetProcess()->OnMediaStreamAdded();
  rfhs[2]->GetProcess()->OnMediaStreamAdded();
  rfhs[3]->GetProcess()->OnMediaStreamAdded();

  // The page should be evicted.
  ASSERT_TRUE(rfhs[1].WaitUntilRenderFrameDeleted());

  // Check that a0 is cached and backgrounded.
  ExpectCached(rfhs[0], /*cached=*/true, /*backgrounded=*/true);
  // Check that a2-3 are cached and foregrounded.
  ExpectCached(rfhs[2], /*cached=*/true, /*backgrounded=*/false);
  ExpectCached(rfhs[3], /*cached=*/true, /*backgrounded=*/false);
}

}  // namespace content
