// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_caret.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "pdf/accessibility_structs.h"
#include "pdf/page_character_index.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_caret_client.h"
#include "pdf/region_data.h"
#include "pdf/test/mock_pdf_caret_client.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kCaretFirstVisibleHistogram[] = "PDF.Caret.FirstVisible";

constexpr base::TimeDelta kOneMs = base::Milliseconds(1);

constexpr gfx::Size kCanvasSize(100, 100);
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr PageCharacterIndex kTestChar0{0, 0};
constexpr PageCharacterIndex kTestChar1{0, 1};
constexpr PageCharacterIndex kTestChar2{0, 2};
constexpr PageCharacterIndex kTestChar3{0, 3};
constexpr PageCharacterIndex kTestChar4{0, 4};
constexpr PageCharacterIndex kTestChar5{0, 5};
constexpr PageCharacterIndex kTestPage1Char0{1, 0};
constexpr PageCharacterIndex kTestPage1Char1{1, 1};
constexpr PageCharacterIndex kTestPage2Char0{2, 0};
constexpr PageCharacterIndex kTestPage3Char0{3, 0};

constexpr gfx::Rect kDefaultScreenRect{10, 10, 12, 12};
constexpr gfx::Rect kDefaultCaret{10, 10, 1, 12};
constexpr gfx::Rect kTestChar0ScreenRect{10, 10, 12, 14};
constexpr gfx::Rect kTestChar1ScreenRect{22, 10, 12, 14};
constexpr gfx::Rect kTestChar0Caret{10, 10, 1, 14};
constexpr gfx::Rect kTestChar0EndCaret{22, 10, 1, 14};
constexpr gfx::Rect kTestChar1Caret = kTestChar0EndCaret;
constexpr gfx::Rect kTestChar1EndCaret{34, 10, 1, 14};
constexpr gfx::Rect kTestChar0ZoomedCaret{20, 20, 1, 28};

constexpr gfx::Rect kTestChar0TopCaret{10, 10, 12, 1};
constexpr gfx::Rect kTestChar0BottomCaret{10, 24, 12, 1};

constexpr gfx::Rect kTestMultiPage1Char0ScreenRect{15, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage1Char1ScreenRect{23, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage2NonTextScreenRect{40, 40, 1, 12};
constexpr gfx::Rect kTestMultiPage3Char0ScreenRect{50, 50, 16, 20};
constexpr gfx::Rect kTestMultiPage1Char0Caret{15, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage1Char1Caret{23, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage1Char1EndCaret{31, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage3Char0Caret{50, 50, 1, 20};
constexpr gfx::Rect kTestMultiPage3Char0EndCaret{66, 50, 1, 20};

AccessibilityTextRunInfo GenerateTestTextRunInfo(
    AccessibilityTextDirection direction) {
  // `PdfCaret` only uses the direction of the text run.
  AccessibilityTextRunInfo text_run;
  text_run.direction = direction;
  return text_run;
}

class PdfCaretTest : public testing::Test {
 public:
  PdfCaretTest() = default;
  PdfCaretTest(const PdfCaretTest&) = delete;
  PdfCaretTest& operator=(const PdfCaretTest&) = delete;
  ~PdfCaretTest() override = default;

  MockPdfCaretClient& client() { return client_; }

  PdfCaret& caret() { return *caret_; }

  void SetUp() override {
    ResetBitmap();
    EXPECT_CALL(client(), GetCurrentOrientation())
        .WillRepeatedly(Return(PageOrientation::kOriginal));
    EXPECT_CALL(client(), GetTextRunInfoAt(_))
        .WillRepeatedly(Return(
            GenerateTestTextRunInfo(AccessibilityTextDirection::kLeftToRight)));
    EXPECT_CALL(client(), IsSelecting()).WillRepeatedly(Return(false));
    EXPECT_CALL(client(), ScrollToChar(_)).Times(AnyNumber());
  }

  void InitializeCaretAtChar(const PageCharacterIndex& index) {
    caret_ = std::make_unique<PdfCaret>(&client_);
    caret_->SetChar(index);
  }

  void InitializeVisibleCaretAtChar(const PageCharacterIndex& index) {
    InitializeCaretAtChar(index);
    caret_->SetEnabled(true);
    caret_->SetVisible(true);
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
    EXPECT_CALL(client_, GetScreenRectsForCaret(index))
        .WillRepeatedly(Return(std::move(rects)));
  }

  void SetUpPagesWithCharCounts(const std::vector<uint32_t>& char_counts) {
    EXPECT_CALL(client(), PageIndexInBounds(_)).WillRepeatedly(Return(false));
    for (size_t i = 0; i < char_counts.size(); ++i) {
      EXPECT_CALL(client(), PageIndexInBounds(i)).WillRepeatedly(Return(true));
      EXPECT_CALL(client(), GetCharCount(i))
          .WillRepeatedly(Return(char_counts[i]));
    }
  }

  void SetUpSingleCharLineTest() {
    SetUpPagesWithCharCounts({1});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  }

  void SetUpTwoCharLineTest() {
    SetUpPagesWithCharCounts({2});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar(kTestChar1, 'b', {kTestChar1ScreenRect});
  }

  void SetUpNoTextPageTest() {
    SetUpPagesWithCharCounts({0});
    SetUpChar(kTestChar0, '\0', {kDefaultScreenRect});
  }

  void SetUpMultiPageTest() {
    SetUpPagesWithCharCounts({1, 2, 0, 1});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar(kTestPage1Char0, 'b', {kTestMultiPage1Char0ScreenRect});
    SetUpChar(kTestPage1Char1, 'c', {kTestMultiPage1Char1ScreenRect});
    SetUpChar(kTestPage2Char0, '\0', {kTestMultiPage2NonTextScreenRect});
    SetUpChar(kTestPage3Char0, 'd', {kTestMultiPage3Char0ScreenRect});
  }

  blink::WebKeyboardEvent GenerateKeyboardEvent(ui::KeyboardCode key) {
    blink::WebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown, 0,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = key;
    return event;
  }

 private:
  StrictMock<MockPdfCaretClient> client_;
  std::unique_ptr<PdfCaret> caret_;
  SkBitmap bitmap_;
};

TEST_F(PdfCaretTest, NoTextPage) {
  base::HistogramTester histograms;
  SetUpNoTextPageTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  TestDrawCaret(kDefaultCaret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);
}

TEST_F(PdfCaretTest, SetEnabled) {
  base::HistogramTester histograms;
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisible(true);

  // Default disabled.
  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectTotalCount(kCaretFirstVisibleHistogram, 0);

  caret().SetEnabled(true);

  TestDrawCaret(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);

  caret().SetEnabled(false);

  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);

  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);

  caret().SetEnabled(true);

  TestDrawCaret(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);
}

TEST_F(PdfCaretTest, SetVisible) {
  base::HistogramTester histograms;
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  caret().SetEnabled(true);

  // Default not visible.
  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectTotalCount(kCaretFirstVisibleHistogram, 0);

  caret().SetVisible(true);

  TestDrawCaret(kTestChar0Caret);

  caret().SetVisible(false);

  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);

  TestDrawCaretFails(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);

  caret().SetVisible(true);

  TestDrawCaret(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);
}

TEST_F(PdfCaretTest, SetBlinkIntervalWhileNotVisible) {
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

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
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

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

  GetPdfTestTaskEnvironment().FastForwardBy(kBlinkInterval - kOneMs);
  TestDrawCaretFails(kTestChar0Caret);

  // Set to the same blink interval. Should not reset the blink timer.
  caret().SetBlinkInterval(kBlinkInterval);

  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetBlinkIntervalNegative) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  // Setting blink interval to negative does nothing.
  caret().SetBlinkInterval(base::Milliseconds(-100));

  TestDrawCaret(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval);
  TestDrawCaretFails(kTestChar0Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(base::Milliseconds(100));
  TestDrawCaretFails(kTestChar0Caret);
}

TEST_F(PdfCaretTest, MaybeDrawCaret) {
  base::HistogramTester histograms;
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  // Not yet visible.
  EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(kTestChar0Caret.origin()),
                                      kTestChar0Caret));
  histograms.ExpectTotalCount(kCaretFirstVisibleHistogram, 0);

  caret().SetEnabled(true);
  caret().SetVisible(true);

  // Not dirty in screen.
  EXPECT_FALSE(caret().MaybeDrawCaret(GetRegionData(gfx::Point(70, 70)),
                                      gfx::Rect(70, 70, 20, 30)));
  histograms.ExpectTotalCount(kCaretFirstVisibleHistogram, 0);

  // Partially dirty in screen. For testing purposes, origin is bottom left
  // instead of top right.
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point(5, 5)),
                                     gfx::Rect(5, 5, 20, 30)));
  VerifyCaretRendering(gfx::Rect(5, 5, 1, 9));
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);
  ResetBitmap();

  // Fully dirty in screen.
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(kTestChar0Caret.origin()),
                                     kTestChar0Caret));
  VerifyCaretRendering(kTestChar0Caret);
  histograms.ExpectUniqueSample(kCaretFirstVisibleHistogram, true, 1);
}

TEST_F(PdfCaretTest, Blink) {
  SetUpTwoCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

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

  caret().SetCharAndDraw(kTestChar1);
  TestDrawCaret(kTestChar1Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(PdfCaret::kDefaultBlinkInterval -
                                            kOneMs);
  TestDrawCaret(kTestChar1Caret);

  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);
  TestDrawCaretFails(kTestChar1Caret);

  // Moving to another char should make the caret reappear immediately.
  caret().SetCharAndDraw(kTestChar0);
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, OnGeometryChanged) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  // Simulate a 200% zoom geometry change.
  SetUpChar(kTestChar0, 'a', {kTestChar0ZoomedCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0ZoomedCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kTestChar0ZoomedCaret));

  ResetBitmap();

  // Simulate a scroll geometry change.
  constexpr gfx::Rect kTestChar0ZoomedScrolledCaret{40, 60, 1, 28};
  SetUpChar(kTestChar0, 'a', {kTestChar0ZoomedScrolledCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0ZoomedScrolledCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kTestChar0ZoomedScrolledCaret));

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

TEST_F(PdfCaretTest, OnGeometryChangedToggleEnabled) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetEnabled(false);

  // Call `OnGeometryChanged()` while the caret is disabled.
  SetUpChar(kTestChar0, 'a', {kTestChar0ZoomedCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetEnabled(true);

  EXPECT_EQ(kTestChar0ZoomedCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kTestChar0ZoomedCaret));
}

TEST_F(PdfCaretTest, OnGeometryChangedToggleVisible) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetVisible(false);

  // Call `OnGeometryChanged()` while the caret is not visible.
  SetUpChar(kTestChar0, 'a', {kTestChar0ZoomedCaret});
  caret().OnGeometryChanged();

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetVisible(true);

  EXPECT_EQ(kTestChar0ZoomedCaret, client().invalidated_rect());
  EXPECT_TRUE(caret().MaybeDrawCaret(GetRegionData(gfx::Point()),
                                     gfx::Rect(kCanvasSize)));
  EXPECT_TRUE(VerifyCaretRendering(kTestChar0ZoomedCaret));
}

TEST_F(PdfCaretTest, SetChar) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetChar(kTestChar1);

  // New caret position should not be invalidated.
  EXPECT_EQ(kTestChar0Caret, client().invalidated_rect());

  caret().SetChar(kTestChar0);

  // Old caret position should be invalidated.
  EXPECT_EQ(kTestChar0EndCaret, client().invalidated_rect());
}

TEST_F(PdfCaretTest, SetCharAndDraw) {
  SetUpPagesWithCharCounts({2});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  // Set up second char two pixels to the right of the first char.
  SetUpChar(kTestChar1, 'b', {gfx::Rect(24, 10, 12, 14)});
  InitializeVisibleCaretAtChar(kTestChar0);

  caret().SetCharAndDraw(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  caret().SetCharAndDraw(kTestChar1);
  TestDrawCaret(gfx::Rect(24, 10, 1, 14));

  constexpr gfx::Rect kSecondCharEndCaret{36, 10, 1, 14};
  caret().SetCharAndDraw(kTestChar2);
  TestDrawCaret(kSecondCharEndCaret);

  // Setting the position should still work, even when not visible. The effects
  // will only appear when the caret is set to visible again.
  caret().SetEnabled(false);
  caret().SetCharAndDraw(kTestChar0);
  EXPECT_EQ(kSecondCharEndCaret, client().invalidated_rect());

  caret().SetEnabled(true);
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, SetCharAndDrawSpecialChars) {
  SetUpPagesWithCharCounts({4});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, ' ', {});
  SetUpChar(kTestChar2, '\n', {});
  SetUpChar(kTestChar3, 'b', {gfx::Rect(10, 26, 10, 8)});
  InitializeVisibleCaretAtChar(kTestChar0);

  caret().SetCharAndDraw(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  // Synthetic whitespaces and newlines added by PDFium do not have screen
  // rects. Caret should be directly to the right of the first char's rect.
  caret().SetCharAndDraw(kTestChar1);
  TestDrawCaret(kTestChar1Caret);

  // Consecutive chars with empty screen rects should still use the right of the
  // previous char's rect.
  caret().SetCharAndDraw(kTestChar2);
  TestDrawCaret(kTestChar1Caret);

  // Char with different width and height after newline.
  caret().SetCharAndDraw(kTestChar3);
  TestDrawCaret(gfx::Rect{10, 26, 1, 8});
}

TEST_F(PdfCaretTest, SetCharAndDrawMultiPage) {
  SetUpMultiPageTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  caret().SetCharAndDraw(kTestChar0);
  TestDrawCaret(kTestChar0Caret);

  caret().SetCharAndDraw(kTestPage3Char0);
  TestDrawCaret(kTestMultiPage3Char0Caret);

  caret().SetCharAndDraw({3, 1});
  TestDrawCaret(kTestMultiPage3Char0EndCaret);

  caret().SetCharAndDraw(kTestPage1Char1);
  TestDrawCaret(kTestMultiPage1Char1Caret);

  caret().SetCharAndDraw(kTestPage1Char0);
  TestDrawCaret(kTestMultiPage1Char0Caret);
}

class PdfCaretTextDirectionTest : public PdfCaretTest {
 public:
  void SetUpTextDirectionTest(const PageCharacterIndex& start_index,
                              AccessibilityTextDirection direction) {
    SetUpPagesWithCharCounts({2});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar(kTestChar1, '\n', {});
    EXPECT_CALL(client(), GetTextRunInfoAt(_))
        .WillRepeatedly(Return(GenerateTestTextRunInfo(direction)));
    InitializeVisibleCaretAtChar(start_index);
  }

  void TestLeftToRight() {
    InSequence sequence;
    TestOrientation(PageOrientation::kOriginal, kTestChar0Caret);
    TestOrientation(PageOrientation::kClockwise90, kTestChar0TopCaret);
    TestOrientation(PageOrientation::kClockwise180, kTestChar0EndCaret);
    TestOrientation(PageOrientation::kClockwise270, kTestChar0BottomCaret);
  }

  void TestRightToLeft() {
    InSequence sequence;
    TestOrientation(PageOrientation::kOriginal, kTestChar0EndCaret);
    TestOrientation(PageOrientation::kClockwise90, kTestChar0BottomCaret);
    TestOrientation(PageOrientation::kClockwise180, kTestChar0Caret);
    TestOrientation(PageOrientation::kClockwise270, kTestChar0TopCaret);
  }

  void TestTopToBottom() {
    InSequence sequence;
    TestOrientation(PageOrientation::kOriginal, kTestChar0TopCaret);
    TestOrientation(PageOrientation::kClockwise90, kTestChar0EndCaret);
    TestOrientation(PageOrientation::kClockwise180, kTestChar0BottomCaret);
    TestOrientation(PageOrientation::kClockwise270, kTestChar0Caret);
  }

  void TestBottomToTop() {
    InSequence sequence;
    TestOrientation(PageOrientation::kOriginal, kTestChar0BottomCaret);
    TestOrientation(PageOrientation::kClockwise90, kTestChar0Caret);
    TestOrientation(PageOrientation::kClockwise180, kTestChar0TopCaret);
    TestOrientation(PageOrientation::kClockwise270, kTestChar0EndCaret);
  }

  void TestOrientation(PageOrientation orientation, gfx::Rect expected_caret) {
    EXPECT_CALL(client(), GetCurrentOrientation())
        .WillOnce(Return(orientation));
    caret().OnGeometryChanged();
    TestDrawCaret(expected_caret);
  }
};

TEST_F(PdfCaretTextDirectionTest, NoTextPage) {
  SetUpNoTextPageTest();
  EXPECT_CALL(client(), GetTextRunInfoAt(_))
      .WillRepeatedly(Return(std::nullopt));
  InitializeVisibleCaretAtChar(kTestChar0);

  InSequence sequence;
  TestOrientation(PageOrientation::kOriginal, kDefaultCaret);
  TestOrientation(PageOrientation::kClockwise90, kTestChar0TopCaret);
  TestOrientation(PageOrientation::kClockwise180, gfx::Rect(22, 10, 1, 12));
  TestOrientation(PageOrientation::kClockwise270, gfx::Rect(10, 22, 12, 1));
}

TEST_F(PdfCaretTextDirectionTest, LeftToRight) {
  SetUpTextDirectionTest(kTestChar0, AccessibilityTextDirection::kLeftToRight);
  TestLeftToRight();
}

TEST_F(PdfCaretTextDirectionTest, RightToLeft) {
  SetUpTextDirectionTest(kTestChar0, AccessibilityTextDirection::kRightToLeft);
  TestRightToLeft();
}

TEST_F(PdfCaretTextDirectionTest, TopToBottom) {
  SetUpTextDirectionTest(kTestChar0, AccessibilityTextDirection::kTopToBottom);
  TestTopToBottom();
}

TEST_F(PdfCaretTextDirectionTest, BottomToTop) {
  SetUpTextDirectionTest(kTestChar0, AccessibilityTextDirection::kBottomToTop);
  TestBottomToTop();
}

TEST_F(PdfCaretTextDirectionTest, LeftToRightEmptyScreenRect) {
  SetUpTextDirectionTest(kTestChar1, AccessibilityTextDirection::kLeftToRight);
  // When on a character with an empty screen rect, the previous character's
  // screen rect is used, but flipped.
  TestRightToLeft();
}

TEST_F(PdfCaretTextDirectionTest, RightToLeftEmptyScreenRect) {
  SetUpTextDirectionTest(kTestChar1, AccessibilityTextDirection::kRightToLeft);
  // When on a character with an empty screen rect, the previous character's
  // screen rect is used, but flipped.
  TestLeftToRight();
}

TEST_F(PdfCaretTextDirectionTest, TopToBottomEmptyScreenRect) {
  SetUpTextDirectionTest(kTestChar1, AccessibilityTextDirection::kTopToBottom);
  // When on a character with an empty screen rect, the previous character's
  // screen rect is used, but flipped.
  TestBottomToTop();
}

TEST_F(PdfCaretTextDirectionTest, BottomToTopEmptyScreenRect) {
  SetUpTextDirectionTest(kTestChar1, AccessibilityTextDirection::kBottomToTop);
  // When on a character with an empty screen rect, the previous character's
  // screen rect is used, but flipped.
  TestTopToBottom();
}

class PdfCaretMoveTest : public PdfCaretTest {
 public:
  void SetUp() override {
    PdfCaretTest::SetUp();
    EXPECT_CALL(client(), IsSynthesizedNewline(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(client(), ClearTextSelection()).Times(AnyNumber());
  }

  void SetUpPagesWithSynthesizedChars(
      const std::vector<std::vector<uint32_t>>& synthesized_chars) {
    for (size_t page_index = 0; page_index < synthesized_chars.size();
         ++page_index) {
      for (uint32_t synthesized_char : synthesized_chars[page_index]) {
        PageCharacterIndex index{static_cast<uint32_t>(page_index),
                                 synthesized_char};
        EXPECT_CALL(client(), IsSynthesizedNewline(index))
            .WillRepeatedly(Return(true));
      }
    }
  }

  void SetUpMultiLineTest() {
    SetUpPagesWithCharCounts({10});
    SetUpPagesWithSynthesizedChars({{2, 3, 6, 7}});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar(kTestChar1, 'b', {kTestChar1ScreenRect});
    SetUpChar(kTestChar2, '\r', {});
    SetUpChar(kTestChar3, '\n', {});
    SetUpChar(kTestChar4, 'c', {gfx::Rect(11, 26, 10, 12)});
    SetUpChar(kTestChar5, 'd', {gfx::Rect(21, 26, 10, 12)});
    SetUpChar({0, 6}, '\r', {});
    SetUpChar({0, 7}, '\n', {});
    SetUpChar({0, 8}, 'e', {gfx::Rect(10, 50, 14, 16)});
    SetUpChar({0, 9}, 'f', {gfx::Rect(24, 50, 14, 16)});
  }
};

TEST_F(PdfCaretMoveTest, OnKeyDownNotEnabledNotVisible) {
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_0)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretMoveTest, OnKeyDownNotEnabledVisible) {
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisible(true);

  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_0)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretMoveTest, OnKeyDownEnabledNotVisible) {
  SetUpSingleCharLineTest();
  InitializeCaretAtChar(kTestChar0);

  caret().SetEnabled(true);

  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_0)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretMoveTest, OnKeyDownEnabledVisible) {
  SetUpSingleCharLineTest();
  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_FALSE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_0)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRight) {
  SetUpTwoCharLineTest();

  // Start at left of char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Left of char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1Caret);

  // Right of char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1EndCaret);

  // Right of char 1.
  EXPECT_CALL(client(), IsSynthesizedNewline(kTestChar2)).Times(0);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1EndCaret);

  // Left of char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar1Caret);

  // Left of char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0Caret);

  // Left of char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRightMultiPage) {
  SetUpMultiPageTest();

  // Start at left of page 1, char 0.
  InitializeVisibleCaretAtChar(kTestPage1Char0);

  // Right of page 0, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0EndCaret);

  // Left of page 1, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestMultiPage1Char0Caret);

  // Left of page 1, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestMultiPage1Char1Caret);

  // Right of page 1, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestMultiPage1Char1EndCaret);

  // Top-left of page 2. Page 2 does not have any chars.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestMultiPage2NonTextScreenRect);

  // Left of page 3, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestMultiPage3Char0Caret);

  // Top-left of page 2.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestMultiPage2NonTextScreenRect);

  // Right of page 1, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestMultiPage1Char1EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRightSkipNewlines) {
  SetUpPagesWithCharCounts({4});
  SetUpPagesWithSynthesizedChars({{1, 2}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\r', {});
  SetUpChar(kTestChar2, '\n', {});
  SetUpChar(kTestChar3, 'b', {gfx::Rect(10, 26, 12, 14)});

  // Start at left of page 0, char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Right of page 0, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar0EndCaret);

  // Left of page 0, char 3 'b', skipping one newline.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(gfx::Rect(10, 26, 1, 14));

  // Right of page 0, char 0, skipping one newline.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRightStartEndNewlines) {
  SetUpPagesWithCharCounts({2});
  SetUpChar(kTestChar0, '\n', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\n', {kTestChar1ScreenRect});

  // Start at left of page 0, char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Left of page 0, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1Caret);

  // Right of page 0, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1EndCaret);

  // Left of page 0, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar1Caret);

  // Left of page 0, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRightConsecutiveNewlines) {
  SetUpPagesWithCharCounts({5});
  SetUpPagesWithSynthesizedChars({{1, 2}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\r', {});
  SetUpChar(kTestChar2, '\n', {});
  SetUpChar(kTestChar3, '\n', {gfx::Rect(10, 26, 12, 14)});
  SetUpChar(kTestChar4, 'b', {gfx::Rect(22, 26, 12, 14)});

  // Start at left of page 0, char 1.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Left of page 0, char 3 '\n', skipping one newline.
  constexpr gfx::Rect kTestChar3Caret{10, 26, 1, 14};
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar3Caret);

  // Left of page 0, char 4 'b'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(gfx::Rect(22, 26, 1, 14));

  // Left of page 0, char 3.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar3Caret);

  // Right of page 0, char 0, skipping one newline.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharLeftRightSingleSyntheticNewline) {
  SetUpPagesWithCharCounts({3});
  SetUpPagesWithSynthesizedChars({{1}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\n', {});
  SetUpChar(kTestChar2, 'b', {gfx::Rect(10, 26, 12, 14)});

  // Start at left of page 0, char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Right of page 0, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar0EndCaret);

  // Left of page 0, char 2 'b'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(gfx::Rect(10, 26, 1, 14));

  // Right of page 0, char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDown) {
  SetUpPagesWithCharCounts({4});
  SetUpPagesWithSynthesizedChars({{1, 2}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\r', {});
  SetUpChar(kTestChar2, '\n', {});
  SetUpChar(kTestChar3, 'b', {gfx::Rect(10, 24, 12, 14)});

  // Start at left of char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Left of char 3 'b'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(10, 24, 1, 14));

  // Left of char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownNonTextPage) {
  SetUpNoTextPageTest();

  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kDefaultCaret);

  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kDefaultCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownSingleLine) {
  SetUpTwoCharLineTest();

  // Start at right of char 0.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Left of char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);

  // No change.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);

  // Right of char 1 'b'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar1EndCaret);

  // No change.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar1EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownMultiLine) {
  SetUpMultiLineTest();

  // Start at left of char 1 'b'.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Left of char 5 'd'.
  constexpr gfx::Rect kTestChar5Caret{21, 26, 1, 12};
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar5Caret);

  // Left of char 9 'f'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(24, 50, 1, 16));

  // Left of char 5.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar5Caret);

  // Left of char 1.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar1Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownStartOnNewline) {
  SetUpPagesWithCharCounts({6});
  SetUpPagesWithSynthesizedChars({{2, 3}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, 'b', {kTestChar1ScreenRect});
  SetUpChar(kTestChar2, '\r', {});
  SetUpChar(kTestChar3, '\n', {});
  SetUpChar(kTestChar4, 'c', {gfx::Rect(10, 22, 12, 14)});
  SetUpChar(kTestChar5, 'd', {gfx::Rect(22, 22, 12, 14)});

  // Start at right of char 1 '\r'.
  InitializeVisibleCaretAtChar(kTestChar2);

  TestDrawCaret(kTestChar1EndCaret);

  // Right of char 5 'd'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(34, 22, 1, 14));

  // Reset back to right of char 1.
  caret().SetCharAndDraw(kTestChar2);
  TestDrawCaret(kTestChar1EndCaret);

  // Left of char 0 'a'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 0, char 0 'a'.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Left of page 1, char 1 'c', which is closer than the right.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestMultiPage1Char1Caret);

  // Top-left of page 2. Page 2 does not have any chars.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestMultiPage2NonTextScreenRect);

  // Left of page 3, char 0 'd', which is closer than the right.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestMultiPage3Char0Caret);

  // Top-left of page 2.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestMultiPage2NonTextScreenRect);

  // Right of page 1, char 1, which is closer than the left.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestMultiPage1Char1EndCaret);

  // Right of page 0, char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownLongerFirstLine) {
  SetUpPagesWithCharCounts({6});
  SetUpPagesWithSynthesizedChars({{3, 4}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, 'b', {kTestChar1ScreenRect});
  SetUpChar(kTestChar2, 'c', {gfx::Rect(34, 10, 12, 14)});
  SetUpChar(kTestChar3, '\r', {});
  SetUpChar(kTestChar4, '\n', {});
  SetUpChar(kTestChar5, 'd', {gfx::Rect(10, 22, 12, 14)});

  // Start at left of char 2 'c'.
  constexpr gfx::Rect kTestChar2Caret{34, 10, 1, 14};
  InitializeVisibleCaretAtChar(kTestChar2);

  TestDrawCaret(kTestChar2Caret);

  // Move down to char with closest screen rect. Right of char 5 'd'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(22, 22, 1, 14));

  // Move up to char with closest screen rect. Left of char 1 'b'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar1Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownLongerSecondLine) {
  SetUpPagesWithCharCounts({6});
  SetUpPagesWithSynthesizedChars({{1, 2}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar(kTestChar1, '\r', {});
  SetUpChar(kTestChar2, '\n', {});
  SetUpChar(kTestChar3, 'b', {gfx::Rect(10, 22, 12, 14)});
  SetUpChar(kTestChar4, 'c', {gfx::Rect(22, 22, 12, 14)});
  SetUpChar(kTestChar5, 'd', {gfx::Rect(34, 22, 12, 14)});

  // Start at right of char 5 'd'.
  InitializeVisibleCaretAtChar({0, 6});

  TestDrawCaret(gfx::Rect(46, 22, 1, 14));

  // Right of char 0 'a'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0EndCaret);

  // Left of char 4 'c'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(22, 22, 1, 14));
}

TEST_F(PdfCaretMoveTest, MoveCharScroll) {
  SetUpMultiPageTest();
  InitializeVisibleCaretAtChar(kTestPage1Char1);

  InSequence sequence;

  EXPECT_CALL(client(), ScrollToChar(kTestPage1Char0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));

  EXPECT_CALL(client(), ScrollToChar(kTestPage1Char1));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  // No-text page.
  EXPECT_CALL(client(), ScrollToChar(kTestPage2Char0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  EXPECT_CALL(client(), ScrollToChar(kTestPage1Char1));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  caret().SetCharAndDraw(kTestPage3Char0);

  // Page 3 char 1 does not have a screen rect, so scroll to the previous char
  // with one.
  EXPECT_CALL(client(), ScrollToChar(kTestPage3Char0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
}

class PdfCaretSelectionTest : public PdfCaretMoveTest {
 public:
  blink::WebKeyboardEvent GenerateShiftKeyboardEvent(ui::KeyboardCode key) {
    blink::WebKeyboardEvent event = GenerateKeyboardEvent(key);
    event.SetModifiers(blink::WebInputEvent::Modifiers::kShiftKey);
    return event;
  }

  void ExpectExtendAndInvalidateSelectionByChar(
      const PageCharacterIndex& index) {
    EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(index))
        .WillOnce([&]() {
          // When text selection is extended, the client will normally hide the
          // caret.
          caret().SetVisible(false);
        });
  }
};

TEST_F(PdfCaretSelectionTest, SelectRight) {
  SetUpTwoCharLineTest();

  // Start at left of char 0.
  InitializeVisibleCaretAtChar(kTestChar0);

  // Move right. Select char 1.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar0));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar1);
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  TestDrawCaretFails(kTestChar1EndCaret);

  // Move right without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1EndCaret);
}

TEST_F(PdfCaretSelectionTest, SelectLeft) {
  SetUpTwoCharLineTest();

  // Start at right of char 1.
  InitializeVisibleCaretAtChar(kTestChar2);

  // Move left. Select char 1.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar2));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar1);
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));

  TestDrawCaretFails(kTestChar1Caret);

  // Move left without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretSelectionTest, SelectDown) {
  SetUpMultiLineTest();

  // Start at left of char 1 'b'.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Move down. Select chars 1, 2, 3, 4.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar1));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 5));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select chars 5, 6, 7, 8.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 9));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select char 9 'f' (end of page).
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 10));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  constexpr gfx::Rect kTestChar9EndCaret{38, 50, 1, 16};
  TestDrawCaretFails(kTestChar9EndCaret);

  // Move down without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar9EndCaret);
}

TEST_F(PdfCaretSelectionTest, SelectUp) {
  SetUpMultiLineTest();

  // Start at left of char 9 'f'.
  constexpr PageCharacterIndex kTestChar9{0, 9};
  InitializeVisibleCaretAtChar(kTestChar9);

  // Move up. Select chars 8, 7, 6, 5.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar9));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 5));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select chars 4, 3, 2, 1.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar1);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select char 0 'a' (start of page).
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar0);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  TestDrawCaretFails(kTestChar0Caret);

  // Move up without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretSelectionTest, SelectStartOnNonTextPageMoveToNonTextPage) {
  SetUpPagesWithCharCounts({0, 0});
  SetUpChar(kTestChar0, '\0', {kDefaultScreenRect});
  SetUpChar(kTestPage1Char0, '\0', {gfx::Rect(10, 50, 1, 12)});

  InitializeVisibleCaretAtChar(kTestChar0);

  // Moving from a no-text page to another no-text page should not start a
  // selection.
  EXPECT_CALL(client(), StartSelection(_)).Times(0);

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
}

TEST_F(PdfCaretSelectionTest, SelectStartOnTextPageMoveToNonTextPages) {
  SetUpPagesWithCharCounts({1, 0, 0});
  SetUpChar(kTestChar0, '\0', {kTestChar0ScreenRect});
  SetUpChar(kTestPage1Char0, '\0', {gfx::Rect(10, 50, 1, 12)});
  SetUpChar(kTestPage2Char0, '\0', {gfx::Rect(10, 100, 1, 12)});

  InitializeVisibleCaretAtChar(kTestChar0);

  // Select page 0, char 0.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar0));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar1);
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  // Moving to multiple no-text pages should not extend selection.
  EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(_)).Times(0);

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
}

TEST_F(PdfCaretSelectionTest, SelectNonTextPage) {
  SetUpNoTextPageTest();

  InitializeVisibleCaretAtChar(kTestChar0);

  EXPECT_CALL(client(), StartSelection(_)).Times(0);
  EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(_)).Times(0);

  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

TEST_F(PdfCaretSelectionTest, SelectStartingOnNonTextPage) {
  SetUpMultiPageTest();

  // Start on the no-text page.
  InitializeVisibleCaretAtChar(kTestPage2Char0);

  // `StartSelection()` should be called on the nearest caret position in the
  // direction of movement. In this case, it would be right of page 1, char 1.
  constexpr PageCharacterIndex kTestPage1Char1End{1, 2};
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestPage1Char1End));
  ExpectExtendAndInvalidateSelectionByChar(kTestPage1Char1End);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

TEST_F(PdfCaretSelectionTest, MoveCaretWithShiftDownMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 0, char 0.
  InitializeVisibleCaretAtChar(kTestChar1);

  // Move down. Select page 1, char 0 'b'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar1));
  ExpectExtendAndInvalidateSelectionByChar(kTestPage1Char1);
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select page 1, char 1 'c'. Caret should be on the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(1, 2));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. The selection should extend past the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 0));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select page 3, char 0 'd'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 1));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretSelectionTest, MoveCaretWithShiftUpMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 3, char 0 'd'.
  InitializeVisibleCaretAtChar({3, 1});

  // Move up. Select page 3, char 0. Caret should be on no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(PageCharacterIndex(3, 1)));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. The selection should extend past the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(PageCharacterIndex(1, 2));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select page 1, char 1 'c' and page 1, char 0 'b'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar1);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select page 0, char 0 'a'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  ExpectExtendAndInvalidateSelectionByChar(kTestChar0);
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

class PdfCaretMoveWithTextDirectionTest : public PdfCaretMoveTest {
 public:
  void SetUpTextDirectionTest(AccessibilityTextDirection direction) {
    // To simplify tests, just use a single character.
    SetUpSingleCharLineTest();
    EXPECT_CALL(client(), GetTextRunInfoAt(_))
        .WillRepeatedly(Return(GenerateTestTextRunInfo(direction)));
    InitializeVisibleCaretAtChar(kTestChar0);
  }

  void TestMove(ui::KeyboardCode key, const gfx::Rect& expected_caret) {
    EXPECT_TRUE(caret().OnKeyDown(GenerateKeyboardEvent(key)));
    TestDrawCaret(expected_caret);
  }
};

TEST_F(PdfCaretMoveWithTextDirectionTest, LeftToRight) {
  SetUpTextDirectionTest(AccessibilityTextDirection::kLeftToRight);
  TestDrawCaret(kTestChar0Caret);
  TestMove(ui::KeyboardCode::VKEY_RIGHT, kTestChar0EndCaret);
  TestMove(ui::KeyboardCode::VKEY_LEFT, kTestChar0Caret);
  TestMove(ui::KeyboardCode::VKEY_DOWN, kTestChar0EndCaret);
  TestMove(ui::KeyboardCode::VKEY_UP, kTestChar0Caret);
}

TEST_F(PdfCaretMoveWithTextDirectionTest, RightToLeft) {
  SetUpTextDirectionTest(AccessibilityTextDirection::kRightToLeft);
  TestDrawCaret(kTestChar0EndCaret);
  TestMove(ui::KeyboardCode::VKEY_LEFT, kTestChar0Caret);
  TestMove(ui::KeyboardCode::VKEY_RIGHT, kTestChar0EndCaret);
  TestMove(ui::KeyboardCode::VKEY_DOWN, kTestChar0Caret);
  TestMove(ui::KeyboardCode::VKEY_UP, kTestChar0EndCaret);
}

TEST_F(PdfCaretMoveWithTextDirectionTest, TopToBottom) {
  SetUpTextDirectionTest(AccessibilityTextDirection::kTopToBottom);
  TestDrawCaret(kTestChar0TopCaret);
  TestMove(ui::KeyboardCode::VKEY_DOWN, kTestChar0BottomCaret);
  TestMove(ui::KeyboardCode::VKEY_UP, kTestChar0TopCaret);
  TestMove(ui::KeyboardCode::VKEY_LEFT, kTestChar0BottomCaret);
  TestMove(ui::KeyboardCode::VKEY_RIGHT, kTestChar0TopCaret);
}

TEST_F(PdfCaretMoveWithTextDirectionTest, BottomToTop) {
  SetUpTextDirectionTest(AccessibilityTextDirection::kBottomToTop);
  TestDrawCaret(kTestChar0BottomCaret);
  TestMove(ui::KeyboardCode::VKEY_UP, kTestChar0TopCaret);
  TestMove(ui::KeyboardCode::VKEY_DOWN, kTestChar0BottomCaret);
  TestMove(ui::KeyboardCode::VKEY_LEFT, kTestChar0TopCaret);
  TestMove(ui::KeyboardCode::VKEY_RIGHT, kTestChar0BottomCaret);
}

}  // namespace

}  // namespace chrome_pdf
