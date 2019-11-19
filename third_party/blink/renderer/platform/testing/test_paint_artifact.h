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

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class FloatRect;
class PaintArtifact;
class TransformPaintPropertyNode;

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
//
class TestPaintArtifact {
  STACK_ALLOCATED();

 public:
  TestPaintArtifact();
  ~TestPaintArtifact();

  // Add a chunk to the artifact. Each chunk will have a different automatically
  // created client.
  TestPaintArtifact& Chunk() { return Chunk(NewClient()); }

  // Add a chunk with the specified client.
  TestPaintArtifact& Chunk(FakeDisplayItemClient&,
                           DisplayItem::Type = DisplayItem::kDrawingFirst);

  // This is for RasterInvalidatorTest, to create a chunk with specific id and
  // bounds calculated with a function from the id. The client is static so
  // the caller doesn't need to retain this object when using the paint
  // artifact.
  TestPaintArtifact& Chunk(int id);

  TestPaintArtifact& Properties(const PropertyTreeState&);
  TestPaintArtifact& Properties(const TransformPaintPropertyNode& transform,
                                const ClipPaintPropertyNode& clip,
                                const EffectPaintPropertyNode& effect) {
    return Properties(PropertyTreeState(transform, clip, effect));
  }
  TestPaintArtifact& Properties(const RefCountedPropertyTreeState& properties) {
    return Properties(properties.GetPropertyTreeState());
  }

  // Shorthands of Chunk().Properties(...).
  TestPaintArtifact& Chunk(const TransformPaintPropertyNode& transform,
                           const ClipPaintPropertyNode& clip,
                           const EffectPaintPropertyNode& effect) {
    return Chunk().Properties(transform, clip, effect);
  }
  TestPaintArtifact& Chunk(const PropertyTreeState& properties) {
    return Chunk().Properties(properties);
  }
  TestPaintArtifact& Chunk(const RefCountedPropertyTreeState& properties) {
    return Chunk().Properties(properties);
  }

  // Add display item in the chunk. Each display item will have a different
  // automatically created client.
  TestPaintArtifact& RectDrawing(const FloatRect& bounds, Color color);
  TestPaintArtifact& ScrollHitTest(
      const TransformPaintPropertyNode* scroll_offset,
      const IntRect& scroll_container_bounds);

  TestPaintArtifact& ForeignLayer(scoped_refptr<cc::Layer> layer,
                                  const FloatPoint& offset);

  // Add display item with the specified client in the chunk.
  TestPaintArtifact& RectDrawing(FakeDisplayItemClient&,
                                 const FloatRect& bounds,
                                 Color);
  TestPaintArtifact& ScrollHitTest(
      FakeDisplayItemClient&,
      const TransformPaintPropertyNode* scroll_offset,
      const IntRect& scroll_container_bounds);

  // Sets fake bounds for the last paint chunk. Note that the bounds will be
  // overwritten when the PaintArtifact is constructed if the chunk has any
  // display items.
  TestPaintArtifact& Bounds(const IntRect&);

  TestPaintArtifact& OutsetForRasterEffects(float);
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
  class DummyRectClient;
  Vector<std::unique_ptr<DummyRectClient>> dummy_clients_;

  DisplayItemList display_item_list_;
  Vector<PaintChunk> paint_chunks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TEST_PAINT_ARTIFACT_H_
