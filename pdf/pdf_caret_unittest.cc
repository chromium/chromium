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
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrictMock;

constexpr base::TimeDelta kOneMs = base::Milliseconds(1);

constexpr gfx::Size kCanvasSize(100, 100);
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr PageCharacterIndex kTestChar0{0, 0};

constexpr gfx::Rect kDefaultCaret{10, 10, 1, 12};
constexpr gfx::Rect kTestChar0ScreenRect{10, 10, 12, 14};
constexpr gfx::Rect kTestChar1ScreenRect{22, 10, 12, 14};
constexpr gfx::Rect kTestChar0Caret{10, 10, 1, 14};
constexpr gfx::Rect kTestChar0EndCaret{22, 10, 1, 14};
constexpr gfx::Rect kTestChar1Caret = kTestChar0EndCaret;
constexpr gfx::Rect kTestChar1EndCaret{34, 10, 1, 14};

constexpr gfx::Rect kTestMultiPage1Char0ScreenRect{15, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage1Char1ScreenRect{23, 15, 8, 4};
constexpr gfx::Rect kTestMultiPage2NonTextScreenRect{40, 40, 1, 12};
constexpr gfx::Rect kTestMultiPage3Char0ScreenRect{50, 50, 16, 20};
constexpr gfx::Rect kTestMultiPage1Char0Caret{15, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage1Char1Caret{23, 15, 1, 4};
constexpr gfx::Rect kTestMultiPage1Char1EndCaret{31, 15, 1, 4};
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
  MOCK_METHOD(void, ClearTextSelection, (), (override));

  MOCK_METHOD(void,
              ExtendAndInvalidateSelectionByChar,
              (const PageCharacterIndex& index),
              (override));

  MOCK_METHOD(uint32_t, GetCharCount, (uint32_t page_index), (const override));

  MOCK_METHOD(std::vector<gfx::Rect>,
              GetScreenRectsForCaret,
              (const PageCharacterIndex& index),
              (const override));

  void InvalidateRect(const gfx::Rect& rect) override {
    invalidated_rect_ = rect;
  }

  MOCK_METHOD(bool, IsSelecting, (), (const override));

  MOCK_METHOD(bool,
              IsSynthesizedNewline,
              (const PageCharacterIndex& index),
              (const override));

  MOCK_METHOD(bool, PageIndexInBounds, (int index), (const override));

  MOCK_METHOD(void,
              StartSelection,
              (const PageCharacterIndex& index),
              (override));

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
    EXPECT_CALL(client(), IsSelecting()).WillRepeatedly(Return(false));
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

  void SetUpMultiPageTest() {
    SetUpPagesWithCharCounts({1, 2, 0, 1});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar({1, 0}, 'b', {kTestMultiPage1Char0ScreenRect});
    SetUpChar({1, 1}, 'c', {kTestMultiPage1Char1ScreenRect});
    SetUpChar({2, 0}, '\0', {kTestMultiPage2NonTextScreenRect});
    SetUpChar({3, 0}, 'd', {kTestMultiPage3Char0ScreenRect});
  }

  blink::WebKeyboardEvent GenerateKeyboardEvent(ui::KeyboardCode key) {
    blink::WebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown, 0,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = key;
    return event;
  }

 private:
  StrictMock<MockTestClient> client_;
  std::unique_ptr<PdfCaret> caret_;
  SkBitmap bitmap_;
};

TEST_F(PdfCaretTest, NonTextPage) {
  SetUpPagesWithCharCounts({0});
  SetUpChar(kTestChar0, '\0', {kDefaultCaret});
  InitializeCaretAtChar(kTestChar0);

  caret().SetVisibility(true);

  TestDrawCaret(kDefaultCaret);
}

TEST_F(PdfCaretTest, SetVisibility) {
  SetUpPagesWithCharCounts({1});
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
  SetUpPagesWithCharCounts({1});
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
  SetUpPagesWithCharCounts({1});
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
  SetUpPagesWithCharCounts({1});
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
  SetUpPagesWithCharCounts({1});
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

TEST_F(PdfCaretTest, CaretNotVisibleWhileSelecting) {
  SetUpPagesWithCharCounts({1});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  TestDrawCaretFails(kTestChar0Caret);

  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretTest, Blink) {
  SetUpPagesWithCharCounts({2});
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
  SetUpPagesWithCharCounts({1});
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

TEST_F(PdfCaretTest, SetChar) {
  SetUpPagesWithCharCounts({2});
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

TEST_F(PdfCaretTest, SetCharSpecialChars) {
  SetUpPagesWithCharCounts({4});
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

TEST_F(PdfCaretTest, SetCharMultiPage) {
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
};

TEST_F(PdfCaretMoveTest, OnKeyDown) {
  SetUpPagesWithCharCounts({1});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar({0, 1}, '\0', {});
  InitializeCaretAtChar(kTestChar0);

  // Relevant key events still handled even when caret is not visible.
  caret().SetVisibility(false);

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

  caret().SetVisibility(true);

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
  SetUpPagesWithCharCounts({2});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});

  // Start at left of char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  // Left of char 1.
  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1Caret);

  // Right of char 1.
  SetUpChar({0, 2}, '\0', {});
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
  TestDrawCaret(kTestChar1EndCaret);

  // Right of char 1.
  PageCharacterIndex kTestLastCaret{0, 2};
  EXPECT_CALL(client(), IsSynthesizedNewline(kTestLastCaret)).Times(0);
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
  InitializeCaretAtChar({1, 0});
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, '\r', {});
  SetUpChar({0, 2}, '\n', {});
  SetUpChar({0, 3}, 'b', {gfx::Rect(10, 26, 12, 14)});

  // Start at left of page 0, char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, '\n', {kTestChar1ScreenRect});

  // Start at left of page 0, char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, '\r', {});
  SetUpChar({0, 2}, '\n', {});
  SetUpChar({0, 3}, '\n', {gfx::Rect(10, 26, 12, 14)});
  SetUpChar({0, 4}, 'b', {gfx::Rect(22, 26, 12, 14)});

  // Start at left of page 0, char 1.
  InitializeCaretAtChar({0, 1});
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, '\n', {});
  SetUpChar({0, 2}, 'b', {gfx::Rect(10, 26, 12, 14)});

  // Start at left of page 0, char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, '\r', {});
  SetUpChar({0, 2}, '\n', {});
  SetUpChar({0, 3}, 'b', {gfx::Rect(10, 24, 12, 14)});

  // Start at left of char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  SetUpPagesWithCharCounts({0});
  SetUpChar(kTestChar0, '\0', {kDefaultCaret});

  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kDefaultCaret);

  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kDefaultCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownSingleLine) {
  SetUpPagesWithCharCounts({3});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  SetUpChar({0, 2}, 'c', {gfx::Rect(34, 10, 12, 14)});

  // Start at right of char 0.
  InitializeCaretAtChar({0, 1});
  caret().SetVisibility(true);

  // Left of char 0.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);

  // No change.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);

  // Right of char 2 'c'.
  constexpr gfx::Rect kTestChar2EndCaret{46, 10, 1, 14};
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar2EndCaret);

  // No change.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(kTestChar2EndCaret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownMultiLine) {
  SetUpPagesWithCharCounts({10});
  SetUpPagesWithSynthesizedChars({{2, 3, 6, 7}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  SetUpChar({0, 2}, '\r', {});
  SetUpChar({0, 3}, '\n', {});
  SetUpChar({0, 4}, 'c', {gfx::Rect(11, 26, 10, 12)});
  SetUpChar({0, 5}, 'd', {gfx::Rect(21, 26, 10, 12)});
  SetUpChar({0, 6}, '\r', {});
  SetUpChar({0, 7}, '\n', {});
  SetUpChar({0, 8}, 'e', {gfx::Rect(10, 50, 14, 16)});
  SetUpChar({0, 9}, 'f', {gfx::Rect(24, 50, 14, 16)});

  // Start at left of char 1 'b'.
  InitializeCaretAtChar({0, 1});
  caret().SetVisibility(true);

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
  constexpr PageCharacterIndex kStartNewline{0, 2};
  SetUpPagesWithCharCounts({6});
  SetUpPagesWithSynthesizedChars({{2, 3}});
  SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  SetUpChar(kStartNewline, '\r', {});
  SetUpChar({0, 3}, '\n', {});
  SetUpChar({0, 4}, 'c', {gfx::Rect(10, 22, 12, 14)});
  SetUpChar({0, 5}, 'd', {gfx::Rect(22, 22, 12, 14)});

  // Start at right of char 1 '\r'.
  InitializeCaretAtChar(kStartNewline);
  caret().SetVisibility(true);
  TestDrawCaret(kTestChar1EndCaret);

  // Right of char 5 'd'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
  TestDrawCaret(gfx::Rect(34, 22, 1, 14));

  // Reset back to right of char 1.
  caret().SetChar(kStartNewline);
  TestDrawCaret(kTestChar1EndCaret);

  // Left of char 0 'a'.
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
  TestDrawCaret(kTestChar0Caret);
}

TEST_F(PdfCaretMoveTest, MoveCharUpDownMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 0, char 0 'a'.
  InitializeCaretAtChar({0, 1});
  caret().SetVisibility(true);

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
  SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
  SetUpChar({0, 2}, 'c', {gfx::Rect(34, 10, 12, 14)});
  SetUpChar({0, 3}, '\r', {});
  SetUpChar({0, 4}, '\n', {});
  SetUpChar({0, 5}, 'd', {gfx::Rect(10, 22, 12, 14)});

  // Start at left of char 2 'c'.
  constexpr gfx::Rect kTestChar2Caret{34, 10, 1, 14};
  InitializeCaretAtChar({0, 2});
  caret().SetVisibility(true);
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
  SetUpChar({0, 1}, '\r', {});
  SetUpChar({0, 2}, '\n', {});
  SetUpChar({0, 3}, 'b', {gfx::Rect(10, 22, 12, 14)});
  SetUpChar({0, 4}, 'c', {gfx::Rect(22, 22, 12, 14)});
  SetUpChar({0, 5}, 'd', {gfx::Rect(34, 22, 12, 14)});

  // Start at right of char 5 'd'.
  InitializeCaretAtChar({0, 6});
  caret().SetVisibility(true);
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

class PdfCaretSelectionTest : public PdfCaretMoveTest {
 public:
  void SetUpSingleLineTest() {
    SetUpPagesWithCharCounts({3});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
    SetUpChar({0, 2}, 'c', {gfx::Rect(34, 10, 12, 14)});
  }

  void SetUpMultiLineTest() {
    SetUpPagesWithCharCounts({10});
    SetUpPagesWithSynthesizedChars({{2, 3, 6, 7}});
    SetUpChar(kTestChar0, 'a', {kTestChar0ScreenRect});
    SetUpChar({0, 1}, 'b', {kTestChar1ScreenRect});
    SetUpChar({0, 2}, '\r', {});
    SetUpChar({0, 3}, '\n', {});
    SetUpChar({0, 4}, 'c', {gfx::Rect(11, 26, 10, 12)});
    SetUpChar({0, 5}, 'd', {gfx::Rect(21, 26, 10, 12)});
    SetUpChar({0, 6}, '\r', {});
    SetUpChar({0, 7}, '\n', {});
    SetUpChar({0, 8}, 'e', {gfx::Rect(10, 50, 14, 16)});
    SetUpChar({0, 9}, 'f', {gfx::Rect(24, 50, 14, 16)});
  }

  blink::WebKeyboardEvent GenerateShiftKeyboardEvent(ui::KeyboardCode key) {
    blink::WebKeyboardEvent event = GenerateKeyboardEvent(key);
    event.SetModifiers(blink::WebInputEvent::Modifiers::kShiftKey);
    return event;
  }
};

TEST_F(PdfCaretSelectionTest, SelectRight) {
  SetUpSingleLineTest();

  // Start at left of char 0.
  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  // Move right. Select char 1.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar0));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 1)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));

  // Move right without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_RIGHT)));
}

TEST_F(PdfCaretSelectionTest, SelectLeft) {
  SetUpSingleLineTest();

  // Start at right of char 2.
  constexpr PageCharacterIndex kTestChar2End{0, 3};
  InitializeCaretAtChar(kTestChar2End);
  caret().SetVisibility(true);

  // Move left. Select char 2.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar2End));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 2)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));

  // Move left without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_LEFT)));
}

TEST_F(PdfCaretSelectionTest, SelectDown) {
  SetUpMultiLineTest();

  // Start at left of char 1 'b'.
  constexpr PageCharacterIndex kTestChar1{0, 1};
  InitializeCaretAtChar(kTestChar1);
  caret().SetVisibility(true);

  // Move down. Select chars 1, 2, 3, 4.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar1));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 5)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select chars 5, 6, 7, 8.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 9)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select char 9 'f' (end of page).
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 10)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretSelectionTest, SelectUp) {
  SetUpMultiLineTest();

  // Start at left of char 9 'f'.
  constexpr PageCharacterIndex kTestChar9{0, 9};
  InitializeCaretAtChar(kTestChar9);
  caret().SetVisibility(true);

  // Move up. Select chars 8, 7, 6, 5.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar9));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 5)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select chars 4, 3, 2, 1.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 1)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select char 0 'a' (start of page).
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(kTestChar0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up without shift.
  EXPECT_CALL(client(), ClearTextSelection());
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

TEST_F(PdfCaretSelectionTest, SelectStartOnNonTextPageMoveToNonTextPage) {
  SetUpPagesWithCharCounts({0, 0});
  SetUpChar(kTestChar0, '\0', {kDefaultCaret});
  SetUpChar({1, 0}, '\0', {gfx::Rect(10, 50, 1, 12)});

  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  SetUpChar({1, 0}, '\0', {gfx::Rect(10, 50, 1, 12)});
  SetUpChar({2, 0}, '\0', {gfx::Rect(10, 100, 1, 12)});

  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

  // Select page 0, char 0.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestChar0));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 1)));
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
  SetUpPagesWithCharCounts({0});
  SetUpChar(kTestChar0, '\0', {kDefaultCaret});

  InitializeCaretAtChar(kTestChar0);
  caret().SetVisibility(true);

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
  InitializeCaretAtChar({2, 0});
  caret().SetVisibility(true);

  // `StartSelection()` should be called on the nearest caret position in the
  // direction of movement. In this case, it would be right of page 1, char 1.
  constexpr PageCharacterIndex kTestPage1Char1End{1, 2};
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(kTestPage1Char1End));
  EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(kTestPage1Char1End));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

TEST_F(PdfCaretSelectionTest, MoveCaretWithShiftDownMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 0, char 0.
  InitializeCaretAtChar({0, 1});
  caret().SetVisibility(true);

  // Move down. Select page 1, char 0 'b'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(PageCharacterIndex(0, 1)));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(1, 1)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select page 1, char 1 'c'. Caret should be on the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(1, 2)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. The selection should extend past the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 0)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));

  // Move down. Select page 3, char 0 'd'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 1)));
  EXPECT_TRUE(caret().OnKeyDown(
      GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_DOWN)));
}

TEST_F(PdfCaretSelectionTest, MoveCaretWithShiftUpMultiPage) {
  SetUpMultiPageTest();

  // Start at right of page 3, char 0 'd'.
  InitializeCaretAtChar({3, 1});
  caret().SetVisibility(true);

  // Move up. Select page 3, char 0. Caret should be on no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(false));
  EXPECT_CALL(client(), StartSelection(PageCharacterIndex(3, 1)));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(3, 0)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. The selection should extend past the no-text page.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(1, 2)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select page 1, char 1 'c' and page 1, char 0 'b'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(),
              ExtendAndInvalidateSelectionByChar(PageCharacterIndex(0, 1)));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));

  // Move up. Select page 0, char 0 'a'.
  EXPECT_CALL(client(), IsSelecting()).WillOnce(Return(true));
  EXPECT_CALL(client(), ExtendAndInvalidateSelectionByChar(kTestChar0));
  EXPECT_TRUE(
      caret().OnKeyDown(GenerateShiftKeyboardEvent(ui::KeyboardCode::VKEY_UP)));
}

}  // namespace

}  // namespace chrome_pdf
