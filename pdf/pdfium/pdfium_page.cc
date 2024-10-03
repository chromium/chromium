// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/angle_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/accessibility_helper.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_ocr.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "pdf/ui/thumbnail.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_catalog.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skbitmap_operations.h"
#endif

using printing::ConvertUnitFloat;
using printing::kPixelsPerInch;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

constexpr float k45DegreesInRadians = base::DegToRad(45.0f);
constexpr float k90DegreesInRadians = base::DegToRad(90.0f);
constexpr float k180DegreesInRadians = base::DegToRad(180.0f);
constexpr float k270DegreesInRadians = base::DegToRad(270.0f);
constexpr float k360DegreesInRadians = base::DegToRad(360.0f);

constexpr float kPointsToPixels = static_cast<float>(printing::kPixelsPerInch) /
                                  static_cast<float>(printing::kPointsPerInch);

// Page rotations in clockwise degrees.
enum class Rotation {
  kRotate0 = 0,
  kRotate90 = 1,
  kRotate180 = 2,
  kRotate270 = 3,
};

gfx::RectF FloatPageRectToPixelRect(FPDF_PAGE page, const gfx::RectF& input) {
  int output_width = FPDF_GetPageWidthF(page);
  int output_height = FPDF_GetPageHeightF(page);

  int min_x;
  int min_y;
  int max_x;
  int max_y;
  if (!FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0, input.x(),
                         input.y(), &min_x, &min_y)) {
    return gfx::RectF();
  }
  if (!FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0,
                         input.right(), input.bottom(), &max_x, &max_y)) {
    return gfx::RectF();
  }
  if (max_x < min_x)
    std::swap(min_x, max_x);
  if (max_y < min_y)
    std::swap(min_y, max_y);

  // Make sure small but non-zero dimensions for `input` does not get rounded
  // down to 0.
  int width = max_x - min_x;
  int height = max_y - min_y;
  if (width == 0 && input.width())
    width = 1;
  if (height == 0 && input.height())
    height = 1;

  gfx::RectF output_rect(
      ConvertUnitFloat(min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitFloat(min_y, kPointsPerInch, kPixelsPerInch),
      ConvertUnitFloat(width, kPointsPerInch, kPixelsPerInch),
      ConvertUnitFloat(height, kPointsPerInch, kPixelsPerInch));
  return output_rect;
}

gfx::RectF GetFloatCharRectInPixels(FPDF_PAGE page,
                                    FPDF_TEXTPAGE text_page,
                                    int index) {
  double left;
  double right;
  double bottom;
  double top;
  if (!FPDFText_GetCharBox(text_page, index, &left, &right, &bottom, &top))
    return gfx::RectF();

  if (right < left)
    std::swap(left, right);
  if (bottom < top)
    std::swap(top, bottom);
  gfx::RectF page_coords(left, top, right - left, bottom - top);
  return FloatPageRectToPixelRect(page, page_coords);
}

int GetFirstNonUnicodeWhiteSpaceCharIndex(FPDF_TEXTPAGE text_page,
                                          int start_char_index,
                                          int chars_count) {
  int i = start_char_index;
  while (i < chars_count &&
         base::IsUnicodeWhitespace(FPDFText_GetUnicode(text_page, i))) {
    i++;
  }
  return i;
}

AccessibilityTextDirection GetDirectionFromAngle(float angle) {
  // Rotating the angle by 45 degrees to simplify the conditions statements.
  // It's like if we rotated the whole cartesian coordinate system like below.
  //   X                   X
  //     X      IV       X
  //       X           X
  //         X       X
  //           X   X
  //   III       X       I
  //           X   X
  //         X       X
  //       X           X
  //     X      II       X
  //   X                   X

  angle = fmodf(angle + k45DegreesInRadians, k360DegreesInRadians);
  // Quadrant I.
  if (angle >= 0 && angle <= k90DegreesInRadians)
    return AccessibilityTextDirection::kLeftToRight;
  // Quadrant II.
  if (angle > k90DegreesInRadians && angle <= k180DegreesInRadians)
    return AccessibilityTextDirection::kTopToBottom;
  // Quadrant III.
  if (angle > k180DegreesInRadians && angle <= k270DegreesInRadians)
    return AccessibilityTextDirection::kRightToLeft;
  // Quadrant IV.
  return AccessibilityTextDirection::kBottomToTop;
}

void AddCharSizeToAverageCharSize(gfx::SizeF new_size,
                                  gfx::SizeF* avg_size,
                                  int* count) {
  // Some characters sometimes have a bogus empty bounding box. We don't want
  // them to impact the average.
  if (!new_size.IsEmpty()) {
    avg_size->set_width((avg_size->width() * *count + new_size.width()) /
                        (*count + 1));
    avg_size->set_height((avg_size->height() * *count + new_size.height()) /
                         (*count + 1));
    (*count)++;
  }
}

float GetRotatedCharWidth(float angle, const gfx::SizeF& size) {
  return fabsf(cosf(angle) * size.width()) + fabsf(sinf(angle) * size.height());
}

float GetAngleOfVector(const gfx::Vector2dF& v) {
  float angle = atan2f(v.y(), v.x());
  if (angle < 0)
    angle += k360DegreesInRadians;
  return angle;
}

float GetAngleDifference(float a, float b) {
  // This is either the difference or (360 - difference).
  float x = fmodf(fabsf(b - a), k360DegreesInRadians);
  return x > k180DegreesInRadians ? k360DegreesInRadians - x : x;
}

bool FloatEquals(float f1, float f2) {
  // The idea behind this is to use this fraction of the larger of the
  // two numbers as the limit of the difference.  This breaks down near
  // zero, so we reuse this as the minimum absolute size we will use
  // for the base of the scale too.
  static constexpr float kEpsilonScale = 0.00001f;
  return fabsf(f1 - f2) <
         kEpsilonScale * fmaxf(fmaxf(fabsf(f1), fabsf(f2)), kEpsilonScale);
}

bool IsRadioButtonOrCheckBox(int button_type) {
  return button_type == FPDF_FORMFIELD_CHECKBOX ||
         button_type == FPDF_FORMFIELD_RADIOBUTTON;
}

template <typename T>
bool CompareTextRuns(const T& a, const T& b) {
  return a.text_range.index < b.text_range.index;
}

// Set text run style information based on the `text_object` associated with a
// character of the text run.
AccessibilityTextStyleInfo CalculateTextRunStyleInfo(
    FPDF_PAGEOBJECT text_object) {
  AccessibilityTextStyleInfo style_info;

  float font_size;
  if (FPDFTextObj_GetFontSize(text_object, &font_size)) {
    style_info.font_size = font_size;
  }

  FPDF_FONT font = FPDFTextObj_GetFont(text_object);
  size_t buffer_size = FPDFFont_GetBaseFontName(font, nullptr, 0);
  if (buffer_size > 0) {
    PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
        &style_info.font_name, buffer_size, true);
    char* data = static_cast<char*>(api_string_adapter.GetData());
    size_t bytes_written = FPDFFont_GetBaseFontName(font, data, buffer_size);
    // Trim the null character.
    api_string_adapter.Close(bytes_written);
  }

  // As defined in ISO 32000-1:2008, table 123.
  constexpr int kFlagItalic = (1 << 6);
  int font_flags = FPDFFont_GetFlags(font);
  if (font_flags != -1) {
    style_info.is_italic = (font_flags & kFlagItalic);
  }

  // Bold text is considered bold when greater than or equal to 700.
  constexpr int kStandardBoldValue = 700;
  int font_weight = FPDFFont_GetWeight(font);
  if (font_weight != -1) {
    style_info.font_weight = font_weight;
    style_info.is_bold = style_info.font_weight >= kStandardBoldValue;
  }

  unsigned int fill_r;
  unsigned int fill_g;
  unsigned int fill_b;
  unsigned int fill_a;
  if (FPDFPageObj_GetFillColor(text_object, &fill_r, &fill_g, &fill_b,
                               &fill_a)) {
    style_info.fill_color = MakeARGB(fill_a, fill_r, fill_g, fill_b);
  } else {
    style_info.fill_color = MakeARGB(0xff, 0, 0, 0);
  }

  unsigned int stroke_r;
  unsigned int stroke_g;
  unsigned int stroke_b;
  unsigned int stroke_a;
  if (FPDFPageObj_GetStrokeColor(text_object, &stroke_r, &stroke_g, &stroke_b,
                                 &stroke_a)) {
    style_info.stroke_color = MakeARGB(stroke_a, stroke_r, stroke_g, stroke_b);
  } else {
    style_info.stroke_color = MakeARGB(0xff, 0, 0, 0);
  }

  int render_mode = FPDFTextObj_GetTextRenderMode(text_object);
  DCHECK_GE(render_mode,
            static_cast<int>(AccessibilityTextRenderMode::kUnknown));
  DCHECK_LE(render_mode,
            static_cast<int>(AccessibilityTextRenderMode::kMaxValue));
  style_info.render_mode =
      static_cast<AccessibilityTextRenderMode>(render_mode);
  return style_info;
}

// Returns true if the `text_object` associated with a given character has the
// same text style as the text run.
bool AreTextStyleEqual(FPDF_PAGEOBJECT text_object,
                       const AccessibilityTextStyleInfo& style) {
  AccessibilityTextStyleInfo char_style =
      CalculateTextRunStyleInfo(text_object);
  return char_style.font_name == style.font_name &&
         char_style.font_weight == style.font_weight &&
         char_style.render_mode == style.render_mode &&
         FloatEquals(char_style.font_size, style.font_size) &&
         char_style.fill_color == style.fill_color &&
         char_style.stroke_color == style.stroke_color &&
         char_style.is_italic == style.is_italic &&
         char_style.is_bold == style.is_bold;
}

// Returns the bounds with the smallest left, smallest bottom, largest right,
// and largest top.
FS_RECTF GetLargestBounds(const FS_RECTF& largest_bounds,
                          const FS_RECTF& bounds) {
  return {std::min(largest_bounds.left, bounds.left),
          std::max(largest_bounds.top, bounds.top),
          std::max(largest_bounds.right, bounds.right),
          std::min(largest_bounds.bottom, bounds.bottom)};
}

gfx::RectF GetRotatedRectF(Rotation rotation,
                           gfx::SizeF page_size,
                           const FS_RECTF& original_bounds) {
  FS_RECTF bounds;

  // When the page is rotated 90 degrees or 270 degrees, the page width and
  // height are swapped. Swap it back for calculations.
  if (rotation == Rotation::kRotate90 || rotation == Rotation::kRotate270) {
    page_size.Transpose();
  }

  switch (rotation) {
    case Rotation::kRotate0: {
      bounds = original_bounds;
      break;
    }
    case Rotation::kRotate90: {
      bounds.left = original_bounds.bottom;
      bounds.top = page_size.width() - original_bounds.left;
      bounds.right = original_bounds.top;
      bounds.bottom = page_size.width() - original_bounds.right;
      break;
    }
    case Rotation::kRotate180: {
      bounds.left = page_size.width() - original_bounds.right;
      bounds.top = page_size.height() - original_bounds.bottom;
      bounds.right = page_size.width() - original_bounds.left;
      bounds.bottom = page_size.height() - original_bounds.top;
      break;
    }
    case Rotation::kRotate270: {
      bounds.left = page_size.height() - original_bounds.top;
      bounds.top = original_bounds.right;
      bounds.right = page_size.height() - original_bounds.bottom;
      bounds.bottom = original_bounds.left;
      break;
    }
  }

  return gfx::RectF(bounds.left, bounds.bottom, bounds.right - bounds.left,
                    bounds.top - bounds.bottom);
}

// Get the effective crop box. If empty or failed to calculate the effective
// crop box, default to a `gfx::RectF` with dimensions page width by page
// height.
gfx::RectF GetEffectiveCropBox(FPDF_PAGE page,
                               Rotation rotation,
                               const gfx::SizeF& page_size) {
  gfx::RectF effective_crop_box;
  FS_RECTF effective_crop_bounds;
  if (FPDF_GetPageBoundingBox(page, &effective_crop_bounds)) {
    effective_crop_box =
        GetRotatedRectF(rotation, page_size, effective_crop_bounds);
  }

  if (effective_crop_box.IsEmpty()) {
    effective_crop_box =
        gfx::RectF(0, 0, page_size.width(), page_size.height());
  }

  return effective_crop_box;
}

}  // namespace

PDFiumPage::LinkTarget::LinkTarget() : page(-1) {}

PDFiumPage::LinkTarget::LinkTarget(const LinkTarget& other) = default;

PDFiumPage::LinkTarget::~LinkTarget() = default;

PDFiumPage::PDFiumPage(PDFiumEngine* engine, int i)
    : engine_(engine), index_(i) {}

PDFiumPage::PDFiumPage(PDFiumPage&& that) = default;

PDFiumPage::~PDFiumPage() {
  DCHECK_EQ(0, preventing_unload_count_);
}

void PDFiumPage::Unload() {
  // Do not unload while in the middle of a load, or if some external source
  // expects `this` to stay loaded.
  if (preventing_unload_count_)
    return;

  text_page_.reset();

  if (page_) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // TODO(crbug.com/360803943): Keep previously generated OCR results.
    engine_->CancelPendingSearchify(index_);
#endif
    if (engine_->form()) {
      FORM_OnBeforeClosePage(page(), engine_->form());
    }
    page_.reset();
  }
}

FPDF_PAGE PDFiumPage::GetPage() {
  ScopedUnsupportedFeature scoped_unsupported_feature(engine_);
  if (!available_)
    return nullptr;
  if (!page_) {
    ScopedUnloadPreventer scoped_unload_preventer(this);
    page_.reset(FPDF_LoadPage(engine_->doc(), index_));
    if (page_) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      engine_->ScheduleSearchifyIfNeeded(this);
#endif
      if (engine_->form()) {
        FORM_OnAfterLoadPage(page(), engine_->form());
      }
    }
  }
  return page();
}

FPDF_TEXTPAGE PDFiumPage::GetTextPage() {
  if (!available_)
    return nullptr;
  if (!text_page_) {
    ScopedUnloadPreventer scoped_unload_preventer(this);
    text_page_.reset(FPDFText_LoadPage(GetPage()));
  }
  return text_page();
}

void PDFiumPage::ReloadTextPage() {
  CHECK_EQ(preventing_unload_count_, 0);
  text_page_.reset();
  GetTextPage();
}

void PDFiumPage::CalculatePageObjectTextRunBreaks() {
  if (calculated_page_object_text_run_breaks_)
    return;

  calculated_page_object_text_run_breaks_ = true;
  int chars_count = FPDFText_CountChars(GetTextPage());
  if (chars_count == 0)
    return;

  CalculateLinks();
  for (const auto& link : links_) {
    if (link.start_char_index >= 0 && link.start_char_index < chars_count) {
      page_object_text_run_breaks_.insert(link.start_char_index);
      int next_text_run_break_index = link.start_char_index + link.char_count;
      // Don't insert a break if the link is at the end of the page text.
      if (next_text_run_break_index < chars_count) {
        page_object_text_run_breaks_.insert(next_text_run_break_index);
      }
    }
  }

  PopulateAnnotations();
  for (const auto& highlight : highlights_) {
    if (highlight.start_char_index >= 0 &&
        highlight.start_char_index < chars_count) {
      page_object_text_run_breaks_.insert(highlight.start_char_index);
      int next_text_run_break_index =
          highlight.start_char_index + highlight.char_count;
      // Don't insert a break if the highlight is at the end of the page text.
      if (next_text_run_break_index < chars_count) {
        page_object_text_run_breaks_.insert(next_text_run_break_index);
      }
    }
  }
}

int PDFiumPage::GetCharCount() {
  if (!available_) {
    return 0;
  }
  return FPDFText_CountChars(GetTextPage());
}

std::optional<AccessibilityTextRunInfo> PDFiumPage::GetTextRunInfo(
    int start_char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  int chars_count = FPDFText_CountChars(text_page);
  // Check to make sure `start_char_index` is within bounds.
  if (start_char_index < 0 || start_char_index >= chars_count)
    return std::nullopt;

  AccessibilityTextRunInfo info;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  info.is_searchified = IsCharacterGeneratedBySearchify(start_char_index);
#endif

  int actual_start_char_index = GetFirstNonUnicodeWhiteSpaceCharIndex(
      text_page, start_char_index, chars_count);
  // Check to see if GetFirstNonUnicodeWhiteSpaceCharIndex() iterated through
  // all the characters.
  if (actual_start_char_index >= chars_count) {
    // If so, `info.len` needs to take the number of characters
    // iterated into account.
    DCHECK_GT(actual_start_char_index, start_char_index);
    info.len = chars_count - start_char_index;
    return info;
  }

  // If the first character in a text run is a space, we need to start
  // `text_run_bounds` from the space character instead of the first
  // non-space unicode character.
  gfx::RectF text_run_bounds =
      actual_start_char_index > start_char_index
          ? GetFloatCharRectInPixels(page, text_page, start_char_index)
          : gfx::RectF();

  int char_index = actual_start_char_index;

  // Set text run's style info from the first character of the text run.
  FPDF_PAGEOBJECT text_object = FPDFText_GetTextObject(text_page, char_index);
  info.style = CalculateTextRunStyleInfo(text_object);

  gfx::RectF start_char_rect =
      GetFloatCharRectInPixels(page, text_page, char_index);
  float text_run_font_size = info.style.font_size;

  // Heuristic: Initialize the average character size to one-third of the font
  // size to avoid having the first few characters misrepresent the average.
  // Without it, if a text run starts with a '.', its small bounding box could
  // lead to a break in the text run after only one space. Ex: ". Hello World"
  // would be split in two runs: "." and "Hello World".
  float font_size_minimum;
  if (FPDFTextObj_GetFontSize(text_object, &font_size_minimum)) {
    font_size_minimum /= 3.0f;
  } else {
    font_size_minimum = 0.0f;
  }
  gfx::SizeF avg_char_size(font_size_minimum, font_size_minimum);
  int non_whitespace_chars_count = 1;
  AddCharSizeToAverageCharSize(start_char_rect.size(), &avg_char_size,
                               &non_whitespace_chars_count);

  // Add first non-space char to text run.
  text_run_bounds.Union(start_char_rect);
  AccessibilityTextDirection char_direction =
      GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index));
  if (char_index < chars_count)
    char_index++;

  gfx::RectF prev_char_rect = start_char_rect;
  float estimated_font_size =
      std::max(start_char_rect.width(), start_char_rect.height());

  // The angle of the vector starting at the first character center-point and
  // ending at the current last character center-point.
  float text_run_angle = 0;

  CalculatePageObjectTextRunBreaks();
  const auto breakpoint_iter =
      std::lower_bound(page_object_text_run_breaks_.begin(),
                       page_object_text_run_breaks_.end(), char_index);
  int breakpoint_index = breakpoint_iter != page_object_text_run_breaks_.end()
                             ? *breakpoint_iter
                             : -1;

  // Continue adding characters until heuristics indicate we should end the text
  // run.
  while (char_index < chars_count) {
    // Split a text run when it encounters a page object like links or images.
    if (char_index == breakpoint_index)
      break;

    unsigned int character = FPDFText_GetUnicode(text_page, char_index);
    gfx::RectF char_rect =
        GetFloatCharRectInPixels(page, text_page, char_index);

    if (!base::IsUnicodeWhitespace(character)) {
      // Heuristic: End the text run if the text style of the current character
      // is different from the text run's style. The style can only be different
      // if the FPDF_PAGEOBJECTs are different, so check the FPDF_PAGEOBJECTs
      // first to make the comparison faster.
      FPDF_PAGEOBJECT current_text_object =
          FPDFText_GetTextObject(text_page, char_index);
      if (current_text_object != text_object &&
          !AreTextStyleEqual(current_text_object, info.style)) {
        break;
      }

      // Heuristic: End text run if character isn't going in the same direction.
      if (char_direction !=
          GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index))) {
        break;
      }

      // Heuristic: End the text run if the difference between the text run
      // angle and the angle between the center-points of the previous and
      // current characters is greater than 90 degrees.
      float current_angle = GetAngleOfVector(char_rect.CenterPoint() -
                                             prev_char_rect.CenterPoint());
      if (start_char_rect != prev_char_rect) {
        text_run_angle = GetAngleOfVector(prev_char_rect.CenterPoint() -
                                          start_char_rect.CenterPoint());

        if (GetAngleDifference(text_run_angle, current_angle) >
            k90DegreesInRadians) {
          break;
        }
      }

      // Heuristic: End the text run if the center-point distance to the
      // previous character is less than 2.5x the average character size.
      AddCharSizeToAverageCharSize(char_rect.size(), &avg_char_size,
                                   &non_whitespace_chars_count);

      float avg_char_width = GetRotatedCharWidth(current_angle, avg_char_size);

      float distance =
          (char_rect.CenterPoint() - prev_char_rect.CenterPoint()).Length() -
          GetRotatedCharWidth(current_angle, char_rect.size()) / 2 -
          GetRotatedCharWidth(current_angle, prev_char_rect.size()) / 2;

      if (distance > 2.5f * avg_char_width) {
        break;
      }

      text_run_bounds.Union(char_rect);
      prev_char_rect = char_rect;
    }

    if (!char_rect.IsEmpty()) {
      // Update the estimated font size if needed.
      float char_largest_side = std::max(char_rect.height(), char_rect.width());
      estimated_font_size = std::max(char_largest_side, estimated_font_size);
    }

    char_index++;
  }

  // Some PDFs have missing or obviously bogus font sizes; substitute the
  // font size by the width or height (whichever's the largest) of the bigger
  // character in the current text run.
  if (text_run_font_size <= 1 || text_run_font_size < estimated_font_size / 2 ||
      text_run_font_size > estimated_font_size * 2) {
    text_run_font_size = estimated_font_size;
  }

  info.len = char_index - start_char_index;
  info.style.font_size = text_run_font_size;
  info.bounds = text_run_bounds;
  // Infer text direction from first and last character of the text run. We
  // can't base our decision on the character direction, since a character of a
  // RTL language will have an angle of 0 when not rotated, just like a
  // character in a LTR language.
  info.direction = char_index - actual_start_char_index > 1
                       ? GetDirectionFromAngle(text_run_angle)
                       : AccessibilityTextDirection::kNone;
  return info;
}

uint32_t PDFiumPage::GetCharUnicode(int char_index) {
  // No explicit `available_` check here to return 0 when unavailable.
  // If this page is unavailable, GetTextPage() returns nullptr and
  // FPDFText_GetUnicode() naturally returns 0.
  return FPDFText_GetUnicode(GetTextPage(), char_index);
}

gfx::RectF PDFiumPage::GetCharBounds(int char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  return GetFloatCharRectInPixels(page, text_page, char_index);
}

gfx::RectF PDFiumPage::GetCroppedRect() {
  FPDF_PAGE page = GetPage();
  FS_RECTF raw_rect;
  if (!FPDF_GetPageBoundingBox(page, &raw_rect))
    return gfx::RectF();

  if (raw_rect.right < raw_rect.left)
    std::swap(raw_rect.right, raw_rect.left);
  if (raw_rect.bottom > raw_rect.top)
    std::swap(raw_rect.bottom, raw_rect.top);

  gfx::RectF rect(raw_rect.left, raw_rect.bottom,
                  raw_rect.right - raw_rect.left,
                  raw_rect.top - raw_rect.bottom);
  return FloatPageRectToPixelRect(page, rect);
}

gfx::RectF PDFiumPage::GetBoundingBox() {
  FPDF_PAGE page = GetPage();
  if (!page) {
    return gfx::RectF();
  }

  // Page width and height are already swapped based on page rotation.
  gfx::SizeF page_size(FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page));
  Rotation rotation = static_cast<Rotation>(FPDFPage_GetRotation(page));

  // Start with bounds with the left and bottom values at the max possible
  // bounds and the right and top values at the min possible bounds. Bounds are
  // relative to the media box.
  FS_RECTF largest_bounds = {page_size.width(), 0, 0, page_size.height()};
  for (int i = 0; i < FPDFPage_CountObjects(page); ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    if (!page_object) {
      continue;
    }

    FS_RECTF bounds;
    if (FPDFPageObj_GetBounds(page_object, &bounds.left, &bounds.bottom,
                              &bounds.right, &bounds.top)) {
      largest_bounds = GetLargestBounds(largest_bounds, bounds);
    }
  }
  for (int i = 0; i < FPDFPage_GetAnnotCount(page); ++i) {
    ScopedFPDFAnnotation annotation(FPDFPage_GetAnnot(page, i));
    if (!annotation) {
      continue;
    }

    FS_RECTF bounds;
    if (FPDFAnnot_GetRect(annotation.get(), &bounds)) {
      largest_bounds = GetLargestBounds(largest_bounds, bounds);
    }
  }

  gfx::RectF bounding_box =
      GetRotatedRectF(rotation, page_size, largest_bounds);

  gfx::RectF effective_crop_box =
      GetEffectiveCropBox(page, rotation, page_size);

  // If the bounding box is empty, default to the effective crop box.
  if (bounding_box.IsEmpty()) {
    bounding_box = effective_crop_box;
  } else {
    // Some bounding boxes may be out-of-bounds of `effective_crop_box`. Clip to
    // be within `effective_crop_box`.
    bounding_box.Intersect(effective_crop_box);
  }

  // Set the bounding box to be relative to the effective crop box.
  bounding_box.set_x(bounding_box.x() - effective_crop_box.x());
  bounding_box.set_y(bounding_box.y() - effective_crop_box.y());

  // Scale to page pixels.
  bounding_box.Scale(kPointsToPixels);

  return bounding_box;
}

bool PDFiumPage::IsCharInPageBounds(int char_index,
                                    const gfx::RectF& page_bounds) {
  gfx::RectF char_bounds = GetCharBounds(char_index);

  // Make sure `char_bounds` has a minimum size so Intersects() works correctly.
  if (char_bounds.IsEmpty()) {
    static constexpr gfx::SizeF kMinimumSize(0.0001f, 0.0001f);
    char_bounds.set_size(kMinimumSize);
  }

  return page_bounds.Intersects(char_bounds);
}

std::vector<AccessibilityLinkInfo> PDFiumPage::GetLinkInfo(
    const std::vector<AccessibilityTextRunInfo>& text_runs) {
  std::vector<AccessibilityLinkInfo> link_info;
  if (!available_)
    return link_info;

  CalculateLinks();

  link_info.reserve(links_.size());
  for (size_t i = 0; i < links_.size(); ++i) {
    const Link& link = links_[i];
    AccessibilityLinkInfo cur_info;
    cur_info.url = link.target.url;
    cur_info.index_in_page = i;
    cur_info.text_range = GetEnclosingTextRunRangeForCharRange(
        text_runs, link.start_char_index, link.char_count);

    gfx::Rect link_rect;
    for (const auto& rect : link.bounding_rects)
      link_rect.Union(rect);
    cur_info.bounds = gfx::RectF(link_rect.x(), link_rect.y(),
                                 link_rect.width(), link_rect.height());

    link_info.push_back(std::move(cur_info));
  }

  std::sort(link_info.begin(), link_info.end(),
            CompareTextRuns<AccessibilityLinkInfo>);
  return link_info;
}

std::vector<AccessibilityImageInfo> PDFiumPage::GetImageInfo(
    uint32_t text_run_count) {
  std::vector<AccessibilityImageInfo> image_info;
  if (!available_)
    return image_info;

  CalculateImages();

  image_info.reserve(images_.size());
  for (const Image& image : images_) {
    AccessibilityImageInfo cur_info;
    cur_info.alt_text = image.alt_text;
    // TODO(mohitb): Update text run index to nearest text run to image bounds.
    cur_info.text_run_index = text_run_count;
    cur_info.bounds =
        gfx::RectF(image.bounding_rect.x(), image.bounding_rect.y(),
                   image.bounding_rect.width(), image.bounding_rect.height());
    cur_info.page_object_index = image.page_object_index;
    image_info.push_back(std::move(cur_info));
  }
  return image_info;
}

std::vector<int> PDFiumPage::GetImageObjectIndices() {
  if (!available_) {
    return {};
  }

  CalculateImages();
  return base::ToVector(
      images_, [](const Image& image) { return image.page_object_index; });
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
SkBitmap PDFiumPage::GetImageForOcr(int page_object_index) {
  FPDF_PAGE page = GetPage();
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, page_object_index);
  SkBitmap bitmap =
      ::chrome_pdf::GetImageForOcr(engine_->doc(), page, page_object);

  SkBitmapOperations::RotationAmount rotation;
  switch (FPDFPage_GetRotation(page)) {
    case 0:
      return bitmap;
    case 1:
      rotation = SkBitmapOperations::RotationAmount::ROTATION_90_CW;
      break;
    case 2:
      rotation = SkBitmapOperations::RotationAmount::ROTATION_180_CW;
      break;
    case 3:
      rotation = SkBitmapOperations::RotationAmount::ROTATION_270_CW;
      break;
  }

  return SkBitmapOperations::Rotate(bitmap, rotation);
}

void PDFiumPage::OnSearchifyGotOcrResult() {
  if (!IsPageSearchified()) {
    first_searchify_generated_object_index_ = FPDFPage_CountObjects(GetPage());
  }
}

bool PDFiumPage::IsPageSearchified() const {
  return first_searchify_generated_object_index_ != -1;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

std::vector<AccessibilityHighlightInfo> PDFiumPage::GetHighlightInfo(
    const std::vector<AccessibilityTextRunInfo>& text_runs) {
  std::vector<AccessibilityHighlightInfo> highlight_info;
  if (!available_)
    return highlight_info;

  PopulateAnnotations();

  highlight_info.reserve(highlights_.size());
  for (size_t i = 0; i < highlights_.size(); ++i) {
    const Highlight& highlight = highlights_[i];
    AccessibilityHighlightInfo cur_info;
    cur_info.index_in_page = i;
    cur_info.text_range = GetEnclosingTextRunRangeForCharRange(
        text_runs, highlight.start_char_index, highlight.char_count);
    cur_info.bounds = gfx::RectF(
        highlight.bounding_rect.x(), highlight.bounding_rect.y(),
        highlight.bounding_rect.width(), highlight.bounding_rect.height());
    cur_info.color = highlight.color;
    cur_info.note_text = highlight.note_text;
    highlight_info.push_back(std::move(cur_info));
  }

  std::sort(highlight_info.begin(), highlight_info.end(),
            CompareTextRuns<AccessibilityHighlightInfo>);
  return highlight_info;
}

std::vector<AccessibilityTextFieldInfo> PDFiumPage::GetTextFieldInfo(
    uint32_t text_run_count) {
  std::vector<AccessibilityTextFieldInfo> text_field_info;
  if (!available_)
    return text_field_info;

  PopulateAnnotations();

  text_field_info.reserve(text_fields_.size());
  for (size_t i = 0; i < text_fields_.size(); ++i) {
    const TextField& text_field = text_fields_[i];
    AccessibilityTextFieldInfo cur_info;
    cur_info.name = text_field.name;
    cur_info.value = text_field.value;
    cur_info.index_in_page = i;
    cur_info.is_read_only = !!(text_field.flags & FPDF_FORMFLAG_READONLY);
    cur_info.is_required = !!(text_field.flags & FPDF_FORMFLAG_REQUIRED);
    cur_info.is_password = !!(text_field.flags & FPDF_FORMFLAG_TEXT_PASSWORD);
    // TODO(crbug.com/40661774): Update text run index to nearest text run to
    // text field bounds.
    cur_info.text_run_index = text_run_count;
    cur_info.bounds = gfx::RectF(
        text_field.bounding_rect.x(), text_field.bounding_rect.y(),
        text_field.bounding_rect.width(), text_field.bounding_rect.height());
    text_field_info.push_back(std::move(cur_info));
  }
  return text_field_info;
}

PDFiumPage::Area PDFiumPage::GetLinkTargetAtIndex(int link_index,
                                                  LinkTarget* target) {
  if (!available_ || link_index < 0)
    return NONSELECTABLE_AREA;
  CalculateLinks();
  if (link_index >= static_cast<int>(links_.size()))
    return NONSELECTABLE_AREA;
  *target = links_[link_index].target;
  return target->url.empty() ? DOCLINK_AREA : WEBLINK_AREA;
}

PDFiumPage::Area PDFiumPage::GetLinkTarget(FPDF_LINK link, LinkTarget* target) {
  FPDF_DEST dest_link = FPDFLink_GetDest(engine_->doc(), link);
  if (dest_link)
    return GetDestinationTarget(dest_link, target);

  FPDF_ACTION action = FPDFLink_GetAction(link);
  if (!action)
    return NONSELECTABLE_AREA;

  switch (FPDFAction_GetType(action)) {
    case PDFACTION_GOTO: {
      FPDF_DEST dest_action = FPDFAction_GetDest(engine_->doc(), action);
      if (dest_action)
        return GetDestinationTarget(dest_action, target);
      // TODO(crbug.com/40445279): We don't fully support all types of the
      // in-document links.
      return NONSELECTABLE_AREA;
    }
    case PDFACTION_URI:
      return GetURITarget(action, target);
      // TODO(crbug.com/40540951): Support PDFACTION_LAUNCH.
      // TODO(crbug.com/40260046): Support PDFACTION_REMOTEGOTO.
    case PDFACTION_LAUNCH:
    case PDFACTION_REMOTEGOTO:
    default:
      return NONSELECTABLE_AREA;
  }
}

PDFiumPage::Area PDFiumPage::GetCharIndex(const gfx::Point& point,
                                          PageOrientation orientation,
                                          int* char_index,
                                          int* form_type,
                                          LinkTarget* target) {
  if (!available_)
    return NONSELECTABLE_AREA;

  gfx::Point device_point = point - rect_.OffsetFromOrigin();
  double new_x;
  double new_y;
  if (!FPDF_DeviceToPage(GetPage(), 0, 0, rect_.width(), rect_.height(),
                         ToPDFiumRotation(orientation), device_point.x(),
                         device_point.y(), &new_x, &new_y)) {
    return NONSELECTABLE_AREA;
  }

  // hit detection tolerance, in points.
  constexpr double kTolerance = 20.0;
  int rv = FPDFText_GetCharIndexAtPos(GetTextPage(), new_x, new_y, kTolerance,
                                      kTolerance);
  *char_index = rv;

  FPDF_LINK link = FPDFLink_GetLinkAtPoint(GetPage(), new_x, new_y);
  int control =
      FPDFPage_HasFormFieldAtPoint(engine_->form(), GetPage(), new_x, new_y);

  // If there is a control and link at the same point, figure out their z-order
  // to determine which is on top.
  if (link && control > FPDF_FORMFIELD_UNKNOWN) {
    int control_z_order = FPDFPage_FormFieldZOrderAtPoint(
        engine_->form(), GetPage(), new_x, new_y);
    int link_z_order = FPDFLink_GetLinkZOrderAtPoint(GetPage(), new_x, new_y);
    DCHECK_NE(control_z_order, link_z_order);
    if (control_z_order > link_z_order) {
      *form_type = control;
      return FormTypeToArea(*form_type);
    }

    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // In that case, GetLinkTarget() will return NONSELECTABLE_AREA
    // and we should proceed with area detection.
    Area area = GetLinkTarget(link, target);
    if (area != NONSELECTABLE_AREA)
      return area;
  } else if (link) {
    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // See identical block above.
    Area area = GetLinkTarget(link, target);
    if (area != NONSELECTABLE_AREA)
      return area;
  } else if (control > FPDF_FORMFIELD_UNKNOWN) {
    *form_type = control;
    return FormTypeToArea(*form_type);
  }

  if (rv < 0)
    return NONSELECTABLE_AREA;

  return GetLink(*char_index, target) != -1 ? WEBLINK_AREA : TEXT_AREA;
}

// static
PDFiumPage::Area PDFiumPage::FormTypeToArea(int form_type) {
  switch (form_type) {
    case FPDF_FORMFIELD_COMBOBOX:
    case FPDF_FORMFIELD_TEXTFIELD:
#if defined(PDF_ENABLE_XFA)
    // TODO(bug_353450): figure out selection and copying for XFA fields.
    case FPDF_FORMFIELD_XFA_COMBOBOX:
    case FPDF_FORMFIELD_XFA_TEXTFIELD:
#endif
      return FORM_TEXT_AREA;
    default:
      return NONSELECTABLE_AREA;
  }
}

bool PDFiumPage::IsCharIndexInBounds(int index) {
  return index >= 0 && index < GetCharCount();
}

PDFiumPage::Area PDFiumPage::GetDestinationTarget(FPDF_DEST destination,
                                                  LinkTarget* target) {
  if (!target)
    return NONSELECTABLE_AREA;

  const int page_index = FPDFDest_GetDestPageIndex(engine_->doc(), destination);
  if (page_index < 0)
    return NONSELECTABLE_AREA;

  target->page = page_index;

  std::optional<float> x;
  std::optional<float> y;
  GetPageDestinationTarget(destination, &x, &y, &target->zoom);

  // The page where a destination exists can be different from the page that it
  // targets. Calculating the in-page coordinates should be based on the target
  // page's size.
  PDFiumPage* target_page = engine_->GetPage(target->page);
  if (!target_page)
    return NONSELECTABLE_AREA;

  if (x) {
    target->x_in_pixels =
        target_page->PreProcessAndTransformInPageCoordX(x.value());
  }
  if (y) {
    target->y_in_pixels =
        target_page->PreProcessAndTransformInPageCoordY(y.value());
  }

  return DOCLINK_AREA;
}

void PDFiumPage::GetPageDestinationTarget(FPDF_DEST destination,
                                          std::optional<float>* dest_x,
                                          std::optional<float>* dest_y,
                                          std::optional<float>* zoom_value) {
  *dest_x = std::nullopt;
  *dest_y = std::nullopt;
  *zoom_value = std::nullopt;
  if (!available_)
    return;

  FPDF_BOOL has_x_coord;
  FPDF_BOOL has_y_coord;
  FPDF_BOOL has_zoom;
  FS_FLOAT x;
  FS_FLOAT y;
  FS_FLOAT zoom;
  FPDF_BOOL success = FPDFDest_GetLocationInPage(
      destination, &has_x_coord, &has_y_coord, &has_zoom, &x, &y, &zoom);

  if (!success)
    return;

  if (has_x_coord)
    *dest_x = x;
  if (has_y_coord)
    *dest_y = y;
  if (has_zoom)
    *zoom_value = zoom;
}

float PDFiumPage::PreProcessAndTransformInPageCoordX(float x) {
  // If `x` < 0, scroll to the left side of the page.
  // If `x` > page width, scroll to the right side of the page.
  return TransformPageToScreenX(
      std::clamp(x, 0.0f, FPDF_GetPageWidthF(GetPage())));
}

float PDFiumPage::PreProcessAndTransformInPageCoordY(float y) {
  // If `y` < 0, it is a valid input, no extra handling is needed.
  // If `y` > page height, scroll to the top of the page.
  return TransformPageToScreenY(std::min(y, FPDF_GetPageHeightF(GetPage())));
}

gfx::PointF PDFiumPage::TransformPageToScreenXY(const gfx::PointF& xy) {
  if (!available_)
    return gfx::PointF();

  gfx::RectF page_rect(xy.x(), xy.y(), 0, 0);
  gfx::RectF pixel_rect(FloatPageRectToPixelRect(GetPage(), page_rect));
  return gfx::PointF(pixel_rect.x(), pixel_rect.y());
}

float PDFiumPage::TransformPageToScreenX(float x) {
  return TransformPageToScreenXY(gfx::PointF(x, 0)).x();
}

float PDFiumPage::TransformPageToScreenY(float y) {
  return TransformPageToScreenXY(gfx::PointF(0, y)).y();
}

PDFiumPage::Area PDFiumPage::GetURITarget(FPDF_ACTION uri_action,
                                          LinkTarget* target) const {
  if (target) {
    std::string url = CallPDFiumStringBufferApi(
        base::BindRepeating(&FPDFAction_GetURIPath, engine_->doc(), uri_action),
        /*check_expected_size=*/true);
    if (!url.empty() && base::IsStringUTF8AllowingNoncharacters(url))
      target->url = url;
  }
  return WEBLINK_AREA;
}

int PDFiumPage::GetLink(int char_index, LinkTarget* target) {
  if (!available_)
    return -1;

  CalculateLinks();

  // Get the bounding box of the rect again, since it might have moved because
  // of the tolerance above.
  double left;
  double right;
  double bottom;
  double top;
  if (!FPDFText_GetCharBox(GetTextPage(), char_index, &left, &right, &bottom,
                           &top)) {
    return -1;
  }

  gfx::Point origin = PageToScreen(gfx::Point(), 1.0, left, top, right, bottom,
                                   PageOrientation::kOriginal)
                          .origin();
  for (size_t i = 0; i < links_.size(); ++i) {
    for (const auto& rect : links_[i].bounding_rects) {
      if (rect.Contains(origin)) {
        if (target)
          target->url = links_[i].target.url;
        return i;
      }
    }
  }
  return -1;
}

void PDFiumPage::CalculateLinks() {
  if (calculated_links_)
    return;

  calculated_links_ = true;
  PopulateWebLinks();
  PopulateAnnotationLinks();
}

void PDFiumPage::PopulateWebLinks() {
  ScopedFPDFPageLink links(FPDFLink_LoadWebLinks(GetTextPage()));
  int count = FPDFLink_CountWebLinks(links.get());
  for (int i = 0; i < count; ++i) {
    // WARNING: FPDFLink_GetURL() is not compatible with
    // CallPDFiumWideStringBufferApi().
    std::u16string url;
    int url_length = FPDFLink_GetURL(links.get(), i, nullptr, 0);
    if (url_length > 0) {
      PDFiumAPIStringBufferAdapter<std::u16string> api_string_adapter(
          &url, url_length, true);
      unsigned short* data =
          reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
      int actual_length = FPDFLink_GetURL(links.get(), i, data, url_length);
      api_string_adapter.Close(actual_length);
    }
    Link link;
    link.target.url = base::UTF16ToUTF8(url);

    if (!engine_->IsValidLink(link.target.url))
      continue;

    // Make sure all the characters in the URL are valid per RFC 1738.
    // http://crbug.com/340326 has a sample bad PDF.
    // GURL does not work correctly, e.g. it just strips \t \r \n.
    bool is_invalid_url = false;
    for (size_t j = 0; j < link.target.url.length(); ++j) {
      // Control characters are not allowed.
      // 0x7F is also a control character.
      // 0x80 and above are not in US-ASCII.
      if (link.target.url[j] < ' ' || link.target.url[j] >= '\x7F') {
        is_invalid_url = true;
        break;
      }
    }
    if (is_invalid_url)
      continue;

    int rect_count = FPDFLink_CountRects(links.get(), i);
    for (int j = 0; j < rect_count; ++j) {
      double left;
      double top;
      double right;
      double bottom;
      if (!FPDFLink_GetRect(links.get(), i, j, &left, &top, &right, &bottom))
        continue;
      gfx::Rect rect = PageToScreen(gfx::Point(), 1.0, left, top, right, bottom,
                                    PageOrientation::kOriginal);
      if (rect.IsEmpty())
        continue;
      link.bounding_rects.push_back(rect);
    }
    FPDF_BOOL is_link_over_text = FPDFLink_GetTextRange(
        links.get(), i, &link.start_char_index, &link.char_count);
    DCHECK(is_link_over_text);
    links_.push_back(link);
  }
}

void PDFiumPage::PopulateAnnotationLinks() {
  int start_pos = 0;
  FPDF_LINK link_annot;
  FPDF_PAGE page = GetPage();
  while (FPDFLink_Enumerate(page, &start_pos, &link_annot)) {
    Link link;
    Area area = GetLinkTarget(link_annot, &link.target);
    if (area == NONSELECTABLE_AREA)
      continue;

    FS_RECTF link_rect;
    if (!FPDFLink_GetAnnotRect(link_annot, &link_rect))
      continue;

    // The horizontal/vertical coordinates in PDF Links could be
    // flipped. Swap the coordinates before further processing.
    if (link_rect.right < link_rect.left)
      std::swap(link_rect.right, link_rect.left);
    if (link_rect.bottom > link_rect.top)
      std::swap(link_rect.bottom, link_rect.top);

    int quad_point_count = FPDFLink_CountQuadPoints(link_annot);
    // Calculate the bounds of link using the quad points data.
    // If quad points for link is not present then use
    // `link_rect` to calculate the bounds instead.
    if (quad_point_count > 0) {
      for (int i = 0; i < quad_point_count; ++i) {
        FS_QUADPOINTSF point;
        if (FPDFLink_GetQuadPoints(link_annot, i, &point)) {
          // PDF Specifications: Quadpoints start from bottom left (x1, y1) and
          // runs counter clockwise.
          link.bounding_rects.push_back(
              PageToScreen(gfx::Point(), 1.0, point.x4, point.y4, point.x2,
                           point.y2, PageOrientation::kOriginal));
        }
      }
    } else {
      link.bounding_rects.push_back(PageToScreen(
          gfx::Point(), 1.0, link_rect.left, link_rect.top, link_rect.right,
          link_rect.bottom, PageOrientation::kOriginal));
    }

    // Calculate underlying text range of link.
    GetUnderlyingTextRangeForRect(
        gfx::RectF(link_rect.left, link_rect.bottom,
                   std::abs(link_rect.right - link_rect.left),
                   std::abs(link_rect.bottom - link_rect.top)),
        &link.start_char_index, &link.char_count);
    links_.emplace_back(link);
  }
}

void PDFiumPage::CalculateImages() {
  if (calculated_images_)
    return;

  calculated_images_ = true;
  FPDF_PAGE page = GetPage();
  int page_object_count = FPDFPage_CountObjects(page);
  MarkedContentIdToImageMap marked_content_id_image_map;
  bool is_tagged = FPDFCatalog_IsTagged(engine_->doc());
  for (int i = 0; i < page_object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    if (FPDFPageObj_GetType(page_object) != FPDF_PAGEOBJ_IMAGE)
      continue;
    float left;
    float top;
    float right;
    float bottom;
    if (!FPDFPageObj_GetBounds(page_object, &left, &bottom, &right, &top))
      continue;

    Image image;
    image.page_object_index = i;
    image.bounding_rect = PageToScreen(gfx::Point(), 1.0, left, top, right,
                                       bottom, PageOrientation::kOriginal);

    if (is_tagged) {
      // Collect all marked content IDs for image objects so that they can
      // later be used to retrieve alt text from struct tree for the page.
      FPDF_IMAGEOBJ_METADATA image_metadata;
      if (FPDFImageObj_GetImageMetadata(page_object, page, &image_metadata)) {
        int marked_content_id = image_metadata.marked_content_id;
        if (marked_content_id >= 0) {
          // If `marked_content_id` is already present, ignore the one being
          // inserted.
          marked_content_id_image_map.insert(
              {marked_content_id, images_.size()});
        }
      }
    }
    images_.push_back(image);
  }

  if (!marked_content_id_image_map.empty())
    PopulateImageAltText(marked_content_id_image_map);
}

void PDFiumPage::PopulateImageAltText(
    const MarkedContentIdToImageMap& marked_content_id_image_map) {
  ScopedFPDFStructTree struct_tree(FPDF_StructTree_GetForPage(GetPage()));
  if (!struct_tree)
    return;

  std::set<FPDF_STRUCTELEMENT> visited_elements;
  int tree_children_count = FPDF_StructTree_CountChildren(struct_tree.get());
  for (int i = 0; i < tree_children_count; ++i) {
    FPDF_STRUCTELEMENT current_element =
        FPDF_StructTree_GetChildAtIndex(struct_tree.get(), i);
    PopulateImageAltTextForStructElement(marked_content_id_image_map,
                                         current_element, &visited_elements);
  }
}

void PDFiumPage::PopulateImageAltTextForStructElement(
    const MarkedContentIdToImageMap& marked_content_id_image_map,
    FPDF_STRUCTELEMENT current_element,
    std::set<FPDF_STRUCTELEMENT>* visited_elements) {
  if (!current_element)
    return;

  bool inserted = visited_elements->insert(current_element).second;
  if (!inserted)
    return;

  int marked_content_id =
      FPDF_StructElement_GetMarkedContentID(current_element);
  if (marked_content_id >= 0) {
    auto it = marked_content_id_image_map.find(marked_content_id);
    if (it != marked_content_id_image_map.end() &&
        images_[it->second].alt_text.empty()) {
      images_[it->second].alt_text =
          base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
              base::BindRepeating(&FPDF_StructElement_GetAltText,
                                  current_element),
              /*check_expected_size=*/true));
    }
  }
  int children_count = FPDF_StructElement_CountChildren(current_element);
  for (int i = 0; i < children_count; ++i) {
    FPDF_STRUCTELEMENT child =
        FPDF_StructElement_GetChildAtIndex(current_element, i);
    PopulateImageAltTextForStructElement(marked_content_id_image_map, child,
                                         visited_elements);
  }
}

void PDFiumPage::PopulateAnnotations() {
  if (calculated_annotations_)
    return;

  FPDF_PAGE page = GetPage();
  if (!page)
    return;

  int annotation_count = FPDFPage_GetAnnotCount(page);
  for (int i = 0; i < annotation_count; ++i) {
    ScopedFPDFAnnotation annot(FPDFPage_GetAnnot(page, i));
    DCHECK(annot);
    FPDF_ANNOTATION_SUBTYPE subtype = FPDFAnnot_GetSubtype(annot.get());

    switch (subtype) {
      case FPDF_ANNOT_HIGHLIGHT: {
        PopulateHighlight(annot.get());
        break;
      }
      case FPDF_ANNOT_WIDGET: {
        PopulateFormField(annot.get());
        break;
      }
      default:
        break;
    }
  }
  calculated_annotations_ = true;
}

void PDFiumPage::PopulateHighlight(FPDF_ANNOTATION annot) {
  DCHECK(annot);
  DCHECK_EQ(FPDFAnnot_GetSubtype(annot), FPDF_ANNOT_HIGHLIGHT);

  FS_RECTF rect;
  if (!FPDFAnnot_GetRect(annot, &rect))
    return;

  Highlight highlight;
  // We use the bounding box of the highlight as the bounding rect.
  highlight.bounding_rect =
      PageToScreen(gfx::Point(), 1.0, rect.left, rect.top, rect.right,
                   rect.bottom, PageOrientation::kOriginal);
  GetUnderlyingTextRangeForRect(
      gfx::RectF(rect.left, rect.bottom, std::abs(rect.right - rect.left),
                 std::abs(rect.bottom - rect.top)),
      &highlight.start_char_index, &highlight.char_count);

  // Retrieve the color of the highlight.
  unsigned int color_r;
  unsigned int color_g;
  unsigned int color_b;
  unsigned int color_a;
  FPDF_PAGEOBJECT page_object = FPDFAnnot_GetObject(annot, 0);
  if (FPDFPageObj_GetFillColor(page_object, &color_r, &color_g, &color_b,
                               &color_a)) {
    highlight.color = MakeARGB(color_a, color_r, color_g, color_b);
  } else {
    // Set the same default color as in pdfium. See calls to
    // GetColorStringWithDefault() in CPVT_GenerateAP::Generate*AP() in
    // pdfium.
    highlight.color = MakeARGB(255, 255, 255, 0);
  }

  // Retrieve the contents of the popup note associated with highlight. See
  // table 164 in ISO 32000-1:2008 spec for more details around "Contents" key
  // in a highlight annotation.
  static constexpr char kContents[] = "Contents";
  highlight.note_text = base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFAnnot_GetStringValue, annot, kContents),
      /*check_expected_size=*/true));

  highlights_.push_back(std::move(highlight));
}

void PDFiumPage::PopulateTextField(FPDF_ANNOTATION annot) {
  DCHECK(annot);
  FPDF_FORMHANDLE form_handle = engine_->form();
  DCHECK_EQ(FPDFAnnot_GetFormFieldType(form_handle, annot),
            FPDF_FORMFIELD_TEXTFIELD);

  TextField text_field;
  if (!PopulateFormFieldProperties(annot, &text_field))
    return;

  text_field.value = base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFAnnot_GetFormFieldValue, form_handle, annot),
      /*check_expected_size=*/true));
  text_fields_.push_back(std::move(text_field));
}

void PDFiumPage::PopulateChoiceField(FPDF_ANNOTATION annot) {
  DCHECK(annot);
  FPDF_FORMHANDLE form_handle = engine_->form();
  int form_field_type = FPDFAnnot_GetFormFieldType(form_handle, annot);
  DCHECK(form_field_type == FPDF_FORMFIELD_LISTBOX ||
         form_field_type == FPDF_FORMFIELD_COMBOBOX);

  ChoiceField choice_field;
  if (!PopulateFormFieldProperties(annot, &choice_field))
    return;

  int options_count = FPDFAnnot_GetOptionCount(form_handle, annot);
  if (options_count < 0)
    return;

  choice_field.options.resize(options_count);
  for (int i = 0; i < options_count; ++i) {
    choice_field.options[i].name =
        base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
            base::BindRepeating(&FPDFAnnot_GetOptionLabel, form_handle, annot,
                                i),
            /*check_expected_size=*/true));
    choice_field.options[i].is_selected =
        FPDFAnnot_IsOptionSelected(form_handle, annot, i);
  }
  choice_fields_.push_back(std::move(choice_field));
}

void PDFiumPage::PopulateButton(FPDF_ANNOTATION annot) {
  DCHECK(annot);
  FPDF_FORMHANDLE form_handle = engine_->form();
  int button_type = FPDFAnnot_GetFormFieldType(form_handle, annot);
  DCHECK(button_type == FPDF_FORMFIELD_PUSHBUTTON ||
         IsRadioButtonOrCheckBox(button_type));

  Button button;
  if (!PopulateFormFieldProperties(annot, &button))
    return;

  button.type = button_type;
  if (IsRadioButtonOrCheckBox(button_type)) {
    button.control_count = FPDFAnnot_GetFormControlCount(form_handle, annot);
    if (button.control_count <= 0)
      return;

    button.control_index = FPDFAnnot_GetFormControlIndex(form_handle, annot);
    button.value = base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
        base::BindRepeating(&FPDFAnnot_GetFormFieldExportValue, form_handle,
                            annot),
        /*check_expected_size=*/true));
    button.is_checked = FPDFAnnot_IsChecked(form_handle, annot);
  }
  buttons_.push_back(std::move(button));
}

void PDFiumPage::PopulateFormField(FPDF_ANNOTATION annot) {
  DCHECK_EQ(FPDFAnnot_GetSubtype(annot), FPDF_ANNOT_WIDGET);
  int form_field_type = FPDFAnnot_GetFormFieldType(engine_->form(), annot);

  // TODO(crbug.com/40661774): Populate other types of form fields too.
  switch (form_field_type) {
    case FPDF_FORMFIELD_PUSHBUTTON:
    case FPDF_FORMFIELD_CHECKBOX:
    case FPDF_FORMFIELD_RADIOBUTTON: {
      PopulateButton(annot);
      break;
    }
    case FPDF_FORMFIELD_COMBOBOX:
    case FPDF_FORMFIELD_LISTBOX: {
      PopulateChoiceField(annot);
      break;
    }
    case FPDF_FORMFIELD_TEXTFIELD: {
      PopulateTextField(annot);
      break;
    }
    default:
      break;
  }
}

bool PDFiumPage::PopulateFormFieldProperties(FPDF_ANNOTATION annot,
                                             FormField* form_field) {
  DCHECK(annot);
  FS_RECTF rect;
  if (!FPDFAnnot_GetRect(annot, &rect))
    return false;

  // We use the bounding box of the form field as the bounding rect.
  form_field->bounding_rect =
      PageToScreen(gfx::Point(), 1.0, rect.left, rect.top, rect.right,
                   rect.bottom, PageOrientation::kOriginal);
  FPDF_FORMHANDLE form_handle = engine_->form();
  form_field->name = base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFAnnot_GetFormFieldName, form_handle, annot),
      /*check_expected_size=*/true));
  form_field->flags = FPDFAnnot_GetFormFieldFlags(form_handle, annot);
  return true;
}

bool PDFiumPage::GetUnderlyingTextRangeForRect(const gfx::RectF& rect,
                                               int* start_index,
                                               int* char_len) {
  if (!available_)
    return false;

  FPDF_TEXTPAGE text_page = GetTextPage();
  int char_count = FPDFText_CountChars(text_page);
  if (char_count <= 0)
    return false;

  int start_char_index = -1;
  int cur_char_count = 0;

  // Iterate over page text to find such continuous characters whose mid-points
  // lie inside the rectangle.
  for (int i = 0; i < char_count; ++i) {
    double char_left;
    double char_right;
    double char_bottom;
    double char_top;
    if (!FPDFText_GetCharBox(text_page, i, &char_left, &char_right,
                             &char_bottom, &char_top)) {
      break;
    }

    float xmid = (char_left + char_right) / 2;
    float ymid = (char_top + char_bottom) / 2;
    if (rect.Contains(xmid, ymid)) {
      if (start_char_index == -1)
        start_char_index = i;
      ++cur_char_count;
    } else if (start_char_index != -1) {
      break;
    }
  }

  if (cur_char_count == 0)
    return false;

  *char_len = cur_char_count;
  *start_index = start_char_index;
  return true;
}

gfx::Rect PDFiumPage::PageToScreen(const gfx::Point& page_point,
                                   double zoom,
                                   double left,
                                   double top,
                                   double right,
                                   double bottom,
                                   PageOrientation orientation) const {
  if (!available_)
    return gfx::Rect();

  double start_x = (rect_.x() - page_point.x()) * zoom;
  double start_y = (rect_.y() - page_point.y()) * zoom;
  double size_x = rect_.width() * zoom;
  double size_y = rect_.height() * zoom;
  if (!base::IsValueInRangeForNumericType<int>(start_x) ||
      !base::IsValueInRangeForNumericType<int>(start_y) ||
      !base::IsValueInRangeForNumericType<int>(size_x) ||
      !base::IsValueInRangeForNumericType<int>(size_y)) {
    return gfx::Rect();
  }

  int new_left;
  int new_top;
  if (!FPDF_PageToDevice(
          page(), static_cast<int>(start_x), static_cast<int>(start_y),
          static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
          ToPDFiumRotation(orientation), left, top, &new_left, &new_top)) {
    return gfx::Rect();
  }

  int new_right;
  int new_bottom;
  if (!FPDF_PageToDevice(
          page(), static_cast<int>(start_x), static_cast<int>(start_y),
          static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
          ToPDFiumRotation(orientation), right, bottom, &new_right,
          &new_bottom)) {
    return gfx::Rect();
  }

  // If the PDF is rotated, the horizontal/vertical coordinates could be
  // flipped.  See
  // http://www.netl.doe.gov/publications/proceedings/03/ubc/presentations/Goeckner-pres.pdf
  if (new_right < new_left)
    std::swap(new_right, new_left);
  if (new_bottom < new_top)
    std::swap(new_bottom, new_top);

  base::CheckedNumeric<int32_t> new_size_x = new_right;
  new_size_x -= new_left;
  new_size_x += 1;
  base::CheckedNumeric<int32_t> new_size_y = new_bottom;
  new_size_y -= new_top;
  new_size_y += 1;
  if (!new_size_x.IsValid() || !new_size_y.IsValid())
    return gfx::Rect();

  return gfx::Rect(new_left, new_top, new_size_x.ValueOrDie(),
                   new_size_y.ValueOrDie());
}

void PDFiumPage::RequestThumbnail(float device_pixel_ratio,
                                  SendThumbnailCallback send_callback) {
  DCHECK(!thumbnail_callback_);

  if (available()) {
    GenerateAndSendThumbnail(device_pixel_ratio, std::move(send_callback));
    return;
  }

  // It is safe to use base::Unretained(this) because the callback is only used
  // by `this`.
  thumbnail_callback_ = base::BindOnce(
      &PDFiumPage::GenerateAndSendThumbnail, base::Unretained(this),
      device_pixel_ratio, std::move(send_callback));
}

Thumbnail PDFiumPage::GenerateThumbnail(float device_pixel_ratio) {
  DCHECK(available());

  Thumbnail thumbnail = GetThumbnail(device_pixel_ratio);
  const gfx::Size& image_size = thumbnail.image_size();

  ScopedFPDFBitmap fpdf_bitmap(FPDFBitmap_CreateEx(
      image_size.width(), image_size.height(), FPDFBitmap_BGRA,
      thumbnail.GetImageData().data(), thumbnail.stride()));

  // Clear the bitmap.
  FPDFBitmap_FillRect(fpdf_bitmap.get(), /*left=*/0, /*top=*/0,
                      image_size.width(), image_size.height(),
                      /*color=*/0xFFFFFFFF);

  // The combination of the `FPDF_REVERSE_BYTE_ORDER` rendering flag and the
  // `FPDFBitmap_BGRA` format when initializing `fpdf_bitmap` results in an RGBA
  // rendering, which is the format required by HTML <canvas>.
  constexpr int kRenderingFlags = FPDF_ANNOT | FPDF_REVERSE_BYTE_ORDER;
  FPDF_RenderPageBitmap(fpdf_bitmap.get(), GetPage(), /*start_x=*/0,
                        /*start_y=*/0, image_size.width(), image_size.height(),
                        ToPDFiumRotation(PageOrientation::kOriginal),
                        kRenderingFlags);

  // Draw the forms.
  FPDF_FFLDraw(engine_->form(), fpdf_bitmap.get(), GetPage(), /*start_x=*/0,
               /*start_y=*/0, image_size.width(), image_size.height(),
               ToPDFiumRotation(PageOrientation::kOriginal), kRenderingFlags);

  return thumbnail;
}

#if BUILDFLAG(ENABLE_PDF_INK2)
gfx::Size PDFiumPage::GetThumbnailSize(float device_pixel_ratio) {
  return GetThumbnail(device_pixel_ratio).image_size();
}
#endif

void PDFiumPage::GenerateAndSendThumbnail(float device_pixel_ratio,
                                          SendThumbnailCallback send_callback) {
  std::move(send_callback).Run(GenerateThumbnail(device_pixel_ratio));
}

Thumbnail PDFiumPage::GetThumbnail(float device_pixel_ratio) {
  CHECK(available());

  FPDF_PAGE page = GetPage();
  gfx::Size page_size(base::saturated_cast<int>(FPDF_GetPageWidthF(page)),
                      base::saturated_cast<int>(FPDF_GetPageHeightF(page)));
  return Thumbnail(page_size, device_pixel_ratio);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
bool PDFiumPage::IsCharacterGeneratedBySearchify(int char_index) {
  if (!IsPageSearchified()) {
    return false;
  }

  FPDF_PAGE page = GetPage();
  int objects_count = FPDFPage_CountObjects(page);
  FPDF_PAGEOBJECT object = FPDFText_GetTextObject(GetTextPage(), char_index);
  for (int i = first_searchify_generated_object_index_; i < objects_count;
       ++i) {
    if (object == FPDFPage_GetObject(page, i)) {
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void PDFiumPage::MarkAvailable() {
  available_ = true;

  // Fulfill pending thumbnail request.
  if (thumbnail_callback_)
    std::move(thumbnail_callback_).Run();
}

PDFiumPage::ScopedUnloadPreventer::ScopedUnloadPreventer(PDFiumPage* page)
    : page_(page) {
  page_->preventing_unload_count_++;
}

PDFiumPage::ScopedUnloadPreventer::ScopedUnloadPreventer(
    const ScopedUnloadPreventer& that)
    : ScopedUnloadPreventer(that.page_) {}

PDFiumPage::ScopedUnloadPreventer& PDFiumPage::ScopedUnloadPreventer::operator=(
    const ScopedUnloadPreventer& that) {
  if (page_ != that.page_) {
    page_->preventing_unload_count_--;
    page_ = that.page_;
    page_->preventing_unload_count_++;
  }
  return *this;
}

PDFiumPage::ScopedUnloadPreventer::~ScopedUnloadPreventer() {
  page_->preventing_unload_count_--;
}

PDFiumPage::Link::Link() = default;

PDFiumPage::Link::Link(const Link& that) = default;

PDFiumPage::Link::~Link() = default;

PDFiumPage::Image::Image() = default;

PDFiumPage::Image::Image(const Image& that) = default;

PDFiumPage::Image::~Image() = default;

PDFiumPage::Highlight::Highlight() = default;

PDFiumPage::Highlight::Highlight(const Highlight& that) = default;

PDFiumPage::Highlight::~Highlight() = default;

PDFiumPage::FormField::FormField() = default;

PDFiumPage::FormField::FormField(const FormField& that) = default;

PDFiumPage::FormField::~FormField() = default;

PDFiumPage::TextField::TextField() = default;

PDFiumPage::TextField::TextField(const TextField& that) = default;

PDFiumPage::TextField::~TextField() = default;

PDFiumPage::ChoiceFieldOption::ChoiceFieldOption() = default;

PDFiumPage::ChoiceFieldOption::ChoiceFieldOption(
    const ChoiceFieldOption& that) = default;

PDFiumPage::ChoiceFieldOption::~ChoiceFieldOption() = default;

PDFiumPage::ChoiceField::ChoiceField() = default;

PDFiumPage::ChoiceField::ChoiceField(const ChoiceField& that) = default;

PDFiumPage::ChoiceField::~ChoiceField() = default;

PDFiumPage::Button::Button() = default;

PDFiumPage::Button::Button(const Button& that) = default;

PDFiumPage::Button::~Button() = default;

}  // namespace chrome_pdf
