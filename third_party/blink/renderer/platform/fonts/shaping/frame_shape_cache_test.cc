// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/frame_shape_cache.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class FrameShapeCacheTest : public FontTestBase {
 protected:
  void SetUp() override { cache_ = MakeGarbageCollected<FrameShapeCache>(); }

  PlainTextNode* CreateNode(const String& text, TextDirection dir) {
    return MakeGarbageCollected<PlainTextNode>(
        TextRun(text, dir), /* normalize_space */ false,
        *test::CreateAhemFont(10), /* supports_bidi */ true, cache_);
  }

  PlainTextItem CreateItem(ShapeResult* shape_result,
                           const gfx::RectF& ink_bounds,
                           const String& text,
                           TextDirection dir) {
    PlainTextItem item(0, text.length(), dir, text);
    item.shape_result_ = shape_result;
    item.ink_bounds_ = ink_bounds;
    return item;
  }

  ShapeResult* CreateDummyShapeResult(TextDirection dir) {
    return MakeGarbageCollected<ShapeResult>(0, 0, dir);
  }

  void AddJunksAndSwitchFrame() {
    auto* entry = cache_->FindOrCreateNodeEntry("i", kLtr);
    PlainTextNode* node = CreateNode("i", kLtr);
    cache_->RegisterNodeEntry("i", kLtr, node, entry);

    auto* shape_entry = cache_->FindOrCreateShapeEntry("i", kLtr);
    ShapeResult* shape_result = MakeGarbageCollected<ShapeResult>(0, 0, kLtr);
    PlainTextItem item =
        CreateItem(shape_result, gfx::RectF(0, 0, 10, 10), "i", kLtr);
    cache_->RegisterShapeEntry(item, shape_entry);

    cache_->DidSwitchFrame();
  }

  Persistent<FrameShapeCache> cache_;

  static constexpr TextDirection kLtr = TextDirection::kLtr;
  static constexpr TextDirection kRtl = TextDirection::kRtl;
};

TEST_F(FrameShapeCacheTest, FindOrCreateNodeEntry) {
  // Adding an entry is successful.
  String text_a = "A";
  auto* entry_a = cache_->FindOrCreateNodeEntry(text_a, kLtr);
  ASSERT_TRUE(entry_a);
  EXPECT_FALSE(entry_a->node);
  PlainTextNode* node_a = CreateNode(text_a, kLtr);
  cache_->RegisterNodeEntry(text_a, kLtr, node_a, entry_a);
  EXPECT_EQ(entry_a->node, node_a);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache_->FindOrCreateNodeEntry(text_a, kLtr), entry_a);

  // Adding an entry with different text does not hit cache.
  String text_b = "B";
  auto* entry_b = cache_->FindOrCreateNodeEntry(text_b, kLtr);
  ASSERT_TRUE(entry_b);
  PlainTextNode* node_b = CreateNode(text_b, kLtr);
  cache_->RegisterNodeEntry(text_b, kLtr, node_b, entry_b);
  EXPECT_EQ(entry_b->node, node_b);
  EXPECT_NE(entry_b, entry_a);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache_->FindOrCreateNodeEntry(text_b, kLtr), entry_b);

  // Adding an entry with different direction does not hit cache.
  auto* entry_a_rtl = cache_->FindOrCreateNodeEntry(text_a, kRtl);
  ASSERT_TRUE(entry_a_rtl);
  EXPECT_FALSE(entry_a_rtl->node);
  PlainTextNode* node_a_rtl = CreateNode(text_a, kRtl);
  cache_->RegisterNodeEntry(text_a, kRtl, node_a_rtl, entry_a_rtl);
  EXPECT_EQ(entry_a_rtl->node, node_a_rtl);
  EXPECT_NE(entry_a_rtl, entry_a);
  EXPECT_NE(entry_a_rtl, entry_b);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache_->FindOrCreateNodeEntry(text_a, kRtl), entry_a_rtl);
}

TEST_F(FrameShapeCacheTest, FindOrCreateShapeEntry) {
  // Adding an entry is successful.
  String text_a = "A";
  auto* entry_a = cache_->FindOrCreateShapeEntry(text_a, kLtr);
  ASSERT_TRUE(entry_a);
  EXPECT_FALSE(entry_a->shape_result);
  ShapeResult* shape_result_a = CreateDummyShapeResult(kLtr);
  PlainTextItem item_a =
      CreateItem(shape_result_a, gfx::RectF(0, 0, 10, 10), text_a, kLtr);
  cache_->RegisterShapeEntry(item_a, entry_a);
  EXPECT_EQ(entry_a->shape_result, shape_result_a);
  EXPECT_EQ(entry_a->ink_bounds, gfx::RectF(0, 0, 10, 10));

  // Adding the same entry again hits cache.
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_a, kLtr)->shape_result);

  // Adding an entry with different text does not hit cache.
  String text_b = "B";
  auto* entry_b = cache_->FindOrCreateShapeEntry(text_b, kLtr);
  ASSERT_TRUE(entry_b);
  EXPECT_FALSE(entry_b->shape_result);
  ShapeResult* shape_result_b = CreateDummyShapeResult(kLtr);
  PlainTextItem item_b =
      CreateItem(shape_result_b, gfx::RectF(0, 0, 20, 20), text_b, kLtr);
  cache_->RegisterShapeEntry(item_b, entry_b);
  EXPECT_EQ(entry_b->shape_result, shape_result_b);
  EXPECT_EQ(entry_b->ink_bounds, gfx::RectF(0, 0, 20, 20));
  EXPECT_NE(entry_b, entry_a);

  // Adding the same entry again hits cache.
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_b, kLtr)->shape_result);

  // Adding an entry with different direction does not hit cache.
  auto* entry_a_rtl = cache_->FindOrCreateShapeEntry(text_a, kRtl);
  ASSERT_TRUE(entry_a_rtl);
  EXPECT_FALSE(entry_a_rtl->shape_result);
  ShapeResult* shape_result_a_rtl = CreateDummyShapeResult(kRtl);
  PlainTextItem item_a_rtl =
      CreateItem(shape_result_a_rtl, gfx::RectF(0, 0, 30, 30), text_a, kRtl);
  cache_->RegisterShapeEntry(item_a_rtl, entry_a_rtl);
  EXPECT_EQ(entry_a_rtl->shape_result, shape_result_a_rtl);
  EXPECT_EQ(entry_a_rtl->ink_bounds, gfx::RectF(0, 0, 30, 30));
  EXPECT_NE(entry_a_rtl, entry_a);
  EXPECT_NE(entry_a_rtl, entry_b);

  // Adding the same entry again hits cache.
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_a, kRtl)->shape_result);
}

TEST_F(FrameShapeCacheTest, DidSwitchFramePurgesOldEntries) {
  // Update the cache for the initial frame.
  AddJunksAndSwitchFrame();

  // Add some entries.
  String text_a = "A";
  String text_b = "B";
  {
    auto* entry_a = cache_->FindOrCreateNodeEntry(text_a, kLtr);
    PlainTextNode* node_a = CreateNode(text_a, kLtr);
    cache_->RegisterNodeEntry(text_a, kLtr, node_a, entry_a);

    auto* entry_b = cache_->FindOrCreateNodeEntry(text_b, kLtr);
    PlainTextNode* node_b = CreateNode(text_b, kLtr);
    cache_->RegisterNodeEntry(text_b, kLtr, node_b, entry_b);

    auto* shape_entry_a = cache_->FindOrCreateShapeEntry(text_a, kLtr);
    PlainTextItem item_a = CreateItem(CreateDummyShapeResult(kLtr),
                                      gfx::RectF(0, 0, 10, 10), text_a, kLtr);
    cache_->RegisterShapeEntry(item_a, shape_entry_a);

    auto* shape_entry_b = cache_->FindOrCreateShapeEntry(text_b, kLtr);
    PlainTextItem item_b = CreateItem(CreateDummyShapeResult(kLtr),
                                      gfx::RectF(0, 0, 20, 20), text_b, kLtr);
    cache_->RegisterShapeEntry(item_b, shape_entry_b);
  }

  // Switch frame.
  cache_->DidSwitchFrame();

  {
    // Touch only entries for "B".
    cache_->FindOrCreateNodeEntry(text_b, kLtr);
    cache_->FindOrCreateShapeEntry(text_b, kLtr);

    // Add something to kick purging in the next DidSwitchFrame().
    String text_c = "C";
    auto* node_entry = cache_->FindOrCreateNodeEntry(text_c, kLtr);
    PlainTextNode* node = CreateNode(text_c, kLtr);
    cache_->RegisterNodeEntry(text_c, kLtr, node, node_entry);

    auto* shape_entry = cache_->FindOrCreateShapeEntry(text_c, kLtr);
    PlainTextItem item = CreateItem(CreateDummyShapeResult(kLtr),
                                    gfx::RectF(0, 0, 10, 10), text_c, kLtr);
    cache_->RegisterShapeEntry(item, shape_entry);
  }

  // Switch frame.
  cache_->DidSwitchFrame();

  // Untouched entries created in the previous frame should be purged.
  EXPECT_FALSE(cache_->FindOrCreateNodeEntry(text_a, kLtr)->node);
  EXPECT_FALSE(cache_->FindOrCreateShapeEntry(text_a, kLtr)->shape_result);
  // Touched entries should be present.
  EXPECT_TRUE(cache_->FindOrCreateNodeEntry(text_b, kLtr)->node);
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_b, kLtr)->shape_result);
}

TEST_F(FrameShapeCacheTest, DidSwitchFrameDoesNotPurgeInitialFrameEntries) {
  // Add some entries.
  String text_a = "A";

  auto* entry_a = cache_->FindOrCreateNodeEntry(text_a, kLtr);
  PlainTextNode* node_a = CreateNode(text_a, kLtr);
  cache_->RegisterNodeEntry(text_a, kLtr, node_a, entry_a);

  auto* shape_entry_a = cache_->FindOrCreateShapeEntry(text_a, kLtr);
  PlainTextItem item_a = CreateItem(CreateDummyShapeResult(kLtr),
                                    gfx::RectF(0, 0, 10, 10), text_a, kLtr);
  cache_->RegisterShapeEntry(item_a, shape_entry_a);

  cache_->DidSwitchFrame();

  {
    // Add something to kick purging in the next DidSwitchFrame().
    String text_c = "C";
    auto* node_entry = cache_->FindOrCreateNodeEntry(text_c, kLtr);
    PlainTextNode* node = CreateNode(text_c, kLtr);
    cache_->RegisterNodeEntry(text_c, kLtr, node, node_entry);

    auto* shape_entry = cache_->FindOrCreateShapeEntry(text_c, kLtr);
    PlainTextItem item = CreateItem(CreateDummyShapeResult(kLtr),
                                    gfx::RectF(0, 0, 10, 10), text_c, kLtr);
    cache_->RegisterShapeEntry(item, shape_entry);
  }
  cache_->DidSwitchFrame();

  // Entries from the initial frame should still be present.
  EXPECT_TRUE(cache_->FindOrCreateNodeEntry(text_a, kLtr)->node);
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_a, kLtr)->shape_result);
}

TEST_F(FrameShapeCacheTest, TouchShapeResultCacheEntries) {
  // Update the cache for the initial frame.
  AddJunksAndSwitchFrame();

  // Add a node entry.
  String text_node = "AB";
  auto* node_entry = cache_->FindOrCreateNodeEntry(text_node, kLtr);
  PlainTextNode* node = CreateNode(text_node, kLtr);
  cache_->RegisterNodeEntry(text_node, kLtr, node, node_entry);
  // The shape cache should have an entry for "AB".
  ASSERT_TRUE(cache_->FindOrCreateShapeEntry(text_node, kLtr)->shape_result);

  cache_->DidSwitchFrame();

  // Write something to the ShapeResult cache.
  {
    String another = "C";
    auto* shape_entry = cache_->FindOrCreateShapeEntry(another, kLtr);
    PlainTextItem item = CreateItem(CreateDummyShapeResult(kLtr),
                                    gfx::RectF(0, 0, 10, 10), another, kLtr);
    cache_->RegisterShapeEntry(item, shape_entry);
  }
  // Touch the existing PlainTextNode.
  // It should touch associated ShapeResult cache entries.
  ASSERT_TRUE(cache_->FindOrCreateNodeEntry(text_node, kLtr)->node);

  cache_->DidSwitchFrame();

  // The entry for "AB" should survive even though it was not used in
  // the previous frame.
  EXPECT_TRUE(cache_->FindOrCreateShapeEntry(text_node, kLtr)->shape_result);
}

}  // namespace blink
