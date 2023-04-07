// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_VIEW_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_VIEW_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gfx {
class Rect;
}

namespace blink {

struct PaintInfo;
class DisplayItemClient;
class Document;
class LayoutView;
class PropertyTreeStateOrAlias;

class ViewPainter {
  STACK_ALLOCATED();

 public:
  ViewPainter(const LayoutView& layout_view) : layout_view_(layout_view) {}

  void PaintBoxDecorationBackground(const PaintInfo&);

 private:
  const LayoutView& layout_view_;

  void PaintRootElementGroup(
      const PaintInfo&,
      const gfx::Rect& pixel_snapped_background_rect,
      const PropertyTreeStateOrAlias& background_paint_state,
      const DisplayItemClient& background_client,
      bool painted_separate_backdrop,
      bool painted_separate_effect);

  void PaintRootGroup(const PaintInfo& paint_info,
                      const gfx::Rect& pixel_snapped_background_rect,
                      const Document&,
                      const DisplayItemClient& background_client,
                      const PropertyTreeStateOrAlias& state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_VIEW_PAINTER_H_
