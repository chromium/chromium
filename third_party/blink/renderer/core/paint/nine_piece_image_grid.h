// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_GRID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class IntRectOutsets;

enum NinePiece {
  kMinPiece = 0,
  kTopLeftPiece = kMinPiece,
  kBottomLeftPiece,
  kLeftPiece,
  kTopRightPiece,
  kBottomRightPiece,
  kRightPiece,
  kTopPiece,
  kBottomPiece,
  kMiddlePiece,
  kMaxPiece
};

inline NinePiece& operator++(NinePiece& piece) {
  piece = static_cast<NinePiece>(static_cast<int>(piece) + 1);
  return piece;
}

// The NinePieceImageGrid class is responsible for computing drawing information
// for the nine piece image.
//
// https://drafts.csswg.org/css-backgrounds/#border-image-process
//
// Given an image, a set of slices and a border area:
//
//       |         |
//   +---+---------+---+          +------------------+
//   | 1 |    7    | 4 |          |      border      |
// --+---+---------+---+---       |  +------------+  |
//   |   |         |   |          |  |            |  |
//   | 3 |    9    | 6 |          |  |    css     |  |
//   |   |  image  |   |          |  |    box     |  |
//   |   |         |   |          |  |            |  |
// --+---+---------+---+---       |  |            |  |
//   | 2 |    8    | 5 |          |  +------------+  |
//   +---+---------+---+          |                  |
//       |         |              +------------------+
//
// it generates drawing information for the nine border pieces.
class CORE_EXPORT NinePieceImageGrid {
  STACK_ALLOCATED();

 public:
  NinePieceImageGrid(const NinePieceImage&,
                     FloatSize image_size,
                     const FloatSize& slice_scale,
                     float zoom,
                     IntRect border_image_area,
                     const IntRectOutsets& border_widths,
                     PhysicalBoxSides sides_to_include = PhysicalBoxSides());

  struct CORE_EXPORT NinePieceDrawInfo {
    STACK_ALLOCATED();

   public:
    bool is_drawable;
    bool is_corner_piece;
    FloatRect destination;
    FloatRect source;

    // tileScale and tileRule are only useful for non-corners, i.e. edge and
    // center pieces.
    FloatSize tile_scale;
    struct {
      ENinePieceImageRule horizontal;
      ENinePieceImageRule vertical;
    } tile_rule;
  };
  NinePieceDrawInfo GetNinePieceDrawInfo(NinePiece) const;

  struct Edge {
    DISALLOW_NEW();
    bool IsDrawable() const { return slice > 0 && width > 0; }
    float Scale() const { return IsDrawable() ? width / slice : 1; }
    float slice;
    int width;
  };

 private:
  void SetDrawInfoCorner(NinePieceDrawInfo&, NinePiece) const;
  void SetDrawInfoEdge(NinePieceDrawInfo&, NinePiece) const;
  void SetDrawInfoMiddle(NinePieceDrawInfo&) const;

  IntRect border_image_area_;
  FloatSize image_size_;
  ENinePieceImageRule horizontal_tile_rule_;
  ENinePieceImageRule vertical_tile_rule_;
  bool fill_;

  Edge top_;
  Edge right_;
  Edge bottom_;
  Edge left_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NINE_PIECE_IMAGE_GRID_H_
