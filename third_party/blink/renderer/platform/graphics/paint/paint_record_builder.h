// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
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
  // Constructs a new builder for the resulting paint record. If |metadata|
  // is specified, that metadata is propagated to the builder's internal canvas.
  // If |containing_context| is specified, the device scale factor, printing,
  // and disabled state are propagated to the builder's internal context.
  // If a PaintController is passed, it is used as the PaintController for
  // painting the picture (and hence we can use its cache). Otherwise, a new
  // transient PaintController is used for the duration of the picture building,
  // which therefore has no caching. It also resets paint chunk state to
  // PropertyTreeState::Root() before beginning to record.
  // TODO(wangxianzhu): Remove the input PaintController feature for
  // CompositeAfterPaint.
  PaintRecordBuilder(printing::MetafileSkia* metafile = nullptr,
                     GraphicsContext* containing_context = nullptr,
                     PaintController* = nullptr,
                     paint_preview::PaintPreviewTracker* tracker = nullptr);
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

  // DisplayItemClient methods
  String DebugName() const final { return "PaintRecordBuilder"; }
  IntRect VisualRect() const final { return IntRect(); }

 private:
  PaintController* paint_controller_;
  std::unique_ptr<PaintController> own_paint_controller_;
  std::unique_ptr<GraphicsContext> context_;

  DISALLOW_COPY_AND_ASSIGN(PaintRecordBuilder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_RECORD_BUILDER_H_
