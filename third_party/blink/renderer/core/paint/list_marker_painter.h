// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LIST_MARKER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LIST_MARKER_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class ComputedStyle;
class IntRect;
class LayoutListMarker;
class LayoutObject;

class ListMarkerPainter {
  STACK_ALLOCATED();

 public:
  ListMarkerPainter(const LayoutListMarker& layout_list_marker)
      : layout_list_marker_(layout_list_marker) {}

  void Paint(const PaintInfo&);

  static void PaintSymbol(const PaintInfo&,
                          const LayoutObject*,
                          const ComputedStyle&,
                          const IntRect&);

 private:
  const LayoutListMarker& layout_list_marker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LIST_MARKER_PAINTER_H_
