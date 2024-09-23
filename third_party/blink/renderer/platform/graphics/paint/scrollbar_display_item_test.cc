// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class ScrollbarDisplayItemTest : public testing::Test {
 protected:
  static CompositorElementId ScrollbarElementId(
      const cc::Scrollbar& scrollbar) {
    return CompositorElementIdFromUniqueObjectId(
        13579, scrollbar.Orientation() == cc::ScrollbarOrientation::kHorizontal
                   ? CompositorElementIdNamespace::kHorizontalScrollbar
                   : CompositorElementIdNamespace::kVerticalScrollbar);
  }

  static CompositorElementId ScrollElementId(const PropertyTreeState& state) {
    return state.Transform().ScrollNode()->GetCompositorElementId();
  }

  static PropertyTreeState CreateScrollState() {
    return CreateScrollTranslationState(PropertyTreeState::Root(), 0, 0,
                                        gfx::Rect(0, 0, 100, 100),
                                        gfx::Size(1000, 1000));
  }
};

TEST_F(ScrollbarDisplayItemTest, HorizontalSolidColorScrollbar) {
  auto scroll_state = CreateScrollState();
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_orientation(cc::ScrollbarOrientation::kHorizontal);
  scrollbar->set_is_solid_color(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_track_rect(gfx::Rect(2, 90, 96, 10));
  scrollbar->set_thumb_size(gfx::Size(30, 7));

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(0, 90, 100, 10);
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar, scrollbar_rect,
      &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kTransparent,
      client.VisualRectOutsetForRasterEffects());
  auto layer = display_item.CreateOrReuseLayer(nullptr, gfx::Vector2dF(10, 20));
  ASSERT_EQ(cc::ScrollbarLayerBase::kSolidColor,
            layer->GetScrollbarLayerType());
  EXPECT_EQ(cc::HitTestOpaqueness::kTransparent, layer->hit_test_opaqueness());

  auto* scrollbar_layer =
      static_cast<cc::SolidColorScrollbarLayer*>(layer.get());
  EXPECT_EQ(gfx::Size(100, 10), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(10, 110),
            scrollbar_layer->offset_to_transform_parent());
  EXPECT_EQ(cc::ScrollbarOrientation::kHorizontal,
            scrollbar_layer->orientation());
  EXPECT_EQ(7, scrollbar_layer->thumb_thickness());
  EXPECT_EQ(2, scrollbar_layer->track_start());
  EXPECT_EQ(element_id, scrollbar_layer->element_id());
  EXPECT_EQ(ScrollElementId(scroll_state),
            scrollbar_layer->scroll_element_id());

  EXPECT_EQ(layer, display_item.CreateOrReuseLayer(layer.get(),
                                                   gfx::Vector2dF(30, 40)));
  EXPECT_EQ(gfx::Size(100, 10), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(30, 130),
            scrollbar_layer->offset_to_transform_parent());
}

TEST_F(ScrollbarDisplayItemTest, VerticalSolidColorScrollbar) {
  auto scroll_state = CreateScrollState();
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_orientation(cc::ScrollbarOrientation::kVertical);
  scrollbar->set_is_solid_color(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_track_rect(gfx::Rect(90, 2, 10, 96));
  scrollbar->set_thumb_size(gfx::Size(7, 30));

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(90, 0, 10, 100);
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar, scrollbar_rect,
      &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kTransparent,
      client.VisualRectOutsetForRasterEffects());
  auto layer = display_item.CreateOrReuseLayer(nullptr, gfx::Vector2dF(10, 20));
  ASSERT_EQ(cc::ScrollbarLayerBase::kSolidColor,
            layer->GetScrollbarLayerType());
  EXPECT_EQ(cc::HitTestOpaqueness::kTransparent, layer->hit_test_opaqueness());

  auto* scrollbar_layer =
      static_cast<cc::SolidColorScrollbarLayer*>(layer.get());
  EXPECT_EQ(gfx::Size(10, 100), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(100, 20),
            scrollbar_layer->offset_to_transform_parent());
  EXPECT_EQ(cc::ScrollbarOrientation::kVertical,
            scrollbar_layer->orientation());
  EXPECT_EQ(7, scrollbar_layer->thumb_thickness());
  EXPECT_EQ(2, scrollbar_layer->track_start());
  EXPECT_EQ(element_id, scrollbar_layer->element_id());
  EXPECT_EQ(ScrollElementId(scroll_state),
            scrollbar_layer->scroll_element_id());

  EXPECT_EQ(layer, display_item.CreateOrReuseLayer(layer.get(),
                                                   gfx::Vector2dF(30, 40)));
  EXPECT_EQ(gfx::Size(10, 100), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(120, 40),
            scrollbar_layer->offset_to_transform_parent());
}

TEST_F(ScrollbarDisplayItemTest, PaintedScrollbar) {
  auto scroll_state = CreateScrollState();
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(0, 90, 100, 10);
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar, scrollbar_rect,
      &scroll_state.Transform(), element_id, cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  auto layer = display_item.CreateOrReuseLayer(nullptr, gfx::Vector2dF(10, 20));
  EXPECT_EQ(gfx::Size(100, 10), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(10, 110), layer->offset_to_transform_parent());
  ASSERT_EQ(cc::ScrollbarLayerBase::kPainted, layer->GetScrollbarLayerType());
  EXPECT_EQ(cc::HitTestOpaqueness::kOpaque, layer->hit_test_opaqueness());

  EXPECT_EQ(layer, display_item.CreateOrReuseLayer(layer.get(),
                                                   gfx::Vector2dF(30, 40)));
  EXPECT_EQ(gfx::Size(100, 10), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(30, 130), layer->offset_to_transform_parent());
}

TEST_F(ScrollbarDisplayItemTest, PaintedScrollbarOverlayNonNinePatch) {
  auto scroll_state = CreateScrollState();
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_has_thumb(true);
  scrollbar->set_is_overlay(true);

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(0, 90, 100, 10);
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar, scrollbar_rect,
      &scroll_state.Transform(), element_id, cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  auto layer = display_item.CreateOrReuseLayer(nullptr, gfx::Vector2dF());
  // We should create PaintedScrollbarLayer instead of
  // NinePatchThumbScrollbarLayer for non-nine-patch overlay scrollbars.
  ASSERT_EQ(cc::ScrollbarLayerBase::kPainted, layer->GetScrollbarLayerType());
  EXPECT_EQ(cc::HitTestOpaqueness::kOpaque, layer->hit_test_opaqueness());

  EXPECT_EQ(layer,
            display_item.CreateOrReuseLayer(layer.get(), gfx::Vector2dF()));
}

TEST_F(ScrollbarDisplayItemTest, PaintedScrollbarOverlayNinePatch) {
  auto scroll_state = CreateScrollState();
  auto scrollbar = base::MakeRefCounted<cc::FakeScrollbar>();
  scrollbar->set_has_thumb(true);
  scrollbar->set_is_overlay(true);
  scrollbar->set_uses_nine_patch_thumb_resource(true);

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(0, 90, 100, 10);
  auto element_id = ScrollbarElementId(*scrollbar);
  ScrollbarDisplayItem display_item(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar, scrollbar_rect,
      &scroll_state.Transform(), element_id, cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  auto layer = display_item.CreateOrReuseLayer(nullptr, gfx::Vector2dF());
  ASSERT_EQ(cc::ScrollbarLayerBase::kNinePatchThumb,
            layer->GetScrollbarLayerType());
  EXPECT_EQ(cc::HitTestOpaqueness::kOpaque, layer->hit_test_opaqueness());

  EXPECT_EQ(layer,
            display_item.CreateOrReuseLayer(layer.get(), gfx::Vector2dF()));
}

TEST_F(ScrollbarDisplayItemTest, CreateOrReuseLayer) {
  auto scroll_state = CreateScrollState();
  auto scrollbar1 = base::MakeRefCounted<cc::FakeScrollbar>();

  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  gfx::Rect scrollbar_rect(0, 90, 100, 10);
  auto element_id = ScrollbarElementId(*scrollbar1);
  ScrollbarDisplayItem display_item1a(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar1,
      scrollbar_rect, &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  auto layer1 =
      display_item1a.CreateOrReuseLayer(nullptr, gfx::Vector2dF(10, 20));
  EXPECT_EQ(gfx::Size(100, 10), layer1->bounds());
  EXPECT_EQ(gfx::Vector2dF(10, 110), layer1->offset_to_transform_parent());

  ScrollbarDisplayItem display_item1b(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar1,
      scrollbar_rect, &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  // Should reuse layer for a different display item and the same scrollbar.
  EXPECT_EQ(layer1, display_item1b.CreateOrReuseLayer(layer1.get(),
                                                      gfx::Vector2dF(30, 40)));
  EXPECT_EQ(gfx::Size(100, 10), layer1->bounds());
  EXPECT_EQ(gfx::Vector2dF(30, 130), layer1->offset_to_transform_parent());

  auto scrollbar2 = base::MakeRefCounted<cc::FakeScrollbar>();
  ScrollbarDisplayItem display_item2(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar2,
      scrollbar_rect, &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  // Should create new layer for a different scrollbar.
  EXPECT_NE(layer1,
            display_item2.CreateOrReuseLayer(layer1.get(), gfx::Vector2dF()));

  ASSERT_FALSE(scrollbar1->IsLeftSideVerticalScrollbar());
  scrollbar1->set_is_left_side_vertical_scrollbar(true);
  // Should create new layer for changed is_left_side_vertical_scrollbar of
  // the same scrollbar.
  EXPECT_NE(layer1,
            display_item1a.CreateOrReuseLayer(layer1.get(), gfx::Vector2dF()));

  ScrollbarDisplayItem display_item1c(
      client.Id(), DisplayItem::kScrollbarHorizontal, scrollbar1,
      scrollbar_rect, &scroll_state.Transform(), element_id,
      cc::HitTestOpaqueness::kOpaque,
      client.VisualRectOutsetForRasterEffects());
  // Should reuse layer for a different display item and the same scrollbar.
  EXPECT_NE(layer1,
            display_item1b.CreateOrReuseLayer(layer1.get(), gfx::Vector2dF()));
}

}  // namespace blink
