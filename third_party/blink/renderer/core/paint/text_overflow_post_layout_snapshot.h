// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_

#include "third_party/blink/renderer/core/frame/post_layout_snapshot_client.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class PaintLayerScrollableArea;

// text-overflow:ellipsis is only rendered if the scroll offset in the inline
// direction is 0. The scroll offset is snapshot at "run snapshot post-layout
// state steps" time to make sure we read the offsets from a clean tree and not
// during layout.
class TextOverflowPostLayoutSnapshot final
    : public GarbageCollected<TextOverflowPostLayoutSnapshot>,
      public PostLayoutSnapshotClient {
 public:
  explicit TextOverflowPostLayoutSnapshot(PaintLayerScrollableArea& scroller);

  bool UpdateSnapshot() override;
  bool ShouldScheduleNextService() override;

  bool IsScrolled() const { return is_scrolled_; }

  void Trace(Visitor* visitor) const override;

 private:
  bool ComputeIsScrolled() const;

  Member<PaintLayerScrollableArea> scroller_;
  bool is_scrolled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_
