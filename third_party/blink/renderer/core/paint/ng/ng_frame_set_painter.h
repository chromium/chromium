// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FRAME_SET_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FRAME_SET_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DisplayItemClient;
class NGPhysicalBoxFragment;
struct PaintInfo;
struct PhysicalOffset;

class NGFrameSetPainter {
  STACK_ALLOCATED();

 public:
  NGFrameSetPainter(const NGPhysicalBoxFragment& box_fragment,
                    const DisplayItemClient& display_item_client)
      : box_fragment_(box_fragment),
        display_item_client_(display_item_client) {}
  void PaintObject(const PaintInfo&, const PhysicalOffset&);

 private:
  void PaintChildren(const PaintInfo& paint_info);
  void PaintBorders(const PaintInfo& paint_info,
                    const PhysicalOffset& paint_offset);

  const NGPhysicalBoxFragment& box_fragment_;
  [[maybe_unused]] const DisplayItemClient& display_item_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FRAME_SET_PAINTER_H_
