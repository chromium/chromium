// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TEST_PAINT_ARTIFACT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TEST_PAINT_ARTIFACT_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class Layer;
}

namespace blink {

class ClipPaintPropertyNodeOrAlias;
class EffectPaintPropertyNodeOrAlias;
class PaintArtifact;
class TransformPaintPropertyNode;
class TransformPaintPropertyNodeOrAlias;

// Useful for quickly making a paint artifact in unit tests.
//
// If any method that automatically creates display item client is called, the
// object must remain in scope while the paint artifact is used, because it owns
// the display item clients.
// Usage:
//   TestPaintArtifact test_artifact;
//   test_artifact.Chunk().Properties(paint_properties)
//       .RectDrawing(bounds, color)
//       .RectDrawing(bounds2, color2);
//   test_artifact.Chunk().Properties(other_paint_properties)
//       .RectDrawing(bounds3, color3);
//   auto artifact = test_artifact.Build();
//   DoSomethingWithArtifact(artifact);
//
// Otherwise the TestPaintArtifact object can be temporary.
// Usage:
//   auto artifact = TestPaintArtifact().Chunk(0).Chunk(1).Build();
//   DoSomethingWithArtifact(artifact);
//  or
//   DoSomethingWithArtifact(TestPaintArtifact().Chunk(0).Chunk(1).Build());
//
class TestPaintArtifact {
  STACK_ALLOCATED();

 public:
  // Add a chunk to the artifact. Each chunk will have a different automatically
  // created client.
  TestPaintArtifact& Chunk() { return Chunk(NewClient()); }

  // Add a chunk with the specified client.
  TestPaintArtifact& Chunk(DisplayItemClient&,
                           DisplayItem::Type = DisplayItem::kDrawingFirst);

  // This is for RasterInvalidatorTest, to create a chunk with specific id and
  // bounds calculated with a function from the id. The client is static so
  // the caller doesn't need to retain this object when using the paint
  // artifact.
  TestPaintArtifact& Chunk(int id);

  TestPaintArtifact& Properties(const PropertyTreeStateOrAlias&);
  TestPaintArtifact& Properties(
      const TransformPaintPropertyNodeOrAlias& transform,
      const ClipPaintPropertyNodeOrAlias& clip,
      const EffectPaintPropertyNodeOrAlias& effect) {
    return Properties(PropertyTreeStateOrAlias(transform, clip, effect));
  }
  TestPaintArtifact& Properties(const RefCountedPropertyTreeState& properties) {
    return Properties(properties.GetPropertyTreeState());
  }

  // Shorthands of Chunk().Properties(...).
  TestPaintArtifact& Chunk(const TransformPaintPropertyNodeOrAlias& transform,
                           const ClipPaintPropertyNodeOrAlias& clip,
                           const EffectPaintPropertyNodeOrAlias& effect) {
    return Chunk().Properties(transform, clip, effect);
  }
  TestPaintArtifact& Chunk(const PropertyTreeStateOrAlias& properties) {
    return Chunk().Properties(properties);
  }
  TestPaintArtifact& Chunk(const RefCountedPropertyTreeState& properties) {
    return Chunk().Properties(properties);
  }

  // Add display item in the chunk. Each display item will have a different
  // automatically created client.
  TestPaintArtifact& RectDrawing(const IntRect& bounds, Color color);
  TestPaintArtifact& ScrollHitTest(
      const IntRect&,
      const TransformPaintPropertyNode* scroll_translation);

  TestPaintArtifact& ForeignLayer(scoped_refptr<cc::Layer> layer,
                                  const IntPoint& offset);

  // Add display item with the specified client in the chunk.
  TestPaintArtifact& RectDrawing(DisplayItemClient&,
                                 const IntRect& bounds,
                                 Color color);
  TestPaintArtifact& ScrollHitTest(
      DisplayItemClient&,
      const IntRect&,
      const TransformPaintPropertyNode* scroll_translation);

  // Sets fake bounds for the last paint chunk. Note that the bounds will be
  // overwritten when the PaintArtifact is constructed if the chunk has any
  // display items. Bounds() sets both bounds and drawable_bounds, while
  // DrawableBounds() sets drawable_bounds only.
  TestPaintArtifact& Bounds(const IntRect&);
  TestPaintArtifact& DrawableBounds(const IntRect&);

  TestPaintArtifact& SetRasterEffectOutset(RasterEffectOutset);
  TestPaintArtifact& KnownToBeOpaque();
  TestPaintArtifact& Uncacheable();

  // Build the paint artifact. After that, if this object has automatically
  // created any display item client, the caller must retain this object when
  // using the returned paint artifact.
  scoped_refptr<PaintArtifact> Build();

  // Create a new display item client which is owned by this TestPaintArtifact.
  FakeDisplayItemClient& NewClient();

  FakeDisplayItemClient& Client(wtf_size_t) const;

 private:
  void DidAddDisplayItem();

  Vector<std::unique_ptr<FakeDisplayItemClient>> clients_;
  scoped_refptr<PaintArtifact> paint_artifact_ =
      base::MakeRefCounted<PaintArtifact>();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TEST_PAINT_ARTIFACT_H_
