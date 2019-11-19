// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_paint_canvas.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {
namespace {

using testing::_;
using testing::AllOf;
using testing::FloatNear;
using testing::InvokeWithoutArgs;
using testing::Property;

constexpr float kBaseIconWidth = 24.0f;
constexpr float kBaseIconHeight = 24.0f;
constexpr float kBaseFeaturePaddingX = 8.0f;
constexpr float kBaseIconPaddingY = 5.0f;

constexpr float kBasePaddingBetweenIconAndText = 2.0f;
constexpr float kBaseTextPaddingY = 9.0f;
constexpr float kBaseFontSize = 14.0f;

constexpr float kBaseIconOnlyFeatureWidth =
    kBaseFeaturePaddingX + kBaseIconWidth + kBaseFeaturePaddingX;
constexpr float kBaseFeatureHeight =
    kBaseIconPaddingY + kBaseIconHeight + kBaseIconPaddingY;

void ExpectDrawGrayBox(MockPaintCanvas& canvas,
                       const FloatRect& expected_rect) {
  EXPECT_CALL(
      canvas,
      drawRect(AllOf(Property(&SkRect::x, FloatNear(expected_rect.X(), 0.01)),
                     Property(&SkRect::y, FloatNear(expected_rect.Y(), 0.01)),
                     Property(&SkRect::width,
                              FloatNear(expected_rect.Width(), 0.01)),
                     Property(&SkRect::height,
                              FloatNear(expected_rect.Height(), 0.01))),
               AllOf(Property(&PaintFlags::getStyle, PaintFlags::kFill_Style),
                     Property(&PaintFlags::getColor,
                              SkColorSetARGB(0x80, 0xD9, 0xD9, 0xD9)))))
      .Times(1);
}

void DrawImageExpectingGrayBoxOnly(PlaceholderImage& image,
                                   const FloatRect& dest_rect) {
  MockPaintCanvas canvas;
  ExpectDrawGrayBox(canvas, dest_rect);
  EXPECT_CALL(canvas, drawImageRect(_, _, _, _, _)).Times(0);
  EXPECT_CALL(canvas, drawTextBlob(_, _, _, _)).Times(0);

  image.Draw(&canvas, PaintFlags(), dest_rect,
             FloatRect(0.0f, 0.0f, 100.0f, 100.0f),
             kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
             Image::kUnspecifiedDecode);
}

void DrawImageExpectingIconOnly(PlaceholderImage& image,
                                const FloatRect& dest_rect,
                                float scale_factor) {
  MockPaintCanvas canvas;
  ExpectDrawGrayBox(canvas, dest_rect);

  EXPECT_CALL(
      canvas,
      drawImageRect(
          /*image=*/_, /*src=*/_, /*dst=*/
          AllOf(Property(&SkRect::x,
                         FloatNear(dest_rect.Center().X() -
                                       scale_factor * kBaseIconWidth / 2.0f,
                                   0.01)),
                Property(&SkRect::y,
                         FloatNear(dest_rect.Center().Y() -
                                       scale_factor * kBaseIconHeight / 2.0f,
                                   0.01)),
                Property(&SkRect::width,
                         FloatNear(scale_factor * kBaseIconWidth, 0.01)),
                Property(&SkRect::height,
                         FloatNear(scale_factor * kBaseIconHeight, 0.01))),
          /*flags=*/_, /*constraint=*/_))
      .Times(1);

  EXPECT_CALL(canvas, drawTextBlob(_, _, _, _)).Times(0);

  image.Draw(&canvas, PaintFlags(), dest_rect,
             FloatRect(0.0f, 0.0f, 100.0f, 100.0f),
             kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
             Image::kUnspecifiedDecode);
}

float GetExpectedPlaceholderTextWidth(const StringView& text,
                                      float scale_factor) {
  FontDescription description;
  description.FirstFamily().SetFamily("Roboto");

  scoped_refptr<SharedFontFamily> helvetica_neue = SharedFontFamily::Create();
  helvetica_neue->SetFamily("Helvetica Neue");
  scoped_refptr<SharedFontFamily> helvetica = SharedFontFamily::Create();
  helvetica->SetFamily("Helvetica");
  scoped_refptr<SharedFontFamily> arial = SharedFontFamily::Create();
  arial->SetFamily("Arial");

  helvetica->AppendFamily(std::move(arial));
  helvetica_neue->AppendFamily(std::move(helvetica));
  description.FirstFamily().AppendFamily(std::move(helvetica_neue));

  description.SetGenericFamily(FontDescription::kSansSerifFamily);
  description.SetComputedSize(scale_factor * 14.0f);
  description.SetWeight(FontSelectionValue(500));

  Font font(description);
  font.Update(nullptr);
  return font.Width(TextRun(text));
}

void DrawImageExpectingIconAndTextLTR(PlaceholderImage& image,
                                      const FloatRect& dest_rect,
                                      float scale_factor) {
  EXPECT_FALSE(Locale::DefaultLocale().IsRTL());

  MockPaintCanvas canvas;
  ExpectDrawGrayBox(canvas, dest_rect);

  const float expected_text_width =
      GetExpectedPlaceholderTextWidth(image.GetTextForTesting(), scale_factor);
  const float expected_feature_width =
      scale_factor *
          (kBaseIconOnlyFeatureWidth + kBasePaddingBetweenIconAndText) +
      expected_text_width;
  const float expected_feature_x =
      dest_rect.Center().X() - expected_feature_width / 2.0f;
  const float expected_feature_y =
      dest_rect.Center().Y() - scale_factor * kBaseFeatureHeight / 2.0f;

  EXPECT_CALL(
      canvas,
      drawImageRect(
          /*image=*/_, /*src=*/_, /*dst=*/
          AllOf(Property(&SkRect::x,
                         FloatNear(expected_feature_x +
                                       scale_factor * kBaseFeaturePaddingX,
                                   0.01)),
                Property(&SkRect::y,
                         FloatNear(expected_feature_y +
                                       scale_factor * kBaseIconPaddingY,
                                   0.01)),
                Property(&SkRect::width,
                         FloatNear(scale_factor * kBaseIconWidth, 0.01)),
                Property(&SkRect::height,
                         FloatNear(scale_factor * kBaseIconHeight, 0.01))),
          /*flags=*/_,
          /*constraint=*/_))
      .Times(1);

  EXPECT_CALL(
      canvas,
      drawTextBlob(
          _,
          FloatNear(expected_feature_x +
                        scale_factor * (kBaseFeaturePaddingX + kBaseIconWidth +
                                        kBasePaddingBetweenIconAndText),
                    0.01),
          FloatNear(expected_feature_y +
                        scale_factor * (kBaseTextPaddingY + kBaseFontSize),
                    0.01),
          AllOf(
              Property(&PaintFlags::getStyle, PaintFlags::kFill_Style),
              Property(&PaintFlags::getColor, SkColorSetARGB(0xAB, 0, 0, 0)))))
      .WillOnce(InvokeWithoutArgs([&image, scale_factor]() {
        EXPECT_NEAR(
            scale_factor * kBaseFontSize,
            image.GetFontForTesting()->GetFontDescription().ComputedSize(),
            0.01);
      }));

  image.Draw(&canvas, PaintFlags(), dest_rect,
             FloatRect(0.0f, 0.0f, 100.0f, 100.0f),
             kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
             Image::kUnspecifiedDecode);
}

class TestingUnitsPlatform : public TestingPlatformSupport {
 public:
  TestingUnitsPlatform() {}
  ~TestingUnitsPlatform() override;

  WebString QueryLocalizedString(int resource_id,
                                 const WebString& parameter) override {
    String p = parameter;
    switch (resource_id) {
      case IDS_UNITS_KIBIBYTES:
        return String(p + " KB");
      case IDS_UNITS_MEBIBYTES:
        return String(p + " MB");
      case IDS_UNITS_GIBIBYTES:
        return String(p + " GB");
      case IDS_UNITS_TEBIBYTES:
        return String(p + " TB");
      case IDS_UNITS_PEBIBYTES:
        return String(p + " PB");
      default:
        return WebString();
    }
  }
};

TestingUnitsPlatform::~TestingUnitsPlatform() = default;

class PlaceholderImageTest : public testing::Test {
 public:
  void SetUp() override {
    old_user_preferred_languages_ = UserPreferredLanguages();
    OverrideUserPreferredLanguagesForTesting(Vector<AtomicString>(1U, "en-US"));
  }

  void TearDown() override {
    OverrideUserPreferredLanguagesForTesting(old_user_preferred_languages_);
  }

 private:
  ScopedTestingPlatformSupport<TestingUnitsPlatform> platform_;
  Vector<AtomicString> old_user_preferred_languages_;
};

TEST_F(PlaceholderImageTest, FormatPlaceholderText) {
  const struct {
    int64_t bytes;
    const char* expected;
  } tests[] = {
      // Placeholder image number format specifications:
      // https://docs.google.com/document/d/1BHeA1azbgCdZgCnr16VN2g7A9MHPQ_dwKn5szh8evMQ/edit#heading=h.d135l9z7tn0a
      {1, "1 KB"},
      {500, "1 KB"},
      {5 * 1024 + 200, "5 KB"},
      {50 * 1024 + 200, "50 KB"},
      {1000 * 1024 - 1, "999 KB"},
      {1000 * 1024, "1 MB"},
      {1024 * 1024 + 103 * 1024, "1.1 MB"},
      {10 * 1024 * 1024, "10 MB"},
      {10 * 1024 * 1024 + 103 * 1024, "10 MB"},
      {1000 * 1024 * 1024 - 1, "999 MB"},
      {1000 * 1024 * 1024, "1 GB"},
      {1024 * 1024 * 1024, "1 GB"},
      {(1LL << 50), "1 PB"},
      {(1LL << 50) + 103 * (1LL << 40), "1.1 PB"},
      {10 * (1LL << 50), "10 PB"},
      {10 * (1LL << 50) + 103 * (1LL << 40), "10 PB"},
      {~(1LL << 63), "8191 PB"},
  };

  for (const auto& test : tests) {
    String expected = test.expected;
    expected.Ensure16Bit();

    EXPECT_EQ(expected,
              PlaceholderImage::Create(nullptr, IntSize(400, 300), test.bytes)
                  ->GetTextForTesting());
  }
}

TEST_F(PlaceholderImageTest, DrawLazyImage) {
  MockPaintCanvas canvas;
  EXPECT_CALL(canvas, drawRect(_, _)).Times(0);
  EXPECT_CALL(canvas, drawImageRect(_, _, _, _, _)).Times(0);
  EXPECT_CALL(canvas, drawTextBlob(_, _, _, _)).Times(0);

  PlaceholderImage::CreateForLazyImages(nullptr, IntSize(800, 600))
      ->Draw(&canvas, PaintFlags(), FloatRect(0.0f, 0.0f, 800.0f, 600.0f),
             FloatRect(0.0f, 0.0f, 800.0f, 600.0f),
             kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
             Image::kUnspecifiedDecode);
}

TEST_F(PlaceholderImageTest, DrawNonIntersectingSrcRect) {
  MockPaintCanvas canvas;
  EXPECT_CALL(canvas, drawRect(_, _)).Times(0);
  EXPECT_CALL(canvas, drawImageRect(_, _, _, _, _)).Times(0);
  EXPECT_CALL(canvas, drawTextBlob(_, _, _, _)).Times(0);

  PlaceholderImage::Create(nullptr, IntSize(800, 600), 0)
      ->Draw(&canvas, PaintFlags(), FloatRect(0.0f, 0.0f, 800.0f, 600.0f),
             // The source rectangle is outside the 800x600 bounds of the image,
             // so nothing should be drawn.
             FloatRect(1000.0f, 0.0f, 800.0f, 600.0f),
             kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
             Image::kUnspecifiedDecode);
}

TEST_F(PlaceholderImageTest, DrawWithoutOriginalResourceSize) {
  scoped_refptr<PlaceholderImage> image =
      PlaceholderImage::Create(nullptr, IntSize(800, 600), 0);

  constexpr float kTestScaleFactors[] = {0.5f, 1.0f, 2.0f};
  for (const float scale_factor : kTestScaleFactors) {
    image->SetIconAndTextScaleFactor(scale_factor);

    DrawImageExpectingGrayBoxOnly(
        *image, FloatRect(1000.0f, 2000.0f,
                          scale_factor * kBaseIconOnlyFeatureWidth - 1.0f,
                          scale_factor * kBaseFeatureHeight + 1.0f));
    DrawImageExpectingGrayBoxOnly(
        *image, FloatRect(1000.0f, 2000.0f,
                          scale_factor * kBaseIconOnlyFeatureWidth + 1.0f,
                          scale_factor * kBaseFeatureHeight - 1.0f));

    DrawImageExpectingIconOnly(
        *image,
        FloatRect(1000.0f, 2000.0f,
                  scale_factor * kBaseIconOnlyFeatureWidth + 1.0f,
                  scale_factor * kBaseFeatureHeight + 1.0f),
        scale_factor);
    DrawImageExpectingIconOnly(
        *image, FloatRect(1000.0f, 2000.0f, 800.0f, 600.0f), scale_factor);
  }
}

TEST_F(PlaceholderImageTest, DrawWithOriginalResourceSizeLTR) {
  scoped_refptr<PlaceholderImage> image =
      PlaceholderImage::Create(nullptr, IntSize(800, 600), 50 * 1024);

  String expected_text = "50 KB";
  expected_text.Ensure16Bit();
  EXPECT_EQ(expected_text, image->GetTextForTesting());

  constexpr float kTestScaleFactors[] = {0.5f, 1.0f, 2.0f};
  for (const float scale_factor : kTestScaleFactors) {
    image->SetIconAndTextScaleFactor(scale_factor);

    DrawImageExpectingGrayBoxOnly(
        *image, FloatRect(1000.0f, 2000.0f,
                          scale_factor * kBaseIconOnlyFeatureWidth - 1.0f,
                          scale_factor * kBaseFeatureHeight + 1.0f));
    DrawImageExpectingGrayBoxOnly(
        *image, FloatRect(1000.0f, 2000.0f,
                          scale_factor * kBaseIconOnlyFeatureWidth + 1.0f,
                          scale_factor * kBaseFeatureHeight - 1.0f));
    DrawImageExpectingGrayBoxOnly(
        *image, FloatRect(1000.0f, 2000.0f, 800.0f,
                          scale_factor * kBaseFeatureHeight - 1.0f));

    const float expected_text_width = GetExpectedPlaceholderTextWidth(
        image->GetTextForTesting(), scale_factor);
    const float expected_icon_and_text_width =
        scale_factor *
            (kBaseIconOnlyFeatureWidth + kBasePaddingBetweenIconAndText) +
        expected_text_width;

    DrawImageExpectingIconOnly(
        *image,
        FloatRect(1000.0f, 2000.0f,
                  scale_factor * kBaseIconOnlyFeatureWidth + 1.0f,
                  scale_factor * kBaseFeatureHeight + 1.0f),
        scale_factor);
    DrawImageExpectingIconOnly(
        *image,
        FloatRect(1000.0f, 2000.0f, expected_icon_and_text_width - 1.0f,
                  scale_factor * kBaseFeatureHeight + 1.0f),
        scale_factor);

    DrawImageExpectingIconAndTextLTR(
        *image,
        FloatRect(1000.0f, 2000.0f, expected_icon_and_text_width + 1.0f,
                  scale_factor * kBaseFeatureHeight + 1.0f),
        scale_factor);
    DrawImageExpectingIconAndTextLTR(
        *image, FloatRect(1000.0f, 2000.0f, 800.0f, 600.0f), scale_factor);
  }
}

TEST_F(PlaceholderImageTest, DrawWithOriginalResourceSizeRTL) {
  scoped_refptr<PlaceholderImage> image =
      PlaceholderImage::Create(nullptr, IntSize(800, 600), 50 * 1024);

  String expected_text = "50 KB";
  expected_text.Ensure16Bit();
  EXPECT_EQ(expected_text, image->GetTextForTesting());

  OverrideUserPreferredLanguagesForTesting(Vector<AtomicString>(1U, "ar"));
  EXPECT_TRUE(Locale::DefaultLocale().IsRTL());

  static constexpr float kScaleFactor = 2.0f;
  image->SetIconAndTextScaleFactor(kScaleFactor);

  const FloatRect dest_rect(1000.0f, 2000.0f, 800.0f, 600.0f);

  MockPaintCanvas canvas;
  ExpectDrawGrayBox(canvas, dest_rect);

  const float expected_text_width =
      GetExpectedPlaceholderTextWidth(image->GetTextForTesting(), kScaleFactor);
  const float expected_feature_width =
      kScaleFactor *
          (kBaseIconOnlyFeatureWidth + kBasePaddingBetweenIconAndText) +
      expected_text_width;
  const float expected_feature_x =
      dest_rect.Center().X() - expected_feature_width / 2.0f;
  const float expected_feature_y =
      dest_rect.Center().Y() - kScaleFactor * kBaseFeatureHeight / 2.0f;

  EXPECT_CALL(
      canvas,
      drawImageRect(
          /*image=*/_, /*src=*/_, /*dst=*/
          AllOf(Property(&SkRect::x,
                         FloatNear(expected_feature_x +
                                       kScaleFactor *
                                           (kBaseFeaturePaddingX +
                                            kBasePaddingBetweenIconAndText) +
                                       expected_text_width,
                                   0.01)),
                Property(&SkRect::y,
                         FloatNear(expected_feature_y +
                                       kScaleFactor * kBaseIconPaddingY,
                                   0.01)),
                Property(&SkRect::width,
                         FloatNear(kScaleFactor * kBaseIconWidth, 0.01)),
                Property(&SkRect::height,
                         FloatNear(kScaleFactor * kBaseIconHeight, 0.01))),
          /*flags=*/_,
          /*constraint=*/_))
      .Times(1);

  EXPECT_CALL(
      canvas,
      drawTextBlob(
          _,
          FloatNear(expected_feature_x + kScaleFactor * kBaseFeaturePaddingX,
                    0.01),
          FloatNear(expected_feature_y +
                        kScaleFactor * (kBaseTextPaddingY + kBaseFontSize),
                    0.01),
          AllOf(
              Property(&PaintFlags::getStyle, PaintFlags::kFill_Style),
              Property(&PaintFlags::getColor, SkColorSetARGB(0xAB, 0, 0, 0)))))
      .WillOnce(InvokeWithoutArgs([image]() {
        EXPECT_NEAR(
            kScaleFactor * kBaseFontSize,
            image->GetFontForTesting()->GetFontDescription().ComputedSize(),
            0.01);
      }));

  image->Draw(&canvas, PaintFlags(), dest_rect,
              FloatRect(0.0f, 0.0f, 100.0f, 100.0f),
              kDoNotRespectImageOrientation, Image::kClampImageToSourceRect,
              Image::kUnspecifiedDecode);
}

TEST_F(PlaceholderImageTest, DrawSeparateImageWithDifferentScaleFactor) {
  scoped_refptr<PlaceholderImage> image_1 =
      PlaceholderImage::Create(nullptr, IntSize(800, 600), 50 * 1024);
  constexpr float kScaleFactor1 = 0.5f;
  image_1->SetIconAndTextScaleFactor(kScaleFactor1);

  DrawImageExpectingIconAndTextLTR(
      *image_1, FloatRect(1000.0f, 2000.0f, 800.0f, 600.0f), kScaleFactor1);

  scoped_refptr<PlaceholderImage> image_2 =
      PlaceholderImage::Create(nullptr, IntSize(800, 600), 100 * 1024);
  constexpr float kScaleFactor2 = 2.0f;
  image_2->SetIconAndTextScaleFactor(kScaleFactor2);

  DrawImageExpectingIconAndTextLTR(
      *image_2, FloatRect(1000.0f, 2000.0f, 800.0f, 600.0f), kScaleFactor2);

  DrawImageExpectingIconAndTextLTR(
      *image_1, FloatRect(1000.0f, 2000.0f, 1600.0f, 1200.0f), kScaleFactor1);
}

}  // namespace

}  // namespace blink
