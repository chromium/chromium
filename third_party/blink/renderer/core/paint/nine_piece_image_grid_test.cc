// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "ui/gfx/geometry/outsets.h"

namespace blink {
namespace {

class NinePieceImageGridTest : public RenderingTest {
 public:
  NinePieceImageGridTest() = default;

  StyleImage* GeneratedImage() {
    auto* gradient = MakeGarbageCollected<cssvalue::

                                              CSSLinearGradientValue>(
        nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);
    return MakeGarbageCollected<StyleGeneratedImage>(
        *gradient, StyleGeneratedImage::ContainerSizes());
  }
};

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoDrawables) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());

  gfx::SizeF image_size(100, 100);
  gfx::Rect border_image_area(0, 0, 100, 100);
  gfx::Outsets border_widths(0);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_FALSE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_AllDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(true);

  gfx::SizeF image_size(100, 100);
  gfx::Rect border_image_area(0, 0, 100, 100);
  gfx::Outsets border_widths(10);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoFillMiddleNotDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(false);  // default

  gfx::SizeF image_size(100, 100);
  gfx::Rect border_image_area(0, 0, 100, 100);
  gfx::Outsets border_widths(10);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    if (piece != kMiddlePiece)
      EXPECT_TRUE(draw_info.is_drawable);
    else
      EXPECT_FALSE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_EmptySidesNotDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(Length::Percent(49), Length::Percent(49),
                                      Length::Percent(49),
                                      Length::Percent(49)));

  gfx::SizeF image_size(6, 6);
  gfx::Rect border_image_area(0, 0, 6, 6);
  gfx::Outsets border_widths(3);

  NinePieceImageGrid grid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                          border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    auto draw_info = grid.GetNinePieceDrawInfo(piece);
    if (piece == kLeftPiece || piece == kRightPiece || piece == kTopPiece ||
        piece == kBottomPiece || piece == kMiddlePiece)
      EXPECT_FALSE(draw_info.is_drawable);
    else
      EXPECT_TRUE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_TopLeftDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));

  gfx::SizeF image_size(100, 100);
  gfx::Rect border_image_area(0, 0, 100, 100);

  const struct {
    gfx::Outsets border_widths;
    bool expected_is_drawable;
  } test_cases[] = {
      {gfx::Outsets(), false},
      {gfx::Outsets().set_top(10), false},
      {gfx::Outsets().set_left(10), false},
      {gfx::Outsets().set_top(10).set_left(10), true},
  };

  for (const auto& test_case : test_cases) {
    NinePieceImageGrid grid =
        NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                           border_image_area, test_case.border_widths);
    for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
      NinePieceImageGrid::NinePieceDrawInfo draw_info =
          grid.GetNinePieceDrawInfo(piece);
      if (piece == kTopLeftPiece)
        EXPECT_EQ(draw_info.is_drawable, test_case.expected_is_drawable);
    }
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_ScaleDownBorder) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));

  gfx::SizeF image_size(100, 100);
  gfx::Rect border_image_area(0, 0, 100, 100);
  gfx::Outsets border_widths(10);

  // Set border slices wide enough so that the widths are scaled
  // down and corner pieces cover the entire border image area.
  nine_piece.SetBorderSlices(BorderImageLengthBox(6));

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    if (draw_info.is_corner_piece)
      EXPECT_EQ(draw_info.destination.size(), gfx::SizeF(50, 50));
    else
      EXPECT_TRUE(draw_info.destination.size().IsEmpty());
  }

  // Like above, but also make sure to get a scale-down factor that requires
  // rounding to pick the larger value on one of the edges. (A 1:3, 2:3 split.)
  BorderImageLength top_left(10);
  BorderImageLength bottom_right(20);
  nine_piece.SetBorderSlices(
      BorderImageLengthBox(top_left, bottom_right, bottom_right, top_left));
  grid = NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                            border_image_area, border_widths);
  NinePieceImageGrid::NinePieceDrawInfo draw_info =
      grid.GetNinePieceDrawInfo(kTopLeftPiece);
  EXPECT_EQ(draw_info.destination.size(), gfx::SizeF(33, 33));
  draw_info = grid.GetNinePieceDrawInfo(kTopRightPiece);
  EXPECT_EQ(draw_info.destination.size(), gfx::SizeF(67, 33));
  draw_info = grid.GetNinePieceDrawInfo(kBottomLeftPiece);
  EXPECT_EQ(draw_info.destination.size(), gfx::SizeF(33, 67));
  draw_info = grid.GetNinePieceDrawInfo(kBottomRightPiece);
  EXPECT_EQ(draw_info.destination.size(), gfx::SizeF(67, 67));

  // Set border slices that overlap in one dimension but not in the other, and
  // where the resulting width in the non-overlapping dimension will round to a
  // larger width.
  BorderImageLength top_bottom(10);
  BorderImageLength left_right(Length::Fixed(11));
  nine_piece.SetBorderSlices(
      BorderImageLengthBox(top_bottom, left_right, top_bottom, left_right));
  grid = NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                            border_image_area, border_widths);
  NinePieceImageGrid::NinePieceDrawInfo tl_info =
      grid.GetNinePieceDrawInfo(kTopLeftPiece);
  EXPECT_EQ(tl_info.destination.size(), gfx::SizeF(5, 50));
  // The top-right, bottom-left and bottom-right pieces are the same size as
  // the top-left piece.
  draw_info = grid.GetNinePieceDrawInfo(kTopRightPiece);
  EXPECT_EQ(tl_info.destination.size(), draw_info.destination.size());
  draw_info = grid.GetNinePieceDrawInfo(kBottomLeftPiece);
  EXPECT_EQ(tl_info.destination.size(), draw_info.destination.size());
  draw_info = grid.GetNinePieceDrawInfo(kBottomRightPiece);
  EXPECT_EQ(tl_info.destination.size(), draw_info.destination.size());
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_AbuttingEdges) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(
      LengthBox(Length::Percent(56.1f), Length::Percent(12.5f),
                Length::Percent(43.9f), Length::Percent(37.5f)));
  BorderImageLength auto_width(Length::Auto());
  nine_piece.SetBorderSlices(
      BorderImageLengthBox(auto_width, auto_width, auto_width, auto_width));

  const gfx::SizeF image_size(200, 35);
  const gfx::Rect border_image_area(0, 0, 250, 35);
  const int kExpectedTileWidth = border_image_area.width() -
                                 0.125f * image_size.width() -
                                 0.375f * image_size.width();
  const gfx::Outsets border_widths(0);
  const NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, gfx::Vector2dF(1, 1), 1,
                         border_image_area, border_widths);

  const NinePieceImageGrid::NinePieceDrawInfo top_info =
      grid.GetNinePieceDrawInfo(kTopPiece);
  EXPECT_EQ(top_info.destination.size(), gfx::SizeF(kExpectedTileWidth, 20));

  const NinePieceImageGrid::NinePieceDrawInfo middle_info =
      grid.GetNinePieceDrawInfo(kMiddlePiece);
  EXPECT_FALSE(middle_info.is_drawable);

  const NinePieceImageGrid::NinePieceDrawInfo bottom_info =
      grid.GetNinePieceDrawInfo(kBottomPiece);
  EXPECT_EQ(bottom_info.destination.size(), gfx::SizeF(kExpectedTileWidth, 15));
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting) {
  const struct {
    gfx::SizeF image_size;
    gfx::Rect border_image_area;
    gfx::Outsets border_widths;
    bool fill;
    LengthBox image_slices;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
    struct Piece {
      bool is_drawable;
      bool is_corner_piece;
      gfx::RectF destination;
      gfx::RectF source;
      float tile_scale_horizontal;
      float tile_scale_vertical;
      ENinePieceImageRule horizontal_rule;
      ENinePieceImageRule vertical_rule;
    };
    std::array<Piece, 9> pieces;
  } test_cases[] = {
      {// Empty border and slices but with fill
       gfx::SizeF(100, 100),
       gfx::Rect(0, 0, 100, 100),
       gfx::Outsets(0),
       true,
       LengthBox(Length::Fixed(0), Length::Fixed(0), Length::Fixed(0),
                 Length::Fixed(0)),
       kStretchImageRule,
       kStretchImageRule,
       {{
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(0, 0, 100, 100), gfx::RectF(0, 0, 100, 100),
            1, 1, kStretchImageRule, kStretchImageRule},
       }}},
      {// Single border and fill
       gfx::SizeF(100, 100),
       gfx::Rect(0, 0, 100, 100),
       gfx::Outsets().set_bottom(10),
       true,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kStretchImageRule,
       {{
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(0, 90, 100, 10), gfx::RectF(20, 80, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(0, 0, 100, 90), gfx::RectF(20, 20, 60, 60),
            1.666667, 1.5, kStretchImageRule, kStretchImageRule},
       }}},
      {// All borders, no fill
       gfx::SizeF(100, 100),
       gfx::Rect(0, 0, 100, 100),
       gfx::Outsets(10),
       false,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kStretchImageRule,
       {{
           {true, true, gfx::RectF(0, 0, 10, 10), gfx::RectF(0, 0, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, true, gfx::RectF(0, 90, 10, 10), gfx::RectF(0, 80, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(0, 10, 10, 80), gfx::RectF(0, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, true, gfx::RectF(90, 0, 10, 10), gfx::RectF(80, 0, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, true, gfx::RectF(90, 90, 10, 10), gfx::RectF(80, 80, 20, 20),
            1, 1, kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(90, 10, 10, 80), gfx::RectF(80, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(10, 0, 80, 10), gfx::RectF(20, 0, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(10, 90, 80, 10), gfx::RectF(20, 80, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
       }}},
      {// Single border, no fill
       gfx::SizeF(100, 100),
       gfx::Rect(0, 0, 100, 100),
       gfx::Outsets().set_left(10),
       false,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kRoundImageRule,
       {{
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {true, false, gfx::RectF(0, 0, 10, 100), gfx::RectF(0, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kRoundImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
       }}},
      {// All borders but no slices, with fill (stretch horizontally, space
       // vertically)
       gfx::SizeF(100, 100),
       gfx::Rect(0, 0, 100, 100),
       gfx::Outsets(10),
       true,
       LengthBox(Length::Fixed(0), Length::Fixed(0), Length::Fixed(0),
                 Length::Fixed(0)),
       kStretchImageRule,
       kSpaceImageRule,
       {{
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, false, gfx::RectF(0, 0, 0, 0), gfx::RectF(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {true, false, gfx::RectF(10, 10, 80, 80), gfx::RectF(0, 0, 100, 100),
            0.800000, 1, kStretchImageRule, kSpaceImageRule},
       }}},
  };

  for (auto& test_case : test_cases) {
    NinePieceImage nine_piece;
    nine_piece.SetImage(GeneratedImage());
    nine_piece.SetFill(test_case.fill);
    nine_piece.SetImageSlices(test_case.image_slices);
    nine_piece.SetHorizontalRule(
        (ENinePieceImageRule)test_case.horizontal_rule);
    nine_piece.SetVerticalRule((ENinePieceImageRule)test_case.vertical_rule);

    NinePieceImageGrid grid = NinePieceImageGrid(
        nine_piece, test_case.image_size, gfx::Vector2dF(1, 1), 1,
        test_case.border_image_area, test_case.border_widths);
    for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
      NinePieceImageGrid::NinePieceDrawInfo draw_info =
          grid.GetNinePieceDrawInfo(piece);
      EXPECT_EQ(test_case.pieces[piece].is_drawable, draw_info.is_drawable);
      if (!test_case.pieces[piece].is_drawable)
        continue;

      EXPECT_EQ(test_case.pieces[piece].destination.x(),
                draw_info.destination.x());
      EXPECT_EQ(test_case.pieces[piece].destination.y(),
                draw_info.destination.y());
      EXPECT_EQ(test_case.pieces[piece].destination.width(),
                draw_info.destination.width());
      EXPECT_EQ(test_case.pieces[piece].destination.height(),
                draw_info.destination.height());
      EXPECT_EQ(test_case.pieces[piece].source.x(), draw_info.source.x());
      EXPECT_EQ(test_case.pieces[piece].source.y(), draw_info.source.y());
      EXPECT_EQ(test_case.pieces[piece].source.width(),
                draw_info.source.width());
      EXPECT_EQ(test_case.pieces[piece].source.height(),
                draw_info.source.height());

      if (test_case.pieces[piece].is_corner_piece)
        continue;

      EXPECT_FLOAT_EQ(test_case.pieces[piece].tile_scale_horizontal,
                      draw_info.tile_scale.x());
      EXPECT_FLOAT_EQ(test_case.pieces[piece].tile_scale_vertical,
                      draw_info.tile_scale.y());
      EXPECT_EQ(test_case.pieces[piece].horizontal_rule,
                draw_info.tile_rule.horizontal);
      EXPECT_EQ(test_case.pieces[piece].vertical_rule,
                draw_info.tile_rule.vertical);
    }
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_Zoomed) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  // Image slices are specified in CSS pixels.
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(true);

  gfx::SizeF image_size(50, 50);
  gfx::Rect border_image_area(0, 0, 200, 200);
  gfx::Outsets border_widths(20);

  NinePieceImageGrid grid(nine_piece, image_size, gfx::Vector2dF(2, 2), 2,
                          border_image_area, border_widths);

  struct ExpectedPiece {
    bool is_drawable;
    bool is_corner_piece;
    gfx::RectF destination;
    gfx::RectF source;
    float tile_scale_horizontal;
    float tile_scale_vertical;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
  };
  std::array<ExpectedPiece, kMaxPiece> expected_pieces = {{
      {true, true, gfx::RectF(0, 0, 20, 20), gfx::RectF(0, 0, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, gfx::RectF(0, 180, 20, 20), gfx::RectF(0, 30, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(0, 20, 20, 160), gfx::RectF(0, 20, 20, 10), 1, 1,
       kStretchImageRule, kStretchImageRule},
      {true, true, gfx::RectF(180, 0, 20, 20), gfx::RectF(30, 0, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, gfx::RectF(180, 180, 20, 20), gfx::RectF(30, 30, 20, 20), 0,
       0, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(180, 20, 20, 160), gfx::RectF(30, 20, 20, 10), 1,
       1, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(20, 0, 160, 20), gfx::RectF(20, 0, 10, 20), 1, 1,
       kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(20, 180, 160, 20), gfx::RectF(20, 30, 10, 20), 1,
       1, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(20, 20, 160, 160), gfx::RectF(20, 20, 10, 10),
       16, 16, kStretchImageRule, kStretchImageRule},
  }};

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);

    const auto& expected = expected_pieces[piece];
    EXPECT_EQ(draw_info.destination, expected.destination);
    EXPECT_EQ(draw_info.source, expected.source);

    if (expected.is_corner_piece)
      continue;

    EXPECT_FLOAT_EQ(draw_info.tile_scale.x(), expected.tile_scale_horizontal);
    EXPECT_FLOAT_EQ(draw_info.tile_scale.y(), expected.tile_scale_vertical);
    EXPECT_EQ(draw_info.tile_rule.vertical, expected.vertical_rule);
    EXPECT_EQ(draw_info.tile_rule.horizontal, expected.horizontal_rule);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_ZoomedNarrowSlices) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  // Image slices are specified in CSS pixels.
  nine_piece.SetImageSlices(LengthBox(1, 1, 1, 1));
  nine_piece.SetFill(true);

  constexpr float zoom = 2.2f;
  const gfx::SizeF image_size(3 * zoom, 3 * zoom);
  const gfx::Rect border_image_area(0, 0, 220, 220);
  const gfx::Outsets border_widths(33);

  const float kSliceWidth = 2.203125f;  // 2.2f rounded to nearest LayoutUnit
  const float kSliceMiddleWidth =
      image_size.width() - kSliceWidth - kSliceWidth;
  // Relative locations of the "inside" of a certain edge.
  const float kSliceTop = kSliceWidth;
  const float kSliceRight = image_size.width() - kSliceWidth;
  const float kSliceBottom = image_size.height() - kSliceWidth;
  const float kSliceLeft = kSliceWidth;

  const float kTileScaleX = border_widths.left() / kSliceWidth;
  const float kTileScaleY = border_widths.top() / kSliceWidth;
  const float kTileMiddleScale =
      (border_image_area.width() - border_widths.left() -
       border_widths.right()) /
      kSliceMiddleWidth;

  NinePieceImageGrid grid(nine_piece, image_size, gfx::Vector2dF(zoom, zoom),
                          zoom, border_image_area, border_widths);

  struct ExpectedPiece {
    bool is_drawable;
    bool is_corner_piece;
    gfx::RectF destination;
    gfx::RectF source;
    float tile_scale_horizontal;
    float tile_scale_vertical;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
  };
  std::array<ExpectedPiece, kMaxPiece> expected_pieces = {{
      {true, true, gfx::RectF(0, 0, 33, 33),
       gfx::RectF(0, 0, kSliceWidth, kSliceWidth), 0, 0, kStretchImageRule,
       kStretchImageRule},
      {true, true, gfx::RectF(0, 187, 33, 33),
       gfx::RectF(0, kSliceBottom, kSliceWidth, kSliceWidth), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(0, 33, 33, 154),
       gfx::RectF(0, kSliceTop, kSliceWidth, kSliceMiddleWidth), kTileScaleX,
       kTileScaleY, kStretchImageRule, kStretchImageRule},
      {true, true, gfx::RectF(187, 0, 33, 33),
       gfx::RectF(kSliceRight, 0, kSliceWidth, kSliceWidth), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, gfx::RectF(187, 187, 33, 33),
       gfx::RectF(kSliceRight, kSliceBottom, kSliceWidth, kSliceWidth), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(187, 33, 33, 154),
       gfx::RectF(kSliceRight, kSliceTop, kSliceWidth, kSliceMiddleWidth),
       kTileScaleX, kTileScaleY, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(33, 0, 154, 33),
       gfx::RectF(kSliceLeft, 0, kSliceMiddleWidth, kSliceWidth), kTileScaleX,
       kTileScaleY, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(33, 187, 154, 33),
       gfx::RectF(kSliceLeft, kSliceBottom, kSliceMiddleWidth, kSliceWidth),
       kTileScaleX, kTileScaleY, kStretchImageRule, kStretchImageRule},
      {true, false, gfx::RectF(33, 33, 154, 154),
       gfx::RectF(kSliceLeft, kSliceTop, kSliceMiddleWidth, kSliceMiddleWidth),
       kTileMiddleScale, kTileMiddleScale, kStretchImageRule,
       kStretchImageRule},
  }};

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);

    const auto& expected = expected_pieces[piece];
    EXPECT_FLOAT_EQ(draw_info.destination.x(), expected.destination.x());
    EXPECT_FLOAT_EQ(draw_info.destination.y(), expected.destination.y());
    EXPECT_FLOAT_EQ(draw_info.destination.width(),
                    expected.destination.width());
    EXPECT_FLOAT_EQ(draw_info.destination.height(),
                    expected.destination.height());
    EXPECT_FLOAT_EQ(draw_info.source.x(), expected.source.x());
    EXPECT_FLOAT_EQ(draw_info.source.y(), expected.source.y());
    EXPECT_FLOAT_EQ(draw_info.source.width(), expected.source.width());
    EXPECT_FLOAT_EQ(draw_info.source.height(), expected.source.height());

    if (expected.is_corner_piece)
      continue;

    EXPECT_FLOAT_EQ(draw_info.tile_scale.x(), expected.tile_scale_horizontal);
    EXPECT_FLOAT_EQ(draw_info.tile_scale.y(), expected.tile_scale_vertical);
    EXPECT_EQ(draw_info.tile_rule.vertical, expected.vertical_rule);
    EXPECT_EQ(draw_info.tile_rule.horizontal, expected.horizontal_rule);
  }
}

TEST_F(NinePieceImageGridTest,
       NinePieceImagePainting_ZoomedMiddleNoLeftRightEdge) {
  constexpr float zoom = 2;
  // A border-image where the left and right edges are collapsed (zero-width),
  // and thus not drawable, as well as zoomed.
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(32, 0, 32, 0));
  nine_piece.SetBorderSlices(BorderImageLengthBox(32 * zoom, 0, 32 * zoom, 0));
  nine_piece.SetHorizontalRule(kStretchImageRule);
  nine_piece.SetVerticalRule(kRepeatImageRule);
  nine_piece.SetFill(true);

  gfx::SizeF image_size(32, 96);
  gfx::Rect border_image_area(24, 8, 128, 464);
  gfx::Outsets border_widths(0);

  NinePieceImageGrid grid(nine_piece, image_size, gfx::Vector2dF(1, 1), zoom,
                          border_image_area, border_widths);
  NinePieceImageGrid::NinePieceDrawInfo draw_info =
      grid.GetNinePieceDrawInfo(kMiddlePiece);
  EXPECT_TRUE(draw_info.is_drawable);
  // border-image-area-width / image-width (128 / 32)
  EXPECT_FLOAT_EQ(draw_info.tile_scale.x(), 4);
  // zoom (because no edges available to derive scale from)
  EXPECT_FLOAT_EQ(draw_info.tile_scale.y(), zoom);
}

}  // namespace
}  // namespace blink
