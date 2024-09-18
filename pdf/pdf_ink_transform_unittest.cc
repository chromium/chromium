// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include <array>

#include "pdf/page_orientation.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

// Standard page size for tests.
constexpr gfx::Size kPageSizePortrait(50, 60);
constexpr gfx::Size kPageSizePortrait2x(kPageSizePortrait.width() * 2,
                                        kPageSizePortrait.height() * 2);
constexpr gfx::Size kPageSizeLandscape(kPageSizePortrait.height(),
                                       kPageSizePortrait.width());
constexpr gfx::Size kPageSizeLandscape2x(kPageSizeLandscape.width() * 2,
                                         kPageSizeLandscape.height() * 2);

// Scale factors used in tests.
constexpr float kScaleFactor1x = 1.0f;
constexpr float kScaleFactor2x = 2.0f;

// Standard page content area for tests.
constexpr gfx::Rect kPageContentAreaPortraitNoOffset(gfx::Point(),
                                                     kPageSizePortrait);
constexpr gfx::Rect kPageContentAreaPortraitNoOffset2x(gfx::Point(),
                                                       kPageSizePortrait2x);
constexpr gfx::Rect kPageContentAreaLandscapeNoOffset(gfx::Point(),
                                                      kPageSizeLandscape);

// Viewport origin offset used in tests.
constexpr gfx::Vector2dF kViewportOriginOffsetNone;

// Sample input positions in screen-based coordinates, based upon the standard
// page size.
constexpr gfx::PointF kInputPositionTopLeft;
constexpr gfx::PointF kInputPositionPortraitBottomRight(49.0f, 59.0f);
constexpr gfx::PointF kInputPositionLandscapeBottomRight(59.0f, 49.0f);
constexpr gfx::PointF kInputPositionPortraitBottomRight2x(99.0f, 119.0f);
constexpr gfx::PointF kInputPositionLandscapeBottomRight2x(119.0f, 99.0f);
constexpr gfx::PointF kInputPositionInterior(40.0f, 16.0f);
constexpr gfx::PointF kInputPositionInterior2x(80.0f, 32.0f);

// Sample canonical output positions.
constexpr gfx::PointF kCanonicalPositionTopLeft;
constexpr gfx::PointF kCanonicalPositionTopRight(49.0f, 0.0f);
constexpr gfx::PointF kCanonicalPositionBottomLeft(0.0f, 59.0f);
constexpr gfx::PointF kCanonicalPositionBottomRight(49.0f, 59.0f);

// Canonical positions can have fractional parts if the scale factor was
// not 1.0. When converting from a scale of 2x, the canonical position can end
// up with an additional half.
constexpr gfx::Vector2dF kCanonicalPositionHalf(0.5f, 0.5f);
constexpr gfx::Vector2dF kCanonicalPositionHalfX(0.5f, 0.0f);
constexpr gfx::Vector2dF kCanonicalPositionHalfY(0.0f, 0.5f);

struct InputOutputPair {
  gfx::PointF input_event_position;
  gfx::PointF output_css_pixel;
};

}  // namespace

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionIdentity) {
  constexpr auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{kInputPositionTopLeft, kCanonicalPositionTopLeft},
      InputOutputPair{kInputPositionPortraitBottomRight,
                      kCanonicalPositionBottomRight},
      InputOutputPair{kInputPositionInterior, gfx::PointF(40.0f, 16.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(input_output.output_css_pixel,
              EventPositionToCanonicalPosition(
                  input_output.input_event_position, PageOrientation::kOriginal,
                  kPageContentAreaPortraitNoOffset, kScaleFactor1x));
  }
}

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionZoom) {
  constexpr auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{kInputPositionTopLeft, kCanonicalPositionTopLeft},
      InputOutputPair{kInputPositionPortraitBottomRight2x,
                      kCanonicalPositionBottomRight + kCanonicalPositionHalf},
      InputOutputPair{kInputPositionInterior2x, gfx::PointF(40.0f, 16.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(input_output.output_css_pixel,
              EventPositionToCanonicalPosition(
                  input_output.input_event_position, PageOrientation::kOriginal,
                  kPageContentAreaPortraitNoOffset2x, kScaleFactor2x));
  }
}

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionRotateClockwise90) {
  constexpr auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{kInputPositionTopLeft, kCanonicalPositionBottomLeft},
      InputOutputPair{kInputPositionLandscapeBottomRight,
                      kCanonicalPositionTopRight},
      InputOutputPair{kInputPositionInterior, gfx::PointF(16.0f, 19.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(
        input_output.output_css_pixel,
        EventPositionToCanonicalPosition(
            input_output.input_event_position, PageOrientation::kClockwise90,
            kPageContentAreaLandscapeNoOffset, kScaleFactor1x));
  }
}

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionRotateClockwise180) {
  constexpr auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{kInputPositionTopLeft, kCanonicalPositionBottomRight},
      InputOutputPair{kInputPositionPortraitBottomRight,
                      kCanonicalPositionTopLeft},
      InputOutputPair{kInputPositionInterior, gfx::PointF(9.0f, 43.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(
        input_output.output_css_pixel,
        EventPositionToCanonicalPosition(
            input_output.input_event_position, PageOrientation::kClockwise180,
            kPageContentAreaPortraitNoOffset, kScaleFactor1x));
  }
}

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionRotateClockwise270) {
  constexpr auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{kInputPositionTopLeft, kCanonicalPositionTopRight},
      InputOutputPair{kInputPositionLandscapeBottomRight,
                      kCanonicalPositionBottomLeft},
      InputOutputPair{kInputPositionInterior, gfx::PointF(33.0f, 40.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(
        input_output.output_css_pixel,
        EventPositionToCanonicalPosition(
            input_output.input_event_position, PageOrientation::kClockwise270,
            kPageContentAreaLandscapeNoOffset, kScaleFactor1x));
  }
}

TEST(PdfInkTransformTest, EventPositionToCanonicalPositionScrolled) {
  constexpr gfx::Point kPageContentRectOrigin(-8, -14);
  const auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{
          kInputPositionTopLeft + kPageContentRectOrigin.OffsetFromOrigin(),
          kCanonicalPositionTopLeft},
      InputOutputPair{kInputPositionPortraitBottomRight +
                          kPageContentRectOrigin.OffsetFromOrigin(),
                      kCanonicalPositionBottomRight},
      InputOutputPair{kInputPositionInterior, gfx::PointF(48.0f, 30.0f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(input_output.output_css_pixel,
              EventPositionToCanonicalPosition(
                  input_output.input_event_position, PageOrientation::kOriginal,
                  /*page_content_rect=*/
                  gfx::Rect(kPageContentRectOrigin, kPageSizePortrait),
                  kScaleFactor1x));
  }
}

TEST(PdfInkTransformTest,
     EventPositionToCanonicalPositionZoomScrolledClockwise90) {
  constexpr gfx::Point kPageContentRectOrigin(-16, -28);
  const auto kInputsAndOutputs = std::to_array<InputOutputPair>({
      InputOutputPair{
          kInputPositionTopLeft + kPageContentRectOrigin.OffsetFromOrigin(),
          kCanonicalPositionBottomLeft + kCanonicalPositionHalfY},
      InputOutputPair{kInputPositionLandscapeBottomRight2x +
                          kPageContentRectOrigin.OffsetFromOrigin(),
                      kCanonicalPositionTopRight + kCanonicalPositionHalfX},
      InputOutputPair{kInputPositionInterior2x, gfx::PointF(30.0f, 11.5f)},
  });

  for (const auto& input_output : kInputsAndOutputs) {
    EXPECT_EQ(
        input_output.output_css_pixel,
        EventPositionToCanonicalPosition(
            input_output.input_event_position, PageOrientation::kClockwise90,
            /*page_content_rect=*/
            gfx::Rect(kPageContentRectOrigin, kPageSizeLandscape2x),
            kScaleFactor2x));
  }
}

TEST(PdfInkTransformTest, RenderTransformIdentity) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      kPageContentAreaPortraitNoOffset, kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformZoom) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      kPageContentAreaPortraitNoOffset2x, kScaleFactor2x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise90,
      kPageContentAreaLandscapeNoOffset, kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -1.0f, 59.0f, 1.0f, 0.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise180) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise180,
      kPageContentAreaPortraitNoOffset, kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(-1.0f, 0.0f, 49.0f, 0.0f, -1.0f, 59.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise270) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise270,
      kPageContentAreaLandscapeNoOffset, kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 49.0f));
}

TEST(PdfInkTransformTest, RenderTransformScrolled) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      /*page_content_rect=*/gfx::Rect(gfx::Point(-8, -14), kPageSizePortrait),
      kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, -8.0f, 0.0f, 1.0f, -14.0f));
}

TEST(PdfInkTransformTest, RenderTransformOffsetScrolled) {
  ink::AffineTransform transform = GetInkRenderTransform(
      /*viewport_origin_offset=*/gfx::Vector2dF(18.0f, 24.0f),
      PageOrientation::kOriginal,
      /*page_content_rect=*/gfx::Rect(gfx::Point(0, -14), kPageSizePortrait),
      kScaleFactor1x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, 18.0f, 0.0f, 1.0f, 10.0f));
}

TEST(PdfInkTransformTest, RenderTransformZoomScrolledClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise90,
      /*page_content_rect=*/
      gfx::Rect(gfx::Point(-16, -28), kPageSizeLandscape2x), kScaleFactor2x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -2.0f, 103.0f, 2.0f, 0.0f, -28.0f));
}

TEST(PdfInkTransformTest, RenderTransformOffsetZoomScrolledClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      /*viewport_origin_offset=*/gfx::Vector2dF(18.0f, 24.0f),
      PageOrientation::kClockwise90,
      /*page_content_rect=*/gfx::Rect(gfx::Point(0, -28), kPageSizeLandscape2x),
      kScaleFactor2x);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -2.0f, 137.0f, 2.0f, 0.0f, -4.0f));
}

}  // namespace chrome_pdf
