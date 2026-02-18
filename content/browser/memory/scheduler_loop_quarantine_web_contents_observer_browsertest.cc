// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/scheduler_loop_quarantine_web_contents_observer.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_host_resolver.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_alloc_features.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace content {
class SchedulerLoopQuarantineWebContentsObserverBrowserTestBase
    : public ContentBrowserTest {
 public:
  SchedulerLoopQuarantineWebContentsObserverBrowserTestBase() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    use_partition_alloc_as_malloc_ = true;
#else
    use_partition_alloc_as_malloc_ = false;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  bool use_partition_alloc_as_malloc_;
};

class SchedulerLoopQuarantineWebContentsObserverBrowserTest
    : public SchedulerLoopQuarantineWebContentsObserverBrowserTestBase {
 public:
  SchedulerLoopQuarantineWebContentsObserverBrowserTest() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    feature_list_.InitWithFeatures(
        {base::features::kPartitionAllocSchedulerLoopQuarantine,
         base::features::kPartitionAllocWithAdvancedChecks},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {});
#else
    feature_list_.InitWithFeatures(
        {},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {});
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// First we test that a web observer doesn't trigger if the feature is disabled.
class SchedulerLoopQuarantineWebContentsObserverBrowserTestDisabled
    : public SchedulerLoopQuarantineWebContentsObserverBrowserTestBase {
 public:
  SchedulerLoopQuarantineWebContentsObserverBrowserTestDisabled() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    feature_list_.InitWithFeatures(
        {},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {base::features::kPartitionAllocSchedulerLoopQuarantine,
         base::features::kPartitionAllocWithAdvancedChecks});
#else
    feature_list_.InitWithFeatures(
        {},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {});
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Disabled test so everything is EXPECT_FALSE.
IN_PROC_BROWSER_TEST_F(
    SchedulerLoopQuarantineWebContentsObserverBrowserTestDisabled,
    FirstNavigationDoesNotTrigger) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // If the PartitionAllocAsMalloc is built then this will be true.
  EXPECT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());
}

// Same test as the disabled one but now EXPECT_EQ if building with
// PartitionAllocAsMalloc & feature enabled.
IN_PROC_BROWSER_TEST_F(SchedulerLoopQuarantineWebContentsObserverBrowserTest,
                       FirstNavigationTriggers) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // If the PartitionAllocAsMalloc is built then this will be true.
  EXPECT_EQ(use_partition_alloc_as_malloc_,
            SchedulerLoopQuarantineWebContentsObserver::
                AlreadyTriggeredReconfiguration());
}

IN_PROC_BROWSER_TEST_F(SchedulerLoopQuarantineWebContentsObserverBrowserTest,
                       SecondNavigationDoesNotTrigger) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  const GURL url1 = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  // If the PartitionAllocAsMalloc is built then this will be true.
  EXPECT_EQ(use_partition_alloc_as_malloc_,
            SchedulerLoopQuarantineWebContentsObserver::
                AlreadyTriggeredReconfiguration());

  // To prove it only triggers once per observer instance, we can reset the
  // global state and see that a second navigation in the same tab does nothing,
  // because the observer has detached itself.
  SchedulerLoopQuarantineWebContentsObserver::ResetForTesting();
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  const GURL url2 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));

  // The observer is detached after the first `ReadyToCommitNavigation`, so
  // subsequent navigations in the same WebContents won't trigger it.
  EXPECT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());
}

IN_PROC_BROWSER_TEST_F(SchedulerLoopQuarantineWebContentsObserverBrowserTest,
                       NewTabNavigationTriggersIfNotTriggered) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  // Open a new tab, but we ignore about:blank because it doesn't have any
  // website generated content.
  ASSERT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));

  EXPECT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  // Navigate the new tab with content.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // If the PartitionAllocAsMalloc is built then this will be true.
  EXPECT_EQ(use_partition_alloc_as_malloc_,
            SchedulerLoopQuarantineWebContentsObserver::
                AlreadyTriggeredReconfiguration());
}
}  // namespace content
