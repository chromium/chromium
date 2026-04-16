// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"

#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class UniqueFontSelectorTest : public PageTestBase {
 protected:
  static wtf_size_t CacheSizeOf(const UniqueFontSelector& selector) {
    return selector.font_cache_.size();
  }
};

TEST_F(UniqueFontSelectorTest, CacheSizeLimit) {
  auto* selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());
  FontDescription desc;
  const wtf_size_t max_size = CanvasFontCache::MaxFonts();

  // We get max_size * 3 different fonts.
  for (wtf_size_t size = 0; size < max_size * 3; ++size) {
    desc.SetComputedSize(size);
    selector->FindOrCreateFont(desc);
  }
  // The cache should have max_size * 2 entries, not max_size * 3.
  EXPECT_EQ(max_size * 2, CacheSizeOf(*selector));

  selector->DidSwitchFrame();
  // Get a new font after switching frame. The cache size should shrink
  // to max_size.
  desc.SetComputedSize(desc.ComputedSize() + 1);
  selector->FindOrCreateFont(desc);
  EXPECT_EQ(max_size, CacheSizeOf(*selector));
}

TEST_F(UniqueFontSelectorTest, StatefulMemoryPressure_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);

  base::TestMemoryConsumerRegistry test_registry;

  auto* selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());

  FontDescription desc;
  const wtf_size_t max_size = CanvasFontCache::MaxFonts();

  // Fill cache to max_size.
  for (wtf_size_t size = 0; size < max_size; ++size) {
    desc.SetComputedSize(size);
    selector->FindOrCreateFont(desc);
  }
  EXPECT_EQ(max_size, CacheSizeOf(*selector));

  // Update limit to 50%.
  {
    base::RunLoop run_loop;
    test_registry.NotifyUpdateMemoryLimitAsync(50, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Cache should NOT shrink immediately.
  EXPECT_EQ(max_size, CacheSizeOf(*selector));

  // Switch frame to make entries "old" and eligible for eviction.
  selector->DidSwitchFrame();

  // Release memory.
  {
    base::RunLoop run_loop;
    test_registry.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Cache should now shrink to 50% of max_size.
  EXPECT_EQ(max_size / 2, CacheSizeOf(*selector));
}

TEST_F(UniqueFontSelectorTest, StatefulMemoryPressure_Disabled_Critical) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  base::TestMemoryConsumerRegistry test_registry;

  auto* selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());

  FontDescription desc;
  const wtf_size_t max_size = CanvasFontCache::MaxFonts();

  // Fill cache.
  for (wtf_size_t size = 0; size < max_size; ++size) {
    desc.SetComputedSize(size);
    selector->FindOrCreateFont(desc);
  }
  EXPECT_EQ(max_size, CacheSizeOf(*selector));

  // Update limit to 0% (critical) and release memory.
  {
    base::RunLoop run_loop;
    test_registry.NotifyUpdateMemoryLimitAsync(0, base::DoNothing());
    test_registry.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Cache should be cleared completely for critical pressure when stateful is
  // disabled.
  EXPECT_EQ(0u, CacheSizeOf(*selector));
}

TEST_F(UniqueFontSelectorTest, StatefulMemoryPressure_Disabled_NonCritical) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  base::TestMemoryConsumerRegistry test_registry;

  auto* selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());

  FontDescription desc;
  const wtf_size_t max_size = CanvasFontCache::MaxFonts();

  // Fill cache.
  for (wtf_size_t size = 0; size < max_size; ++size) {
    desc.SetComputedSize(size);
    selector->FindOrCreateFont(desc);
  }
  EXPECT_EQ(max_size, CacheSizeOf(*selector));

  // Update limit to 80% (non-critical) and release memory.
  {
    base::RunLoop run_loop;
    test_registry.NotifyUpdateMemoryLimitAsync(80, base::DoNothing());
    test_registry.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Cache should NOT be cleared for non-critical pressure when stateful is
  // disabled.
  EXPECT_EQ(max_size, CacheSizeOf(*selector));
}

}  // namespace blink
