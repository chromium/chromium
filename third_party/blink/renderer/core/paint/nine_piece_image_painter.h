// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_PAINTER_H_

#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class ImageResourceObserver;
class Node;
class NinePieceImage;
class Document;
struct PhysicalRect;

class NinePieceImagePainter {
  STACK_ALLOCATED();

 public:
  static bool Paint(GraphicsContext&,
                    const ImageResourceObserver&,
                    const Document&,
                    Node*,
                    const PhysicalRect&,
                    const ComputedStyle&,
                    const NinePieceImage&,
                    bool include_logical_left_edge = true,
                    bool include_logical_right_edge = true);

 private:
  NinePieceImagePainter() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_PAINTER_H_
