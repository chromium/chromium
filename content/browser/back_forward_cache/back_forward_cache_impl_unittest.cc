// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache/back_forward_cache_impl.h"

#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

// Generates a unique GURL for each call.
// The sequence uses numeric hosts: "0.com", "1.com", "2.com", etc.
GURL GetNextUrl() {
  static int counter = 0;
  return GURL("http://" + base::NumberToString(counter++) + ".com");
}

void SimulateMemoryPressure(base::MemoryPressureLevel memory_pressure_level) {
  base::RunLoop run_loop;
  base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      memory_pressure_level, run_loop.QuitClosure());
  run_loop.Run();
}

class FrameDeletionObserver : public WebContentsObserver {
 public:
  FrameDeletionObserver(WebContents* web_contents,
                        RenderFrameHost* rfh,
                        base::OnceClosure quit_closure)
      : WebContentsObserver(web_contents),
        observed_rfh_(rfh),
        quit_closure_(std::move(quit_closure)) {}

  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    if (rfh == observed_rfh_) {
      observed_rfh_ = nullptr;
      std::move(quit_closure_).Run();
    }
  }

 private:
  raw_ptr<RenderFrameHost> observed_rfh_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class BackForwardCacheImplTest : public RenderViewHostImplTestHarness {
 public:
  BackForwardCacheImplTest() = default;
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        kAllowCrossOriginNotRestoredReasons);
  }

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> SetUpTree() {
    //     (a-1)
    //     /   |    \
    //  (b-1) (a-2) (b-4)
    //    |    |
    //  (b-2) (b-3)
    auto tree_a_1 = CreateSameOriginTree();
    auto tree_a_2 = CreateSameOriginTree();
    auto tree_b_1 = CreateCrossOriginTree(/*block=*/false);
    auto tree_b_2 = CreateCrossOriginTree(/*block=*/false);
    auto tree_b_3 = CreateCrossOriginTree(/*block=*/true);
    auto tree_b_4 = CreateCrossOriginTree(/*block=*/true);
    tree_b_1->AppendChild(std::move(tree_b_2));
    tree_a_2->AppendChild(std::move(tree_b_3));
    tree_a_1->AppendChild(std::move(tree_b_1));
    tree_a_1->AppendChild(std::move(tree_a_2));
    tree_a_1->AppendChild(std::move(tree_b_4));
    return tree_a_1;
  }

 private:
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> CreateSameOriginTree() {
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
        new BackForwardCacheCanStoreTreeResult(/*is_same_origin=*/true,
                                               GetNextUrl()));
    return tree;
  }

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> CreateCrossOriginTree(
      bool block) {
    BackForwardCacheCanStoreDocumentResult result;
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
        new BackForwardCacheCanStoreTreeResult(/*is_same_origin=*/false,
                                               GetNextUrl()));
    if (block) {
      BackForwardCacheCanStoreDocumentResult can_store;
      // Test blocking from cross-origin subframes.
      can_store.No(BackForwardCacheMetrics::NotRestoredReason::kErrorDocument);
      tree->AddReasonsToSubtreeRootFrom(std::move(can_store));
    }
    return tree;
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BackForwardCacheImplTest, CrossOriginReachableFrameCount) {
  auto tree_root = SetUpTree();
  // The reachable cross-origin frames are b-1 and b-3 and b-4.
  EXPECT_EQ(static_cast<int>(tree_root->GetCrossOriginReachableFrameCount()),
            3);
}

TEST_F(BackForwardCacheImplTest, CrossOriginAllMasked) {
  auto tree_root = SetUpTree();
  int index = 0;
  // All cross origin iframe information should be masked regardless of the
  // index.
  auto result = tree_root->GetWebExposedNotRestoredReasonsInternal(index);
  // Main frame has "masked" as a reason.
  EXPECT_EQ(result->reasons.size(), 1u);
  EXPECT_EQ(result->reasons[0]->name, "masked");
  EXPECT_FALSE(result->reasons[0]->source);

  // b-1 is masked.
  EXPECT_TRUE(result->same_origin_details->children[0]->reasons.empty());
  // b-3 is masked.
  EXPECT_TRUE(result->same_origin_details->children[1]
                  ->same_origin_details->children[0]
                  ->reasons.empty());
  // b-4 is masked.
  EXPECT_TRUE(result->same_origin_details->children[2]->reasons.empty());
}

class BackForwardCacheImplTestExposeCrossOrigin
    : public BackForwardCacheImplTest {
 public:
  BackForwardCacheImplTestExposeCrossOrigin() = default;
  void SetUp() override {
    BackForwardCacheImplTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        kAllowCrossOriginNotRestoredReasons);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BackForwardCacheImplTestExposeCrossOrigin, FirstCrossOriginReachable) {
  auto tree_root = SetUpTree();
  int index = 0;
  // First cross-origin reachable frame (b-1) should be unmasked.
  auto result = tree_root->GetWebExposedNotRestoredReasonsInternal(index);
  // Main frame has "masked" as a reason.
  EXPECT_EQ(result->reasons.size(), 1u);
  EXPECT_EQ(result->reasons[0]->name, "masked");
  EXPECT_FALSE(result->reasons[0]->source);
  // b-1 is unmasked, but reasons are empty because it does not have any
  // blocking reasons.
  EXPECT_TRUE(result->same_origin_details->children[0]->reasons.empty());
  // b-3 is masked.
  EXPECT_TRUE(result->same_origin_details->children[1]
                  ->same_origin_details->children[0]
                  ->reasons.empty());
  // b-4 is masked.
  EXPECT_TRUE(result->same_origin_details->children[2]->reasons.empty());
}

TEST_F(BackForwardCacheImplTestExposeCrossOrigin, SecondCrossOriginReachable) {
  auto tree_root = SetUpTree();
  int index = 1;
  // Second cross-origin reachable frame (b-3) should be unmasked.
  auto result = tree_root->GetWebExposedNotRestoredReasonsInternal(index);
  // Main frame has "masked" as a reason.
  EXPECT_EQ(result->reasons.size(), 1u);
  EXPECT_EQ(result->reasons[0]->name, "masked");
  EXPECT_FALSE(result->reasons[0]->source);

  // b-1 is masked.
  EXPECT_TRUE(result->same_origin_details->children[0]->reasons.empty());
  // b-3 is unmasked and has reasons {"masked"}.
  EXPECT_EQ(static_cast<int>(result->same_origin_details->children[1]
                                 ->same_origin_details->children[0]
                                 ->reasons.size()),
            1);
  auto& reason = result->same_origin_details->children[1]
                     ->same_origin_details->children[0]
                     ->reasons[0];
  EXPECT_EQ(reason->name, "masked");
  EXPECT_FALSE(result->reasons[0]->source);
  // b-4 is masked.
  EXPECT_TRUE(result->same_origin_details->children[2]->reasons.empty());
}

// Covers BackForwardCache's cache size-related values used in Stable.
// See docs/back_forward_cache_size.md for more details.
class BackForwardCacheActiveSizeTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kBackForwardCache,
          {{"cache_size", "6"}, {"foreground_cache_size", "2"}}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheActiveSizeTest, ActiveCacheSize) {
  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  // The default cache sizes specified by kBackForwardCacheSize takes precedence
  // over kBackForwardCache.
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 6u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());
}

// Covers overwriting BackForwardCache's cache size-related values.
// When "cache_size" or "foreground_cache_size" presents in both
// `kBackForwardCacheSize` and `features::kBackForwardCache`, the former should
// take precedence.
class BackForwardCacheOverwriteSizeTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{kBackForwardCacheSize,
          {{"cache_size", "8"}, {"foreground_cache_size", "4"}}},
         {features::kBackForwardCache,
          {{"cache_size", "6"}, {"foreground_cache_size", "2"}}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
    RenderViewHostImplTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheOverwriteSizeTest, OverwrittenCacheSize) {
  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 8u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(),
            std::optional<size_t>(4u));
  EXPECT_TRUE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());

  // Changing the embedder-supplied cache size will change the return value of
  // GetCacheSize() and disables foreground cache limit.
  bfcache_impl.SetEmbedderSuppliedCacheSize(3u);
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 3u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());

  contents()
      ->GetController()
      .GetBackForwardCache()
      .SetEmbedderSuppliedCacheSize(10u);
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 10u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());
}

// Covers BackForwardCache's default cache size-related values.
// Note that these tests don't cover the values configured from Finch.
class BackForwardCacheDefaultSizeTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        // Ensure BackForwardCache is enabled.
        {{features::kBackForwardCache, {}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
    RenderViewHostImplTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheDefaultSizeTest, DefaultCacheSize) {
  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  // Default cache sizes are specified by kBackForwardCacheSize.
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 6u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());

  // Changing the embedder-supplied cache size will change the return value of
  // GetCacheSize().
  bfcache_impl.SetEmbedderSuppliedCacheSize(3u);
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 3u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());

  bfcache_impl.SetEmbedderSuppliedCacheSize(10u);
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 10u);
  EXPECT_EQ(bfcache_impl.GetForegroundedEntriesCacheSize(), std::nullopt);
  EXPECT_FALSE(bfcache_impl.UsingForegroundBackgroundCacheSizeLimit());
}

class BackForwardCacheMemoryPressureTest
    : public RenderViewHostImplTestHarness,
      public WebContentsDelegate,
      public testing::WithParamInterface<
          std::tuple<base::MemoryPressureLevel, bool>> {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    contents()->SetDelegate(this);
  }

 private:
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(BackForwardCacheMemoryPressureTest, MemoryPressure) {
  const base::MemoryPressureLevel memory_pressure_level =
      std::get<0>(GetParam());
  const bool is_foreground = std::get<1>(GetParam());

  // Get an active URL.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());

  // Set up the cache with 4 entries by doing 4 more navigations.
  std::vector<base::WeakPtr<RenderFrameHostImpl>> cached_rfhs;
  for (int i = 0; i < 4; ++i) {
    cached_rfhs.push_back(contents()->GetPrimaryMainFrame()->GetWeakPtr());
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  }

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 4u);

  // Test foreground/background behavior.
  if (is_foreground) {
    contents()->WasShown();
  } else {
    contents()->WasHidden();
  }

  // After memory pressure, verify eviction logic.
  SimulateMemoryPressure(memory_pressure_level);

  size_t expected_limit = 0;
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    expected_limit = 0;
  } else {
    expected_limit = is_foreground ? 3 : 1;
  }

  EXPECT_EQ(bfcache_impl.GetEntries().size(), expected_limit);
  // Entries are evicted in a FIFO order.
  for (size_t i = 0; i < cached_rfhs.size(); ++i) {
    if (i < cached_rfhs.size() - expected_limit) {
      EXPECT_FALSE(cached_rfhs[i]);
    } else {
      EXPECT_TRUE(cached_rfhs[i]->IsInBackForwardCache());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheMemoryPressureTest,
    testing::Combine(
        // Memory pressure level.
        testing::Values(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                        base::MEMORY_PRESSURE_LEVEL_MODERATE),
        // Is foregrounded.
        testing::Bool()));

class BackForwardCacheMemoryPressureStatefulTest
    : public RenderViewHostImplTestHarness,
      public WebContentsDelegate,
      public testing::WithParamInterface<
          std::tuple<base::MemoryPressureLevel, bool>> {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{base::kStatefulMemoryPressure, {}}}),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    contents()->SetDelegate(this);
  }

  void TearDown() override {
    SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
    RenderViewHostImplTestHarness::TearDown();
  }

  void SimulateMemoryPressure(base::MemoryPressureLevel memory_pressure_level) {
    base::RunLoop run_loop;
    base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
        memory_pressure_level, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(BackForwardCacheMemoryPressureStatefulTest, MemoryPressure) {
  const base::MemoryPressureLevel memory_pressure_level =
      std::get<0>(GetParam());
  const bool is_foreground = std::get<1>(GetParam());

  // Get an active URL.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());

  // Set up the cache with 4 entries by doing 4 more navigations.
  std::vector<base::WeakPtr<RenderFrameHostImpl>> cached_rfhs;
  for (int i = 0; i < 4; ++i) {
    cached_rfhs.push_back(contents()->GetPrimaryMainFrame()->GetWeakPtr());
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  }

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 4u);

  // Test foreground/background behavior.
  if (is_foreground) {
    contents()->WasShown();
  } else {
    contents()->WasHidden();
  }

  // After memory pressure, verify eviction logic.
  SimulateMemoryPressure(memory_pressure_level);

  size_t expected_limit = 0;
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    expected_limit = 0;
  } else {
    expected_limit = is_foreground ? 3 : 1;
  }

  EXPECT_EQ(bfcache_impl.GetEntries().size(), expected_limit);
  // Entries are evicted in a FIFO order.
  for (size_t i = 0; i < cached_rfhs.size(); ++i) {
    if (i < cached_rfhs.size() - expected_limit) {
      EXPECT_FALSE(cached_rfhs[i]);
    } else {
      EXPECT_TRUE(cached_rfhs[i]->IsInBackForwardCache());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheMemoryPressureStatefulTest,
    testing::Combine(
        // Memory pressure level.
        testing::Values(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
                        base::MEMORY_PRESSURE_LEVEL_MODERATE),
        // Is foregrounded.
        testing::Bool()));

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       PressureDoesNotGrowCacheBeyondBaseline) {
  // Set a strict embedder limit of 1.
  contents()
      ->GetController()
      .GetBackForwardCache()
      .SetEmbedderSuppliedCacheSize(1);

  // Navigate twice to fill the cache to the limit.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);

  // Start FOREGROUND and trigger Moderate Pressure.
  // The foreground pressure limit is usually 3, which is larger than the
  // baseline limit of 1.
  contents()->WasShown();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // The limit should remain capped at the baseline of 1.
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);

  // The active limit should remain strictly capped at the baseline of 1, not
  // grow to 3!
  EXPECT_EQ(bfcache_impl.GetCacheSize(), 1u);
}

class BackForwardCacheVisibilityPressureTest
    : public RenderViewHostImplTestHarness,
      public WebContentsDelegate {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{base::kStatefulMemoryPressure, {}}}),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    contents()->SetDelegate(this);
  }

  void TearDown() override {
    SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
    RenderViewHostImplTestHarness::TearDown();
  }

  void SimulateMemoryPressure(base::MemoryPressureLevel memory_pressure_level) {
    base::RunLoop run_loop;
    base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
        memory_pressure_level, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BackForwardCacheVisibilityPressureTest, VisibilityChangeUnderPressure) {
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  bfcache_impl.SetEmbedderSuppliedCacheSize(5);

  // Set up the cache with 5 entries by doing 5 more navigations.
  std::vector<base::WeakPtr<RenderFrameHostImpl>> cached_rfhs;
  for (int i = 0; i < 5; ++i) {
    cached_rfhs.push_back(contents()->GetPrimaryMainFrame()->GetWeakPtr());
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  }
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 5u);

  // Start FOREGROUND and trigger Moderate Pressure.
  contents()->WasShown();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Expect limit to be 3 (Foreground Moderate limit).
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 3u);

  // Hide the WebContents.
  // Expect limit to update to 1 (Background Moderate limit) and immediately
  // prune.
  base::RunLoop run_loop;
  FrameDeletionObserver observer(contents(), cached_rfhs[3].get(),
                                 run_loop.QuitClosure());
  contents()->WasHidden();
  run_loop.Run();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);
}

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       SubsequentNavigationEnforcesPressureLimit) {
  // Navigate to fill cache with 3 entries. (Requires 4 navigations).
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      3u);

  // Trigger Moderate Pressure in Background (Limit 1).
  contents()->WasHidden();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Limit of 1 should be enforced immediately. (SimulateMemoryPressure runs a
  // loop, flushing eviction tasks).
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      1u);

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  RenderFrameHost* page2 =
      bfcache_impl.GetEntries().front()->render_frame_host();
  base::RunLoop run_loop;
  FrameDeletionObserver observer(contents(), page2, run_loop.QuitClosure());

  // Now navigate AGAIN to add a new entry.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  run_loop.Run();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);
}

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       InitialStateEnforcesPressureLimit) {
  // Simulate globally that we are under Critical Memory Pressure BEFORE the tab
  // is created.
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Create a brand new WebContents while pressure is active.
  std::unique_ptr<TestWebContents> new_contents =
      TestWebContents::Create(browser_context(), nullptr);
  new_contents->SetDelegate(this);

  // Navigate to add entries.
  NavigationSimulator::NavigateAndCommitFromBrowser(new_contents.get(),
                                                    GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(new_contents.get(),
                                                    GetNextUrl());

  RenderFrameHost* page1 = new_contents->GetPrimaryMainFrame();
  base::RunLoop run_loop;
  FrameDeletionObserver observer(new_contents.get(), page1,
                                 run_loop.QuitClosure());

  NavigationSimulator::NavigateAndCommitFromBrowser(new_contents.get(),
                                                    GetNextUrl());
  run_loop.Run();
  EXPECT_EQ(
      new_contents->GetController().GetBackForwardCache().GetEntries().size(),
      0u);
}

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       DeferredPruningEnforcesStatefulLimit) {
  // Navigate to fill cache with 2 entries. (Requires 3 navigations).
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      2u);

  // Start a pending navigation.
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(GetNextUrl(), contents());
  simulator->Start();
  EXPECT_TRUE(contents()->GetController().GetPendingEntry());

  // Trigger Moderate Pressure in Background (Limit 1).
  contents()->WasHidden();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Pruning should be deferred because of the pending navigation!
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      2u);

  // Commit the navigation. This should trigger the deferred pruning!
  // Note: Since entries don't have ACK in unit tests, pruning would be blocked
  // if not for the stateful pressure reason bypass. The success of this
  // eviction proves the pressure reason is correctly propagated!
  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  RenderFrameHost* page0 =
      bfcache_impl.GetEntries().back()->render_frame_host();
  base::RunLoop run_loop;
  FrameDeletionObserver observer(contents(), page0, run_loop.QuitClosure());

  simulator->Commit();
  run_loop.Run();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);
}

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       DeferredPruningEnforcesStatefulLimitOnVisibilityChange) {
  // Navigate to fill cache with 2 entries. (Requires 3 navigations).
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      2u);

  // Start FOREGROUND and trigger Moderate Pressure (Foreground Limit is 3).
  contents()->WasShown();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Start a pending navigation.
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(GetNextUrl(), contents());
  simulator->Start();
  EXPECT_TRUE(contents()->GetController().GetPendingEntry());

  // Hide the WebContents (Background Limit is 1).
  // Pruning should be deferred because of the pending navigation!
  contents()->WasHidden();
  EXPECT_EQ(
      contents()->GetController().GetBackForwardCache().GetEntries().size(),
      2u);

  // Commit the navigation. This should trigger the deferred pruning!
  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  RenderFrameHost* page0 =
      bfcache_impl.GetEntries().back()->render_frame_host();
  base::RunLoop run_loop;
  FrameDeletionObserver observer(contents(), page0, run_loop.QuitClosure());

  simulator->Commit();
  run_loop.Run();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 1u);
}

TEST_F(BackForwardCacheMemoryPressureStatefulTest,
       ModeratePressureDoesNotBypassAckWhenAboveBaseline) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kBFCachePerformanceManagerPolicy,
        {{"foreground_cache_size_on_moderate_pressure", "3"}}}},
      {});

  // Set baseline limit to 1.
  contents()
      ->GetController()
      .GetBackForwardCache()
      .SetEmbedderSuppliedCacheSize(1);

  // Navigate to add 3 entries. Since baseline is 1, and entries don me have
  // ACK, all 3 entries should remain in the cache despite standard limit
  // enforcement.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), GetNextUrl());

  auto& bfcache_impl = contents()->GetController().GetBackForwardCache();
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 3u);

  // Start FOREGROUND and trigger Moderate Pressure.
  // The foreground pressure limit is 3, which is larger than baseline 1.
  contents()->WasShown();
  SimulateMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Because memory pressure did not tighten the limit below baseline, it must
  // NOT bypass the ACK check. Therefore, no entries should be pruned!
  EXPECT_EQ(bfcache_impl.GetEntries().size(), 3u);
}

}  // namespace content
