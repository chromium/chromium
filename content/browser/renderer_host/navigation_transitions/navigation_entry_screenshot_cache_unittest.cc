// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"

#include "base/test/scoped_feature_list.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static NavigationEntryScreenshotCache* GetCacheForTab(WebContents* tab) {
  return static_cast<NavigationControllerImpl*>(&(tab->GetController()))
      ->GetNavigationEntryScreenshotCache();
}

NavigationEntry* GetEntryWithID(WebContents* tab, int nav_entry_id) {
  auto* entry = static_cast<NavigationControllerImpl*>(&(tab->GetController()))
                    ->GetEntryWithUniqueID(nav_entry_id);
  EXPECT_NE(entry, nullptr);
  return entry;
}

void AssertEntryHasNoScreenshot(WebContents* tab, int nav_entry_id) {
  EXPECT_EQ(GetEntryWithID(tab, nav_entry_id)
                ->GetUserData(NavigationEntryScreenshot::kUserDataKey),
            nullptr);
}
}  // namespace

class NavigationEntryScreenshotCacheTest : public RenderViewHostTestHarness {
 public:
  NavigationEntryScreenshotCacheTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kBackForwardTransitions}, {});
  }
  ~NavigationEntryScreenshotCacheTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    tabs_[0] = RenderViewHostTestHarness::CreateTestWebContents();
    tabs_[1] = RenderViewHostTestHarness::CreateTestWebContents();
    tabs_[2] = RenderViewHostTestHarness::CreateTestWebContents();
  }

  void TearDown() override {
    // Remove all the `WebContents` first; otherwise `RenderWidgetHost`s would
    // be leaked.
    std::fill(tabs_.begin(), tabs_.end(), nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  // Returns a 4x4 immutable bitmap filled with `color`. `kN32_SkColorType` is
  // 4-bytes per pixel, making this bitmap 64 bytes.
  const SkBitmap GetBitmapOfColor(SkColor color) {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32(size_.height(), size_.width(),
                                            kOpaque_SkAlphaType));
    bitmap.eraseColor(color);
    bitmap.setImmutable();
    return bitmap;
  }

  // Cache a screenshot of `color` for entry of `entry_id` inside the
  // controller of `tab`.
  void CacheScreenshot(WebContents* tab, int entry_id, SkColor color) {
    const auto& bitmap = GetBitmapOfColor(color);
    auto* cache = GetCacheForTab(tab);
    cache->SetScreenshot(
        nullptr,
        std::make_unique<NavigationEntryScreenshot>(bitmap, entry_id, true),
        false);
  }

  std::unique_ptr<NavigationEntryScreenshot> GetScreenshot(WebContents* tab,
                                                           int entry_id) {
    auto* entry = GetEntryWithID(tab, entry_id);
    return GetCacheForTab(tab)->RemoveScreenshot(entry);
  }

  void AssertBitmapOfColor(
      std::unique_ptr<NavigationEntryScreenshot> screenshot,
      SkColor color) {
    ASSERT_EQ(screenshot->dimensions_without_compression(), size_);
    auto bitmap = screenshot->GetBitmapForTesting();
    int num_pixel_mismatch = 0;
    gfx::Rect err_bounding_box;
    for (int r = 0; r < size_.height(); ++r) {
      for (int c = 0; c < size_.width(); ++c) {
        if (bitmap.getColor(c, r) != color) {
          ++num_pixel_mismatch;
          err_bounding_box.Union(gfx::Rect(c, r, 1, 1));
        }
      }
    }
    if (num_pixel_mismatch != 0) {
      ASSERT_TRUE(false)
          << "Number of pixel mismatches: " << num_pixel_mismatch
          << "; error bounding box: " << err_bounding_box.ToString()
          << "; bitmap size: "
          << gfx::Size(bitmap.width(), bitmap.height()).ToString();
    }
  }

  // Asserts the entry of `nav_entry_id` for `tab` does not have a screenshot.
  void AssertEntryHasNoBitmap(WebContents* tab, int entry_id) {
    auto* entry = GetEntryWithID(tab, entry_id);
    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(entry->GetUserData(NavigationEntryScreenshot::kUserDataKey),
              nullptr);
  }

  void RemoveTestNavEntry(WebContents* tab, int entry_id) {
    const int idx =
        static_cast<NavigationControllerImpl*>(&(tab->GetController()))
            ->GetEntryIndexWithUniqueID(entry_id);
    ASSERT_NE(idx, -1);
    tab->GetController().RemoveEntryAtIndex(idx);
  }

  void RemoveTab(WebContents* tab) {
    bool found = false;
    for (std::unique_ptr<WebContents>& web_contents : tabs_) {
      if (web_contents.get() == tab) {
        web_contents = nullptr;
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }

  // `last_committed_index` is 0-indexed from the first entry in the navigation
  // controller: if we already have five entries in the controller and we call
  // Restore to append another five, meanwhile setting `last_committed_index` /
  // to 2, the last committed entry will be the third entry, but not the eighth
  // entry.
  void RestoreEntriesToTab(WebContents* tab,
                           int id_start,
                           int id_end,
                           int last_committed_index) {
    std::vector<std::unique_ptr<NavigationEntry>> entries;
    for (int i = id_start; i <= id_end; ++i) {
      entries.emplace_back(std::make_unique<NavigationEntryImpl>());
      static_cast<NavigationEntryImpl*>(entries.back().get())->set_unique_id(i);
    }
    tab->GetController().Restore(last_committed_index, RestoreType::kRestored,
                                 &entries);
  }

  WebContents* tab1() { return tabs_[0].get(); }
  WebContents* tab2() { return tabs_[1].get(); }
  WebContents* tab3() { return tabs_[2].get(); }

  NavigationEntryScreenshotManager* GetManager() {
    return BrowserContextImpl::From(browser_context())
        ->GetNavigationEntryScreenshotManager();
  }

 private:
  gfx::Size size_ = gfx::Size(4, 4);

  // Hold the test WebContents.
  std::array<std::unique_ptr<WebContents>, 3> tabs_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test the basic functionalities of `SetScreenshot`, `RemoveScreenshot` of
// `NavigationEntryScreenshotCache`.
TEST_F(NavigationEntryScreenshotCacheTest, GetRemoveBasic) {
  GetManager()->SetMemoryBudgetForTesting(10240U);
  ASSERT_TRUE(GetManager()->IsEmpty());

  // Restore entry1 to entry10 into the controller of tab1; entry11 to entry20
  // into the controller of tab2. The entries with screenshots are always a
  // subset of the entries owned by the controller.
  RestoreEntriesToTab(tab1(), /*id_start=*/1, /*id_end=*/10,
                      /*last_committed_index=*/9);
  RestoreEntriesToTab(tab2(), /*id_start=*/11, /*id_end=*/20,
                      /*last_committed_index=*/9);

  // Set:
  // Tab1: entry1->Red, entry2->Green, entry3->Blue;
  // Tab2: entry11->Black, entry12->White;
  CacheScreenshot(tab1(), 1, SK_ColorRED);
  ASSERT_FALSE(GetManager()->IsEmpty());
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 1);
  CacheScreenshot(tab1(), 2, SK_ColorGREEN);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);
  CacheScreenshot(tab1(), 3, SK_ColorBLUE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);
  CacheScreenshot(tab2(), 11, SK_ColorBLACK);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 4);
  CacheScreenshot(tab2(), 12, SK_ColorWHITE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 5);

  // Get (remove from the cache), in an arbitrary order.
  AssertBitmapOfColor(GetScreenshot(tab1(), 3), SK_ColorBLUE);
  AssertEntryHasNoBitmap(tab1(), 3);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 4);
  AssertBitmapOfColor(GetScreenshot(tab2(), 11), SK_ColorBLACK);
  AssertEntryHasNoBitmap(tab2(), 11);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);
  AssertBitmapOfColor(GetScreenshot(tab1(), 2), SK_ColorGREEN);
  AssertEntryHasNoBitmap(tab1(), 2);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);
  AssertBitmapOfColor(GetScreenshot(tab2(), 12), SK_ColorWHITE);
  AssertEntryHasNoBitmap(tab2(), 12);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 1);
  AssertBitmapOfColor(GetScreenshot(tab1(), 1), SK_ColorRED);
  AssertEntryHasNoBitmap(tab1(), 1);
  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetCacheForTab(tab2())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());

  // Re-setting/getting should work.
  CacheScreenshot(tab1(), 1, SK_ColorCYAN);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 1);
  CacheScreenshot(tab2(), 11, SK_ColorDKGRAY);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);
  CacheScreenshot(tab1(), 2, SK_ColorMAGENTA);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);

  AssertBitmapOfColor(GetScreenshot(tab1(), 1), SK_ColorCYAN);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);
  AssertBitmapOfColor(GetScreenshot(tab2(), 11), SK_ColorDKGRAY);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 1);
  AssertBitmapOfColor(GetScreenshot(tab1(), 2), SK_ColorMAGENTA);
  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetCacheForTab(tab2())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// Test Get/Set when the `NavigationEntry` is deleted. When the navigation entry
// is deleted, we must clear the cached screenshot for the entry.
TEST_F(NavigationEntryScreenshotCacheTest, DeletedNavEntry) {
  RestoreEntriesToTab(tab1(), /*id_start=*/1, /*id_end=*/10,
                      /*last_committed_index=*/9);
  RestoreEntriesToTab(tab2(), /*id_start=*/11, /*id_end=*/20,
                      /*last_committed_index=*/9);

  GetManager()->SetMemoryBudgetForTesting(10240U);

  // Set:
  // Tab1: entry2->Red, entry4->Green, entry5->Blue;
  // Tab2: entry15->Black, entry20->White;
  CacheScreenshot(tab1(), 2, SK_ColorRED);
  CacheScreenshot(tab1(), 4, SK_ColorGREEN);
  CacheScreenshot(tab1(), 5, SK_ColorBLUE);
  CacheScreenshot(tab2(), 15, SK_ColorBLACK);
  CacheScreenshot(tab2(), 19, SK_ColorWHITE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 5);

  // Remove the entry4->Green from tab1, entry20->Black from tab2.
  RemoveTestNavEntry(tab1(), 4);
  RemoveTestNavEntry(tab2(), 15);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);

  // Remove a navigation entry without a screenshot. We should not expect change
  // in size.
  RemoveTestNavEntry(tab1(), 1);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);

  // Get:
  AssertBitmapOfColor(GetScreenshot(tab1(), 2), SK_ColorRED);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);
  AssertBitmapOfColor(GetScreenshot(tab1(), 5), SK_ColorBLUE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 1);
  AssertBitmapOfColor(GetScreenshot(tab2(), 19), SK_ColorWHITE);
  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetCacheForTab(tab2())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// Test that screenshot eviction works in basic one-tab case.
TEST_F(NavigationEntryScreenshotCacheTest, CacheEvictionOneTab) {
  const auto cache_size = 128U;
  // In this test, we only have budget for two screenshots (each is 64 bytes).
  GetManager()->SetMemoryBudgetForTesting(cache_size);

  // Now manually insert Entry1 to Entry6 into NavigationController for Tab1.
  // Entry6 is used as the last committed entry.
  //
  // In this initial setup:
  // Controller: [1&, 2&, 3, 4, 5, 6*]; "&": nav entry with a screenshot; "*":
  // last committed nav entry.
  //
  RestoreEntriesToTab(tab1(), /*id_start=*/1, /*id_end=*/6,
                      /*last_committed_index=*/5);

  // Set:
  // Tab1: entry1->Red, entry2->Green;
  CacheScreenshot(tab1(), 1, SK_ColorRED);
  CacheScreenshot(tab1(), 2, SK_ColorGREEN);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);

  // Cache one more screenshot for tab1. We should expect entry1->Red for tab1
  // gets evicted. Tab1: entry2->Green, entry3->Blue.
  CacheScreenshot(tab1(), 3, SK_ColorBLUE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab1(), 1);

  // Evicts entry2->Green. Tab1: entry3->Blue, entry4->Black.
  CacheScreenshot(tab1(), 4, SK_ColorBLACK);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab1(), 2);

  // Evicts entry3->Blue. Tab1: entry4->Black, entry5->White.
  CacheScreenshot(tab1(), 5, SK_ColorWHITE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab1(), 2);
  AssertEntryHasNoScreenshot(tab1(), 3);

  // Get the screenshots from tab1 and free up space for tab2.
  AssertBitmapOfColor(GetScreenshot(tab1(), 4), SK_ColorBLACK);
  AssertBitmapOfColor(GetScreenshot(tab1(), 5), SK_ColorWHITE);
  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());

  // Restore Entry1 to Entry9 into the controller of tab2. Entry7 is the last
  // committed entry.
  //
  // Controller: [1, 2, 3, 4&, 5&, 6, 7*, 8, 9]; "&": nav entry with a
  // screenshot; "*": last committed nav entry.
  RestoreEntriesToTab(tab2(), /*id_start=*/1, /*id_end=*/9,
                      /*last_committed_index=*/6);
  CacheScreenshot(tab2(), 4, SK_ColorBLACK);
  CacheScreenshot(tab2(), 5, SK_ColorWHITE);

  // Entry4 is evicted. Tab2: entry5->White, entry8->Dkgray.
  CacheScreenshot(tab2(), 8, SK_ColorDKGRAY);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab2(), 4);

  // Entry9 is evicted immediately because it is the farthest from the last
  // committed entry. Entry7 is equally distant to both entry5 and entry9; tie
  // breaking favors eviction from the "forward end" of the entries.
  // Tab2: entry5->White, entry8->Dkgray.
  CacheScreenshot(tab2(), 9, SK_ColorMAGENTA);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab2(), 4);
  AssertEntryHasNoScreenshot(tab2(), 9);

  AssertBitmapOfColor(GetScreenshot(tab2(), 5), SK_ColorWHITE);
  AssertBitmapOfColor(GetScreenshot(tab2(), 8), SK_ColorDKGRAY);

  ASSERT_TRUE(GetCacheForTab(tab2())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// Specifically testing the iterative implementation of
// `NavigationEntryScreenshotCache::EvictScreenshotsUntilInBudgetOrEmpty`:
// In previous tests, only one screenshot is evicted at a time; this test
// asserts the all but the nearest screenshots are evicted.
TEST_F(NavigationEntryScreenshotCacheTest, MultipleCacheEvictionsOneTab) {
  GetManager()->SetMemoryBudgetForTesting(10240U);

  // Setup: (&: nav entry with a screenshot; *: last committed entry)
  // Controller: [1, 2&, 3&, 4, 5&, 6, 7&, 8&, 9*, 10, 11, 12&, 13&, 14, 15&]
  // Distance:   [8  7   6   5  4   3  2   1   0   1   2   3    4    5   6]
  //
  RestoreEntriesToTab(tab1(), 1, 15, 8);
  ASSERT_EQ(tab1()->GetController().GetLastCommittedEntry()->GetUniqueID(), 9);

  CacheScreenshot(tab1(), 2, SK_ColorRED);
  CacheScreenshot(tab1(), 3, SK_ColorBLUE);
  CacheScreenshot(tab1(), 5, SK_ColorGREEN);
  CacheScreenshot(tab1(), 7, SK_ColorWHITE);
  CacheScreenshot(tab1(), 8, SK_ColorBLACK);
  CacheScreenshot(tab1(), 12, SK_ColorGRAY);
  CacheScreenshot(tab1(), 13, SK_ColorDKGRAY);
  // We kick off the eviction by reducing the memory budget, and cache another
  // screenshot.
  GetManager()->SetMemoryBudgetForTesting(64U * 3);
  CacheScreenshot(tab1(), 15, SK_ColorCYAN);

  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 3);
  // All evicted except 7, 8 and 12.
  AssertBitmapOfColor(GetScreenshot(tab1(), 7), SK_ColorWHITE);
  AssertBitmapOfColor(GetScreenshot(tab1(), 8), SK_ColorBLACK);
  AssertBitmapOfColor(GetScreenshot(tab1(), 12), SK_ColorGRAY);
  AssertEntryHasNoScreenshot(tab1(), 2);
  AssertEntryHasNoScreenshot(tab1(), 3);
  AssertEntryHasNoScreenshot(tab1(), 5);
  AssertEntryHasNoScreenshot(tab1(), 13);
  AssertEntryHasNoScreenshot(tab1(), 15);

  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// Test that cached screenshot eviction works with multiple tabs: we always
// evict screenshots from the least recently used tabs. (Caching a screenshot
// affects which tab is LRU.)
TEST_F(NavigationEntryScreenshotCacheTest,
       AlwaysEvictFromLeastRecentlyUsedTabs) {
  RestoreEntriesToTab(tab1(), 1, 10, 9);
  RestoreEntriesToTab(tab2(), 11, 20, 9);

  const auto cache_size = 256U;
  GetManager()->SetMemoryBudgetForTesting(cache_size);

  // Tab1: entry1->Red, entry2->Green, entry3->Blue.
  // Tab2: entry11->Black.
  CacheScreenshot(tab1(), 1, SK_ColorRED);
  CacheScreenshot(tab1(), 2, SK_ColorGREEN);
  CacheScreenshot(tab1(), 3, SK_ColorBLUE);
  CacheScreenshot(tab2(), 11, SK_ColorBLACK);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);

  // Tab1: entry1->Red, entry2->Green, entry3->Blue, entry5->Gray.
  // Tab2: empty (entry4 evicted).  <- LRU.
  CacheScreenshot(tab1(), 5, SK_ColorGRAY);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab2(), 11);

  // Tab1: entry2->Green, entry3->Blue, entry5->Gray, entry6->White.  <- LRU.
  // Tab2: not tracked as a cache.
  CacheScreenshot(tab1(), 6, SK_ColorWHITE);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab2(), 11);

  // Tab1: entry3->Blue, entry5->Gray, entry6->White.  <- LRU.
  // Tab2: entry12->Cyan.
  CacheScreenshot(tab2(), 12, SK_ColorCYAN);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab1(), 2);
  AssertEntryHasNoScreenshot(tab2(), 11);

  // Tab1: entry5->Gray, entry6->White.  <- LRU.
  // Tab2: entry12->Cyan, entry13->Magenta.
  CacheScreenshot(tab2(), 13, SK_ColorMAGENTA);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), cache_size);
  AssertBitmapOfColor(GetScreenshot(tab1(), 5), SK_ColorGRAY);
  AssertBitmapOfColor(GetScreenshot(tab1(), 6), SK_ColorWHITE);
  AssertBitmapOfColor(GetScreenshot(tab2(), 12), SK_ColorCYAN);
  AssertBitmapOfColor(GetScreenshot(tab2(), 13), SK_ColorMAGENTA);
  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab1(), 2);
  AssertEntryHasNoScreenshot(tab1(), 3);
  AssertEntryHasNoScreenshot(tab2(), 11);

  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetCacheForTab(tab2())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// Test that when the WebContents is destroyed, all the screenshots for that tab
// are deleted.
TEST_F(NavigationEntryScreenshotCacheTest, OnWebContentsDestroyed) {
  GetManager()->SetMemoryBudgetForTesting(10240U);

  // Restore entry1/2 into tab1; entry3/4 into tab2; entry5/6 into tab3.
  RestoreEntriesToTab(tab1(), 1, 3, 2);
  RestoreEntriesToTab(tab2(), 3, 5, 2);
  RestoreEntriesToTab(tab3(), 5, 7, 2);

  // Tab1: entry1->Red, entry2->Green.
  // Tab2: entry3->Blue, entry4->Black.
  // Tab3: entry5->White, entry6->Gray.
  CacheScreenshot(tab1(), 1, SK_ColorRED);
  CacheScreenshot(tab1(), 2, SK_ColorGREEN);
  CacheScreenshot(tab2(), 3, SK_ColorBLUE);
  CacheScreenshot(tab2(), 4, SK_ColorBLACK);
  CacheScreenshot(tab3(), 5, SK_ColorWHITE);
  CacheScreenshot(tab3(), 6, SK_ColorGRAY);
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 6);

  RemoveTab(tab2());
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 4);

  RemoveTab(tab3());
  ASSERT_EQ(GetManager()->GetCurrentCacheSize(), 64U * 2);

  RemoveTab(tab1());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

// `base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL` signals the
// purge of all the cached screenshots within the global manager's Profile. This
// test asserts that.
TEST_F(NavigationEntryScreenshotCacheTest, OnMemoryPressureCritical) {
  GetManager()->SetMemoryBudgetForTesting(10240U);

  RestoreEntriesToTab(tab1(), 1, 10, 9);

  CacheScreenshot(tab1(), 1, SK_ColorRED);
  CacheScreenshot(tab1(), 2, SK_ColorGREEN);
  CacheScreenshot(tab1(), 3, SK_ColorBLUE);
  CacheScreenshot(tab1(), 4, SK_ColorBLACK);
  CacheScreenshot(tab1(), 5, SK_ColorWHITE);
  CacheScreenshot(tab1(), 6, SK_ColorGRAY);

  GetManager()->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  AssertEntryHasNoScreenshot(tab1(), 1);
  AssertEntryHasNoScreenshot(tab1(), 2);
  AssertEntryHasNoScreenshot(tab1(), 3);
  AssertEntryHasNoScreenshot(tab1(), 4);
  AssertEntryHasNoScreenshot(tab1(), 5);
  AssertEntryHasNoScreenshot(tab1(), 6);

  ASSERT_TRUE(GetCacheForTab(tab1())->IsEmpty());
  ASSERT_TRUE(GetManager()->IsEmpty());
}

}  // namespace content
