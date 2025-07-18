// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_caret.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "pdf/page_character_index.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_caret_client.h"
#include "pdf/region_data.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

constexpr base::TimeDelta kOneMs = base::Milliseconds(1);

constexpr gfx::Size kCanvasSize(100, 100);
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr PageCharacterIndex kTestChar0{0, 0};

constexpr gfx::Rect kTestChar0ScreenRect{10, 10, 12, 14};
constexpr gfx::Rect kTestChar1ScreenRect{22, 10, 12, 14};
constexpr gfx::Rect kTestChar0Caret{10, 10, 1, 14};
constexpr gfx::Rect kTestChar0EndCaret{22, 10, 1, 14};
constexpr gfx::Rect kTestChar1Caret = kTestChar0EndCaret;

constexpr gfx::Rect kTestMultiPage1Char0ScreenRect{15, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage1Char1ScreenRect{23, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage3Char0ScreenRect{50, 50, 16, 20};
constexpr gfx::Rect kTestMultiPage1Char0Caret{15, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage1Char1Caret{23, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage3Char0Caret{50, 50, 1, 20};
constexpr gfx::Rect kTestMultiPage3Char0EndCaret{66, 50, 1, 20};

class MockTestClient : public PdfCaretClient {
 public:
  MockTestClient() = default;
  MockTestClient(const MockTestClient&) = delete;
  MockTestClient& operator=(const MockTestClient&) = delete;
  ~MockTestClient() override = default;

  const gfx::Rect& invalidated_rect() const { return invalidated_rect_; }

  // PdfCaretClient:
  MOCK_METHOD(uint32_t, GetCharCount, (uint32_t page_index), (const override));

  MOCK_METHOD(std::vector<gfx::Rect>,
              GetScreenRectsForChar,
              (const PageCharacterIndex& index),
              (const override));

  void InvalidateRect(const gfx::Rect& rect) override {
    invalidated_rect_ = rect;
  }

 private:
  gfx::Rect invalidated_rect_;
};

class PdfCaretTest : public testing::Test {
 public:
  PdfCaretTest() = default;
  PdfCaretTest(const PdfCaretTest&) = delete;
  PdfCaretTest& operator=(const PdfCaretTest&) = delete;
  ~PdfCaretTest() override = default;

  MockTestClient& client() { return client_; }

  PdfCaret& caret() { return *caret_; }

  void SetUp() override {
    ResetBitmap();
  }

  void InitializeCaretAtChar(const PageCharacterIndex& index) {
    caret_ = std::make_unique<PdfCaret>(&client_, index);
  }

  RegionData GetRegionData(const gfx::Point& location) {
    uint8_t* buffer = static_cast<uint8_t*>(bitmap_.getPixels());
    CHECK(buffer);

    size_t stride = bitmap_.rowBytes();
    size_t offset = location.y() * stride + location.x() * 4;
    // SAFETY: Skia guarantees bitmap_.height() * bitmap_.rowBytes() is the
    // exact size of the allocated pixel buffer, including row padding. However,
    // Skia does not have a span-based API for this.
    // TODO(crbug.com/357905831): Switch to SkSpan when possible.
    UNSAFE_BUFFERS(
        base::span<uint8_t> buffer_span(buffer, bitmap_.height() * stride));
    return RegionData(buffer_span.subspan(offset), stride);
  }

  void TestDrawCaret(const gfx::Rect& expected_caret) {
    EXPECT_EQ(expected_caret, client().invalidated_rect());
    EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(expected_caret.origin()),
                                       expected_caret));
    EXPECT_TRUE(VerifyCaretRendering(expected_caret));

    // Reset for future calls.
    ResetBitmap();
  }

  void TestDrawCaretFails(const gfx::Rect& expected_caret) {
    EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(expected_caret.origin()),
                                        expected_caret));
    EXPECT_TRUE(VerifyBlankRendering());

    // Reset for future calls.
    ResetBitmap();
  }

  bool VerifyCaretRendering(const gfx::Rect& expected_caret) {
    int width = bitmap_.width();
    int height = bitmap_.height();

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        if (expected_caret.Contains(x, y) ==
            (bitmap_.getColor(x, y) == kDefaultColor)) {
          return false;
        }
      }
    }

    return true;
  }

  bool VerifyBlankRendering() {
    int width = bitmap_.width();
    int height = bitmap_.height();

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        if (bitmap_.getColor(x, y) != kDefaultColor) {
          return false;
        }
      }
    }

    return true;
  }

  void ResetBitmap() {
    bitmap_.reset();

    sk_sp<SkSurface> surface =
        CreateSkiaSurfaceForTesting(kCanvasSize, kDefaultColor);
    SkImageInfo image_info =
        SkImageInfo::MakeN32Premul(kCanvasSize.width(), kCanvasSize.height());
    CHECK(bitmap_.tryAllocPixels(image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    CHECK(image);
    CHECK(image->readPixels(bitmap_.info(), bitmap_.getPixels(),
                            bitmap_.rowBytes(), 0, 0));
  }

  void SetUpChar(const PageCharacterIndex& index,
                 uint32_t unicode_char,
                 std::vector<gfx::Rect> rects) {
    EXPECT_CALL(client_, GetScreenRectsForChar(index))
        .WillRepeatedly(Return(std::move(rects)));
  }

  void SetUpMultiPageTest() {
    EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
    EXPECT_CALL(client(), GetCharCount(1)).WillRepeatedly(Return(2));
    EXPECT_CALL(client(), GetCharCount(2)).WillRepeatedly(Return(0));
    EXPECT_CALL(client(), GetCharCount(3)).WillRepeatedly(Return(1));
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar({1, 0}, 'b', {kTestMultiPage1Char0ScreenRect});
    SetUpChar({1, 1}, 'c', {kTestMultiPage1Char1ScreenRect});
    SetUpChar({3, 0}, 'd', {kTestMultiPage3Char0ScreenRect});
  }

 private:
  StrictMock<MockTestClient> client_;
  std::unique_ptr<PdfCaret> caret_;
  SkBitmap bitmap_;
};

TEST_F(PdfCaretTest, SetVisibility) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(false);

  TestDrawCaretFails(kTestChar0Caret);

  caret().SetVisibility(true);

  TestDrawCaret(kTestChar0Caret);

  caret().SetVisibility(false);
  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetBlinkIntervalWhileNotVisible) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(false);
  TestDrawCaretFails(kTestChar0Caret);

  // Blinks by default, but not visible.
  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);

  // Turn off blinking. Still not visible.
  caret().SetBlinkInterval(base::TimeDelta());

  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);

  // Turn on blinking. Still not visible.
  constexpr base::TimeDelta kBlinkInterval = base::Milliseconds(200);
  caret().SetBlinkInterval(kBlinkInterval);

  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetBlinkIntervalWhileVisible) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(true);

  TestDrawCaret(kTestChar0Caret);

  // Blinks by default.
  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);

  // Turn off blinking. Caret should always be visible.
  caret().SetBlinkInterval(base::TimeDelta());

  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaret(kTestChar0Caret);

  // Turn on blinking.
  constexpr base::TimeDelta kBlinkInterval = base::Milliseconds(300);
  caret().SetBlinkInterval(kBlinkInterval);

  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetBlinkIntervalNegative) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(true);

  // Setting blink interval to negative does nothing.
  caret().SetBlinkInterval(base::Milliseconds(-100));

  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(base::Milliseconds(100));
  TestDrawCaretFails(kTestChar0Caret);
}

TEST_F(PdfCaretTest, MaybeDrawCaret) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  // Not yet visible.
  EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(kTestChar0Caret.origin()),
                                      kTestChar0Caret));

  caret().SetVisibility(true);

  // Not dirty in screen.
  EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(gfx::Point(70, 70)),
                                      gfx::Rect(70, 70, 20, 30)));

  // Partially dirty in screen. For testing purposes, origin is bottom left
  // instead of top right.
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point(5, 5)),
                                     gfx::Rect(5, 5, 20, 30)));
  VerifyCaretRendering(gfx::Rect(5, 5, 1, 9));
  ResetBitmap();

  // Fully dirty in screen.
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(kTestChar0Caret.origin()),
                                     kTestChar0Caret));
  VerifyCaretRendering(kTestChar0Caret);
}

TEST_F(PdfCaretTest, Blink) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(2));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(true);
  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval -
                                            kOneMs);
  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);
  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval -
                                            kOneMs);
  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);
  TestDrawCaret(kTestChar0Caret);

  // Moving to another char should reset the blink duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);

  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  caret().SetChar({0, 1});
  TestDrawCaret(kTestChar1Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval -
                                            kOneMs);
  TestDrawCaret(kTestChar1Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);
  TestDrawCaretFails(kTestChar1Caret);

  // Moving to another char should make the caret reappear immediately.
  caret().SetChar(kTestChar0);
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, OnGeometryChanged) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(1));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);

  EXPECT_EQ(gfx::Rect(), client().invalidated_rect());

  caret().SetVisibility(true);

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  // Simulate a 200% zoom geometry change.
  constexpr gfx::Rect kZoomedCaret{20, 20, 1, 28};
  SetUpChar(kTestChar0, 'a', {kZoomedCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kZoomedCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kZoomedCaret));

  ResetBitmap();

  // Simulate a scroll geometry change.
  constexpr gfx::Rect kZoomedScrolledCaret{40, 60, 1, 28};
  SetUpChar(kTestChar0, 'a', {kZoomedScrolledCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kZoomedScrolledCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kZoomedScrolledCaret));

  ResetBitmap();

  // Simulate a scroll geometry change such that the caret is off-screen.
  constexpr gfx::Rect kOffScreenCaret{140, 160, 1, 28};
  SetUpChar(kTestChar0, 'a', {kOffScreenCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kOffScreenCaret, client().invalidated_rect());
  EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                      gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyBlankRendering());
}

TEST_F(PdfCaretTest, SetPosition) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(2));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  // Set up second char two pixels to the right of the first char.
  SetUpChar({0, 1}, 'b', {gfx::Rect(24, 10, 12, 14)});
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  caret().SetChar(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  caret().SetChar({0, 1});
  TestDrawCaret(gfx::Rect(24, 10, 1, 14));

  constexpr gfx::Rect kSecondCharEndCaret{36, 10, 1, 14};
  caret().SetChar({0, 2});
  TestDrawCaret(kSecondCharEndCaret);

  // Setting the position should still work, even when not visible. The effects
  // will only appear when the caret is set to visible again.
  caret().SetVisibility(false);
  caret().SetChar(kTestChar0);
  EXPECT_EQ(kSecondCharEndCaret, client().invalidated_rect());

  caret().SetVisibility(true);
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetPositionSpecialChars) {
  EXPECT_CALL(client(), GetCharCount(0)).WillRepeatedly(Return(4));
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  caret().SetChar(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  // Synthetic whitespaces and newlines added by PDFium do not have screen
  // rects. Caret should be directly to the right of the first char's rect.
  SetUpChar({0, 1}, ' ', {});
  caret().SetChar({0, 1});
  TestDrawCaret(kTestChar1Caret);

  // Consecutive chars with empty screen rects should still use the right of the
  // previous char's rect.
  SetUpChar({0, 2}, '\n', {});
  caret().SetChar({0, 2});
  TestDrawCaret(kTestChar1Caret);

  // Char with different width and height after newline.
  SetUpChar({0, 3}, 'b', {gfx::Rect(10, 26, 10, 8)});
  caret().SetChar({0, 3});
  TestDrawCaret(gfx::Rect{10, 26, 1, 8});
}

TEST_F(PdfCaretTest, SetPositionMultiPage) {
  SetUpMultiPageTest();
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  caret().SetChar(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  caret().SetChar({3, 0});
  TestDrawCaret(kTestMultiPage3Char0Caret);

  caret().SetChar({3, 1});
  TestDrawCaret(kTestMultiPage3Char0EndCaret);

  caret().SetChar({1, 1});
  TestDrawCaret(kTestMultiPage1Char1Caret);

  caret().SetChar({1, 0});
  TestDrawCaret(kTestMultiPage1Char0Caret);
}

}  // namespace

}  // namespace chrome_pdf
