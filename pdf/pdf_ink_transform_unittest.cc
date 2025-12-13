// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include <array>

#include "pdf/page_orientation.h"
#include "pdf/page_rotation.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/envelope.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

using printing::kUnitConversionFactorPixelsToPoints;

namespace chrome_pdf {

namespace {

// Standard page size in pixels for tests.
constexpr gfx::Size kPageSizePortrait(50, 60);
constexpr gfx::Size kPageSizePortrait2x(kPageSizePortrait.width() * 2,
                                        kPageSizePortrait.height() * 2);
constexpr gfx::Size kPageSizeLandscape(kPageSizePortrait.height(),
                                       kPageSizePortrait.width());
constexpr gfx::Size kPageSizeLandscape2x(kPageSizeLandscape.width() * 2,
                                         kPageSizeLandscape.height() * 2);

// Standard page size in points for tests, corresponding to page size in pixels
// above.
constexpr gfx::SizeF kPageSizePortraitInPoints(
    kPageSizePortrait.width() * kUnitConversionFactorPixelsToPoints,
    kPageSizePortrait.height() * kUnitConversionFactorPixelsToPoints);

// A page size in points `kPageSizePortraitFractionalInPoints` has fractional
// component, which gets truncated when converted to integer pixels.
constexpr gfx::SizeF kPageSizePortraitFractionalInPoints(199.9f, 201.1f);
constexpr gfx::Size kPageSizePortraitFractional(266, 268);
constexpr gfx::Size kPageSizeLandscapeFractional(268, 266);

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

struct TransformTestCase {
  gfx::PointF input_event_position;
  gfx::PointF expected_css_pixel;
};

}  // namespace

TEST(PdfInkTransformTest, EventToCanonicalTransformIdentity) {
  constexpr auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft, kCanonicalPositionTopLeft},
      {kInputPositionPortraitBottomRight, kCanonicalPositionBottomRight},
      {kInputPositionInterior, gfx::PointF(40.0f, 16.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kOriginal, kPageContentAreaPortraitNoOffset,
      kScaleFactor1x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformZoom) {
  constexpr auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft, kCanonicalPositionTopLeft},
      {kInputPositionPortraitBottomRight2x,
       kCanonicalPositionBottomRight + kCanonicalPositionHalf},
      {kInputPositionInterior2x, gfx::PointF(40.0f, 16.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kOriginal, kPageContentAreaPortraitNoOffset2x,
      kScaleFactor2x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformRotateClockwise90) {
  constexpr auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft, kCanonicalPositionBottomLeft},
      {kInputPositionLandscapeBottomRight, kCanonicalPositionTopRight},
      {kInputPositionInterior, gfx::PointF(16.0f, 19.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kClockwise90, kPageContentAreaLandscapeNoOffset,
      kScaleFactor1x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformRotateClockwise180) {
  constexpr auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft, kCanonicalPositionBottomRight},
      {kInputPositionPortraitBottomRight, kCanonicalPositionTopLeft},
      {kInputPositionInterior, gfx::PointF(9.0f, 43.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kClockwise180, kPageContentAreaPortraitNoOffset,
      kScaleFactor1x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformRotateClockwise270) {
  constexpr auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft, kCanonicalPositionTopRight},
      {kInputPositionLandscapeBottomRight, kCanonicalPositionBottomLeft},
      {kInputPositionInterior, gfx::PointF(33.0f, 40.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kClockwise270, kPageContentAreaLandscapeNoOffset,
      kScaleFactor1x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformScrolled) {
  constexpr gfx::Point kPageContentRectOrigin(-8, -14);
  const auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft + kPageContentRectOrigin.OffsetFromOrigin(),
       kCanonicalPositionTopLeft},
      {kInputPositionPortraitBottomRight +
           kPageContentRectOrigin.OffsetFromOrigin(),
       kCanonicalPositionBottomRight},
      {kInputPositionInterior, gfx::PointF(48.0f, 30.0f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kOriginal,
      gfx::Rect(kPageContentRectOrigin, kPageSizePortrait), kScaleFactor1x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, EventToCanonicalTransformZoomScrolledClockwise90) {
  constexpr gfx::Point kPageContentRectOrigin(-16, -28);
  const auto kTestCases = std::to_array<TransformTestCase>({
      {kInputPositionTopLeft + kPageContentRectOrigin.OffsetFromOrigin(),
       kCanonicalPositionBottomLeft + kCanonicalPositionHalfY},
      {kInputPositionLandscapeBottomRight2x +
           kPageContentRectOrigin.OffsetFromOrigin(),
       kCanonicalPositionTopRight + kCanonicalPositionHalfX},
      {kInputPositionInterior2x, gfx::PointF(30.0f, 11.5f)},
  });

  gfx::Transform transform = GetEventToCanonicalTransform(
      PageOrientation::kClockwise90,
      gfx::Rect(kPageContentRectOrigin, kPageSizeLandscape2x), kScaleFactor2x);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_css_pixel,
              transform.MapPoint(test_case.input_event_position));
  }
}

TEST(PdfInkTransformTest, RenderTransformIdentity) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      kPageContentAreaPortraitNoOffset, kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformZoom) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      kPageContentAreaPortraitNoOffset2x, kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise90,
      kPageContentAreaLandscapeNoOffset, kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -1.0f, 60.0f, 1.0f, 0.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise180) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise180,
      kPageContentAreaPortraitNoOffset, kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(-1.0f, 0.0f, 50.0f, 0.0f, -1.0f, 60.0f));
}

TEST(PdfInkTransformTest, RenderTransformRotateClockwise270) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise270,
      kPageContentAreaLandscapeNoOffset, kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, 1.0, 0.0f, -1.0f, 0.0f, 50.0f));
}

TEST(PdfInkTransformTest, RenderTransformScrolled) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      /*page_content_rect=*/gfx::Rect(gfx::Point(-8, -14), kPageSizePortrait),
      kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, -8.0f, 0.0f, 1.0f, -14.0f));
}

TEST(PdfInkTransformTest, RenderTransformOffsetScrolled) {
  ink::AffineTransform transform = GetInkRenderTransform(
      /*viewport_origin_offset=*/gfx::Vector2dF(18.0f, 24.0f),
      PageOrientation::kOriginal,
      /*page_content_rect=*/gfx::Rect(gfx::Point(0, -14), kPageSizePortrait),
      kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(1.0f, 0.0f, 18.0f, 0.0f, 1.0f, 10.0f));
}

TEST(PdfInkTransformTest, RenderTransformZoomScrolledClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise90,
      /*page_content_rect=*/
      gfx::Rect(gfx::Point(-16, -28), kPageSizeLandscape2x),
      kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -2.0, 104.0f, 2.0f, 0.0f, -28.0f));
}

TEST(PdfInkTransformTest, RenderTransformOffsetZoomScrolledClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      /*viewport_origin_offset=*/gfx::Vector2dF(18.0f, 24.0f),
      PageOrientation::kClockwise90,
      /*page_content_rect=*/gfx::Rect(gfx::Point(0, -28), kPageSizeLandscape2x),
      kPageSizePortraitInPoints);
  EXPECT_THAT(transform,
              InkAffineTransformEq(0.0f, -2.0, 138.0f, 2.0f, 0.0f, -4.0f));
}

TEST(PdfInkTransformTest, RenderTransformFractionalPointsSizeIdentity) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kOriginal,
      gfx::Rect(gfx::Point(), kPageSizePortraitFractional),
      kPageSizePortraitFractionalInPoints);
  EXPECT_THAT(transform, InkAffineTransformEq(0.997999012f, 0.0f, 0.0f, 0.0f,
                                              0.999502718f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformFractionalPointsSizeClockwise90) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise90,
      gfx::Rect(gfx::Point(), kPageSizeLandscapeFractional),
      kPageSizePortraitFractionalInPoints);
  EXPECT_THAT(transform, InkAffineTransformEq(0.0f, -0.997999012f, 268.0f,
                                              0.999502718f, 0.0f, 0.0f));
}

TEST(PdfInkTransformTest, RenderTransformFractionalPointsSizeClockwise180) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise180,
      gfx::Rect(gfx::Point(), kPageSizePortraitFractional),
      kPageSizePortraitFractionalInPoints);
  EXPECT_THAT(transform, InkAffineTransformEq(-0.997999012f, 0.0f, 266.0f, 0.0f,
                                              -0.999502718f, 268.0f));
}

TEST(PdfInkTransformTest, RenderTransformFractionalPointsSizeClockwise270) {
  ink::AffineTransform transform = GetInkRenderTransform(
      kViewportOriginOffsetNone, PageOrientation::kClockwise270,
      gfx::Rect(gfx::Point(), kPageSizeLandscapeFractional),
      kPageSizePortraitFractionalInPoints);
  EXPECT_THAT(transform, InkAffineTransformEq(0.0f, 0.997999012f, 0.0f,
                                              -0.999502718f, 0.0f, 266.0f));
}

TEST(PdfInkTransformTest, ThumbnailTransformNoZoom) {
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kOriginal,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kOriginal,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform, InkAffineTransformEq(1.6666666f, 0.0f, 0.0f, 0.0f,
                                                1.6666666f, 0.0f));
  }
}

TEST(PdfInkTransformTest, ThumbnailTransformZoom) {
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kOriginal,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kOriginal,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform, InkAffineTransformEq(3.333333f, 0.0f, 0.0f, 0.0f,
                                                3.333333f, 0.0f));
  }
}

TEST(PdfInkTransformTest, ThumbnailTransformRotate) {
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise90,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform, InkAffineTransformEq(0.8333333f, 0.0f, 0.0f, 0.0f,
                                                0.8333333f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise90,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise180,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise180,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform, InkAffineTransformEq(1.6666666f, 0.0f, 0.0f, 0.0f,
                                                1.6666666f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise270,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform, InkAffineTransformEq(0.8333333f, 0.0f, 0.0f, 0.0f,
                                                0.8333333f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise270,
        kPageContentAreaPortraitNoOffset, kScaleFactor1x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
  }
}

TEST(PdfInkTransformTest, ThumbnailTransformRotateAndZoom) {
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise90,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform, InkAffineTransformEq(1.6666666f, 0.0f, 0.0f, 0.0f,
                                                1.6666666f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise90,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(4.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise180,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise180,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform, InkAffineTransformEq(3.333333f, 0.0f, 0.0f, 0.0f,
                                                3.333333f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{50, 60}, PageOrientation::kClockwise270,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform, InkAffineTransformEq(1.6666666f, 0.0f, 0.0f, 0.0f,
                                                1.6666666f, 0.0f));
  }
  {
    ink::AffineTransform transform = GetInkThumbnailTransform(
        /*canvas_size=*/{120, 100}, PageOrientation::kClockwise270,
        kPageContentAreaPortraitNoOffset, kScaleFactor2x);
    EXPECT_THAT(transform,
                InkAffineTransformEq(4.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f));
  }
}

TEST(PdfInkTransformTest, CanonicalInkEnvelopeToInvalidationScreenRect) {
  static constexpr gfx::Rect kExpectedScreenRect(20, 40, 82, 144);
  ink::Envelope envelope(ink::Point(10.5f, 20.6f));
  envelope.Add(ink::Point(50.0f, 90.9f));

  gfx::Rect screen_rect = CanonicalInkEnvelopeToInvalidationScreenRect(
      envelope, gfx::Transform::MakeScale(2.0f));
  EXPECT_EQ(screen_rect, kExpectedScreenRect);
}

TEST(PdfInkTransformTest, GetCanonicalToPdfTransform) {
  static constexpr gfx::SizeF kPageSize(300, 600);
  static constexpr gfx::SizeF kRotated90PageSize(600, 300);

  static constexpr gfx::Vector2dF kNoTranslate;
  static constexpr gfx::Vector2dF kTranslate(50, 60);

  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kPageSize, PageRotation::kRotate0, kNoTranslate);
    EXPECT_EQ(gfx::PointF(0, 600), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(0, 0), tr.MapPoint(gfx::PointF(0, 800)));
    EXPECT_EQ(gfx::PointF(300, 600), tr.MapPoint(gfx::PointF(400, 0)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kPageSize, PageRotation::kRotate0, kTranslate);
    EXPECT_EQ(gfx::PointF(50, 660), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(50, 60), tr.MapPoint(gfx::PointF(0, 800)));
    EXPECT_EQ(gfx::PointF(350, 660), tr.MapPoint(gfx::PointF(400, 0)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kRotated90PageSize, PageRotation::kRotate90, kNoTranslate);
    EXPECT_EQ(gfx::PointF(0, 0), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(0, 600), tr.MapPoint(gfx::PointF(800, 0)));
    EXPECT_EQ(gfx::PointF(300, 0), tr.MapPoint(gfx::PointF(0, 400)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kRotated90PageSize, PageRotation::kRotate90, kTranslate);
    EXPECT_EQ(gfx::PointF(50, 60), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(50, 660), tr.MapPoint(gfx::PointF(800, 0)));
    EXPECT_EQ(gfx::PointF(350, 60), tr.MapPoint(gfx::PointF(0, 400)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kPageSize, PageRotation::kRotate180, kTranslate);
    EXPECT_EQ(gfx::PointF(350, 60), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(350, 660), tr.MapPoint(gfx::PointF(0, 800)));
    EXPECT_EQ(gfx::PointF(50, 60), tr.MapPoint(gfx::PointF(400, 0)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kPageSize, PageRotation::kRotate180, kNoTranslate);
    EXPECT_EQ(gfx::PointF(300, 0), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(300, 600), tr.MapPoint(gfx::PointF(0, 800)));
    EXPECT_EQ(gfx::PointF(0, 0), tr.MapPoint(gfx::PointF(400, 0)));
  }
  {
    gfx::Transform tr = GetCanonicalToPdfTransform(
        kRotated90PageSize, PageRotation::kRotate270, kTranslate);
    EXPECT_EQ(gfx::PointF(350, 660), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(350, 60), tr.MapPoint(gfx::PointF(800, 0)));
    EXPECT_EQ(gfx::PointF(50, 660), tr.MapPoint(gfx::PointF(0, 400)));
  }
  {
    gfx::Transform tr =
        GetCanonicalToPdfTransform(kRotated90PageSize, PageRotation::kRotate270,
                                   /*translate=*/gfx::Vector2dF());
    EXPECT_EQ(gfx::PointF(300, 600), tr.MapPoint(gfx::PointF(0, 0)));
    EXPECT_EQ(gfx::PointF(300, 0), tr.MapPoint(gfx::PointF(800, 0)));
    EXPECT_EQ(gfx::PointF(0, 600), tr.MapPoint(gfx::PointF(0, 400)));
  }
}

}  // namespace chrome_pdf
