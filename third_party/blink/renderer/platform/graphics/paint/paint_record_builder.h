// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class GraphicsContext;
class PaintController;

class PLATFORM_EXPORT PaintRecordBuilder final : public DisplayItemClient {
 public:
  // Constructs a new builder for the resulting paint record. A transient
  // PaintController is created and will be used for the duration of the picture
  // building, which therefore has no caching. It also resets paint chunk state
  // to PropertyTreeState::Root() before beginning to record.
  PaintRecordBuilder();

  // Same as PaintRecordBulder() except that the properties of
  // |containing_context| such as device scale factor, printing, etc. are
  // propagated to this builder's internal context.
  explicit PaintRecordBuilder(GraphicsContext& containing_context);

  // The input PaintController will be used for painting the picture (and hence
  // we can use its cache).
  explicit PaintRecordBuilder(PaintController&);

  ~PaintRecordBuilder() override;

  GraphicsContext& Context() { return *context_; }

  // Returns a PaintRecord capturing all drawing performed on the builder's
  // context since construction, into the ancestor state given by
  // |replay_state|.
  sk_sp<PaintRecord> EndRecording(
      const PropertyTreeState& replay_state = PropertyTreeState::Root());

  // Replays the recording directly into the given canvas, in the ancestor
  // state given by |replay_state|.
  void EndRecording(
      cc::PaintCanvas&,
      const PropertyTreeState& replay_state = PropertyTreeState::Root());

  // DisplayItemClient.
  String DebugName() const final { return "PaintRecordBuilder"; }

 private:
  std::unique_ptr<PaintController> own_paint_controller_;
  PaintController* paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_
