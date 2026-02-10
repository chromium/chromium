// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_

#include "third_party/blink/renderer/core/frame/post_layout_snapshot_client.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class PaintLayerScrollableArea;

class TextOverflowPostLayoutSnapshot final
    : public GarbageCollected<TextOverflowPostLayoutSnapshot>,
      public PostLayoutSnapshotClient {
 public:
  explicit TextOverflowPostLayoutSnapshot(PaintLayerScrollableArea& scroller);

  bool UpdateSnapshot() override;
  bool ShouldScheduleNextService() override;

  bool IsScrolled() const;

  void Trace(Visitor* visitor) const override;

 private:
  Member<PaintLayerScrollableArea> scroller_;
  bool was_scrolled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_OVERFLOW_POST_LAYOUT_SNAPSHOT_H_
