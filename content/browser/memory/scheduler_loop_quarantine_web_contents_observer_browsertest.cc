// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/scheduler_loop_quarantine_web_contents_observer.h"

#include <algorithm>
#include <string>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
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
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace content {
namespace {
// We currently only test the browser UI thread quarantine, to minimize the
// impact if params get changed we only do the bar minimum config the test
// needs.
constexpr char kQuarantineConfigJson[] = R"(
  {
    "browser": {
      "main": {
        "enable-quarantine":true,
        "enable-zapping":true,
        "branch-capacity-in-bytes":524288
      }
    }
  })";
// The finch jsons requires there be no spaces or new lines for some reason
// (encoded or otherwise).
std::string GetQuarantineConfigJson() {
  std::string config_json = kQuarantineConfigJson;
  base::RemoveChars(kQuarantineConfigJson, " \n", &config_json);
  return base::EscapeQueryParamValue(config_json,
                                     /*use_plus=*/false);
}
}  // namespace

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
    feature_list_.InitWithFeaturesAndParameters(
        {{base::features::kPartitionAllocSchedulerLoopQuarantine,
          std::map<std::string, std::string> {
            { base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
              GetQuarantineConfigJson() }
          }},
         { base::features::kPartitionAllocWithAdvancedChecks,
           {} }},
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
    // Ensure even when USE_PARTITION_ALLOC_AS_MALLOC is false we use the
    // function and that the string parses properly.
    EXPECT_TRUE(!GetQuarantineConfigJson().empty());
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

// This helper function, observer, and test only make sense when partition alloc
// is malloc.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// By allocating and freeing a pointer we can check if the quarantine is active
// or not.
bool IsQuarantineActive() {
  auto* root = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  CHECK(root != nullptr);
  partition_alloc::internal::
      ScopedSchedulerLoopQuarantineBranchAccessorForTesting branch_accessor(
          root);
  void* ptr = root->Alloc(10);
  CHECK(ptr != nullptr);
  CHECK(!branch_accessor.IsQuarantined(ptr));
  // We explicitly request the quarantine here, but if it was paused it might
  // return false.
  root->Free<partition_alloc::FreeFlags::kSchedulerLoopQuarantine>(ptr);
  return branch_accessor.IsQuarantined(ptr);
}

// A helper class that we use be able to check mid navigation if the quarantine
// is active. It will fail the test if the quarantine is active during the
// ReadyToCommitNavigation stage.
class TestQuarantinePausedWebContentsObserver
    : public WebContentsObserver,
      public WebContentsUserData<TestQuarantinePausedWebContentsObserver> {
  explicit TestQuarantinePausedWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        WebContentsUserData<TestQuarantinePausedWebContentsObserver>(
            *web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(IsQuarantineActive());
  }

 private:
  friend class WebContentsUserData<TestQuarantinePausedWebContentsObserver>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TestQuarantinePausedWebContentsObserver);

IN_PROC_BROWSER_TEST_F(SchedulerLoopQuarantineWebContentsObserverBrowserTest,
                       SecondNavigationPausesQuarantine) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(SchedulerLoopQuarantineWebContentsObserver::
                   AlreadyTriggeredReconfiguration());

  const GURL url1 = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  // If the PartitionAllocAsMalloc is built then this will be true.
  EXPECT_EQ(use_partition_alloc_as_malloc_,
            SchedulerLoopQuarantineWebContentsObserver::
                AlreadyTriggeredReconfiguration());

  // Before the navigation starts the quarantine is still active.
  EXPECT_TRUE(IsQuarantineActive());
  // The second navigation should block the quarantine.
  TestQuarantinePausedWebContentsObserver::CreateForWebContents(web_contents());
  const GURL url2 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  // But after the navigation finishes it should be active again.
  EXPECT_TRUE(IsQuarantineActive());
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

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
