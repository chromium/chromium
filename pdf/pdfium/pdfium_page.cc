// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_catalog.h"

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using printing::kPixelsPerInch;

namespace chrome_pdf {

namespace {

constexpr double k45DegreesInRadians = base::kPiDouble / 4;
constexpr double k90DegreesInRadians = base::kPiDouble / 2;
constexpr double k180DegreesInRadians = base::kPiDouble;
constexpr double k270DegreesInRadians = 3 * base::kPiDouble / 2;
constexpr double k360DegreesInRadians = 2 * base::kPiDouble;

PDFiumPage::IsValidLinkFunction g_is_valid_link_func_for_testing = nullptr;

// If the link cannot be converted to a pp::Var, then it is not possible to
// pass it to JS. In this case, ignore the link like other PDF viewers.
// See https://crbug.com/312882 for an example.
// TODO(crbug.com/702993): Get rid of the PPAPI usage here, as well as
// SetIsValidLinkFunctionForTesting() and related code.
bool IsValidLink(const std::string& url) {
  return pp::Var(url).is_string();
}

pp::FloatRect FloatPageRectToPixelRect(FPDF_PAGE page,
                                       const pp::FloatRect& input) {
  int output_width = FPDF_GetPageWidth(page);
  int output_height = FPDF_GetPageHeight(page);

  int min_x;
  int min_y;
  int max_x;
  int max_y;
  FPDF_BOOL ret = FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0,
                                    input.x(), input.y(), &min_x, &min_y);
  DCHECK(ret);
  ret = FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0,
                          input.right(), input.bottom(), &max_x, &max_y);
  DCHECK(ret);

  if (max_x < min_x)
    std::swap(min_x, max_x);
  if (max_y < min_y)
    std::swap(min_y, max_y);

  pp::FloatRect output_rect(
      ConvertUnitDouble(min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(min_y, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_x - min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_y - min_y, kPointsPerInch, kPixelsPerInch));
  return output_rect;
}

pp::FloatRect GetFloatCharRectInPixels(FPDF_PAGE page,
                                       FPDF_TEXTPAGE text_page,
                                       int index) {
  double left;
  double right;
  double bottom;
  double top;
  FPDFText_GetCharBox(text_page, index, &left, &right, &bottom, &top);
  if (right < left)
    std::swap(left, right);
  if (bottom < top)
    std::swap(top, bottom);
  pp::FloatRect page_coords(left, top, right - left, bottom - top);
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

PP_PrivateDirection GetDirectionFromAngle(double angle) {
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

  angle = fmod(angle + k45DegreesInRadians, k360DegreesInRadians);
  // Quadrant I.
  if (angle >= 0 && angle <= k90DegreesInRadians)
    return PP_PRIVATEDIRECTION_LTR;
  // Quadrant II.
  if (angle > k90DegreesInRadians && angle <= k180DegreesInRadians)
    return PP_PRIVATEDIRECTION_TTB;
  // Quadrant III.
  if (angle > k180DegreesInRadians && angle <= k270DegreesInRadians)
    return PP_PRIVATEDIRECTION_RTL;
  // Quadrant IV.
  return PP_PRIVATEDIRECTION_BTT;
}

double GetDistanceBetweenPoints(const pp::FloatPoint& p1,
                                const pp::FloatPoint& p2) {
  pp::FloatPoint dist_vector = p1 - p2;
  return sqrt(pow(dist_vector.x(), 2) + pow(dist_vector.y(), 2));
}

void AddCharSizeToAverageCharSize(pp::FloatSize new_size,
                                  pp::FloatSize* avg_size,
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

double GetRotatedCharWidth(double angle, const pp::FloatSize& size) {
  return abs(cos(angle) * size.width()) + abs(sin(angle) * size.height());
}

double GetAngleOfVector(const pp::FloatPoint& v) {
  double angle = atan2(v.y(), v.x());
  if (angle < 0)
    angle += k360DegreesInRadians;
  return angle;
}

double GetAngleDifference(double a, double b) {
  // This is either the difference or (360 - difference).
  double x = fmod(fabs(b - a), k360DegreesInRadians);
  return x > k180DegreesInRadians ? k360DegreesInRadians - x : x;
}

bool DoubleEquals(double d1, double d2) {
  // The idea behind this is to use this fraction of the larger of the
  // two numbers as the limit of the difference.  This breaks down near
  // zero, so we reuse this as the minimum absolute size we will use
  // for the base of the scale too.
  static const float epsilon_scale = 0.00001f;
  return fabs(d1 - d2) <
         epsilon_scale *
             std::fmax(std::fmax(std::fabs(d1), std::fabs(d2)), epsilon_scale);
}

uint32_t MakeARGB(unsigned int a,
                  unsigned int r,
                  unsigned int g,
                  unsigned int b) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

}  // namespace

PDFiumPage::LinkTarget::LinkTarget() : page(-1) {}

PDFiumPage::LinkTarget::LinkTarget(const LinkTarget& other) = default;

PDFiumPage::LinkTarget::~LinkTarget() = default;

PDFiumPage::PDFiumPage(PDFiumEngine* engine, int i)
    : engine_(engine), index_(i), available_(false) {}

PDFiumPage::PDFiumPage(PDFiumPage&& that) = default;

PDFiumPage::~PDFiumPage() {
  DCHECK_EQ(0, preventing_unload_count_);
}

// static
void PDFiumPage::SetIsValidLinkFunctionForTesting(
    IsValidLinkFunction function) {
  g_is_valid_link_func_for_testing = function;
}

void PDFiumPage::Unload() {
  // Do not unload while in the middle of a load.
  if (preventing_unload_count_)
    return;

  text_page_.reset();

  if (page_) {
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
    if (page_ && engine_->form()) {
      FORM_OnAfterLoadPage(page(), engine_->form());
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
}

void PDFiumPage::CalculateTextRunStyleInfo(
    int char_index,
    pp::PDF::PrivateAccessibilityTextStyleInfo* style_info) {
  FPDF_TEXTPAGE text_page = GetTextPage();
  style_info->font_size = FPDFText_GetFontSize(text_page, char_index);

  int flags = 0;
  size_t buffer_size =
      FPDFText_GetFontInfo(text_page, char_index, nullptr, 0, &flags);
  if (buffer_size > 0) {
    PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
        &style_info->font_name, buffer_size, true);
    void* data = api_string_adapter.GetData();
    size_t bytes_written =
        FPDFText_GetFontInfo(text_page, char_index, data, buffer_size, nullptr);
    // Trim the null character.
    api_string_adapter.Close(bytes_written);
  }

  style_info->font_weight = FPDFText_GetFontWeight(text_page, char_index);
  // As defined in PDF 1.7 table 5.20.
  constexpr int kFlagItalic = (1 << 6);
  // Bold text is considered bold when greater than or equal to 700.
  constexpr int kStandardBoldValue = 700;
  style_info->is_italic = (flags & kFlagItalic);
  style_info->is_bold = style_info->font_weight >= kStandardBoldValue;
  unsigned int fill_r;
  unsigned int fill_g;
  unsigned int fill_b;
  unsigned int fill_a;
  if (FPDFText_GetFillColor(text_page, char_index, &fill_r, &fill_g, &fill_b,
                            &fill_a)) {
    style_info->fill_color = MakeARGB(fill_a, fill_r, fill_g, fill_b);
  } else {
    style_info->fill_color = MakeARGB(0xff, 0, 0, 0);
  }

  unsigned int stroke_r;
  unsigned int stroke_g;
  unsigned int stroke_b;
  unsigned int stroke_a;
  if (FPDFText_GetStrokeColor(text_page, char_index, &stroke_r, &stroke_g,
                              &stroke_b, &stroke_a)) {
    style_info->stroke_color = MakeARGB(stroke_a, stroke_r, stroke_g, stroke_b);
  } else {
    style_info->stroke_color = MakeARGB(0xff, 0, 0, 0);
  }

  int render_mode = FPDFText_GetTextRenderMode(text_page, char_index);
  if (render_mode < 0 || render_mode > PP_TEXTRENDERINGMODE_LAST) {
    style_info->render_mode = PP_TEXTRENDERINGMODE_UNKNOWN;
  } else {
    style_info->render_mode = static_cast<PP_TextRenderingMode>(render_mode);
  }
}

bool PDFiumPage::AreTextStyleEqual(
    int char_index,
    const pp::PDF::PrivateAccessibilityTextStyleInfo& style) {
  pp::PDF::PrivateAccessibilityTextStyleInfo char_style;
  CalculateTextRunStyleInfo(char_index, &char_style);
  return char_style.font_name == style.font_name &&
         char_style.font_weight == style.font_weight &&
         char_style.render_mode == style.render_mode &&
         DoubleEquals(char_style.font_size, style.font_size) &&
         char_style.fill_color == style.fill_color &&
         char_style.stroke_color == style.stroke_color &&
         char_style.is_italic == style.is_italic &&
         char_style.is_bold == style.is_bold;
}

base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo>
PDFiumPage::GetTextRunInfo(int start_char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  int chars_count = FPDFText_CountChars(text_page);
  // Check to make sure |start_char_index| is within bounds.
  if (start_char_index < 0 || start_char_index >= chars_count)
    return base::nullopt;

  int actual_start_char_index = GetFirstNonUnicodeWhiteSpaceCharIndex(
      text_page, start_char_index, chars_count);
  // Check to see if GetFirstNonUnicodeWhiteSpaceCharIndex() iterated through
  // all the characters.
  if (actual_start_char_index >= chars_count) {
    // If so, |info.len| needs to take the number of characters
    // iterated into account.
    DCHECK_GT(actual_start_char_index, start_char_index);
    pp::PDF::PrivateAccessibilityTextRunInfo info;
    info.len = chars_count - start_char_index;
    info.bounds = pp::FloatRect();
    info.direction = PP_PRIVATEDIRECTION_NONE;
    return info;
  }
  int char_index = actual_start_char_index;

  // Set text run's style info from the first character of the text run.
  pp::PDF::PrivateAccessibilityTextRunInfo info;
  CalculateTextRunStyleInfo(char_index, &info.style);

  pp::FloatRect start_char_rect =
      GetFloatCharRectInPixels(page, text_page, char_index);
  double text_run_font_size = info.style.font_size;

  // Heuristic: Initialize the average character size to one-third of the font
  // size to avoid having the first few characters misrepresent the average.
  // Without it, if a text run starts with a '.', its small bounding box could
  // lead to a break in the text run after only one space. Ex: ". Hello World"
  // would be split in two runs: "." and "Hello World".
  double font_size_minimum = FPDFText_GetFontSize(text_page, char_index) / 3.0;
  pp::FloatSize avg_char_size =
      pp::FloatSize(font_size_minimum, font_size_minimum);
  int non_whitespace_chars_count = 1;
  AddCharSizeToAverageCharSize(start_char_rect.Floatsize(), &avg_char_size,
                               &non_whitespace_chars_count);

  // Add first char to text run.
  pp::FloatRect text_run_bounds = start_char_rect;
  PP_PrivateDirection char_direction =
      GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index));
  if (char_index < chars_count)
    char_index++;

  pp::FloatRect prev_char_rect = start_char_rect;
  float estimated_font_size =
      std::max(start_char_rect.width(), start_char_rect.height());

  // The angle of the vector starting at the first character center-point and
  // ending at the current last character center-point.
  double text_run_angle = 0;

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
    pp::FloatRect char_rect =
        GetFloatCharRectInPixels(page, text_page, char_index);

    if (!base::IsUnicodeWhitespace(character)) {
      // Heuristic: End the text run if the text style of the current character
      // is different from the text run's style.
      if (!AreTextStyleEqual(char_index, info.style))
        break;

      // Heuristic: End text run if character isn't going in the same direction.
      if (char_direction !=
          GetDirectionFromAngle(FPDFText_GetCharAngle(text_page, char_index)))
        break;

      // Heuristic: End the text run if the difference between the text run
      // angle and the angle between the center-points of the previous and
      // current characters is greater than 90 degrees.
      double current_angle = GetAngleOfVector(char_rect.CenterPoint() -
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
      AddCharSizeToAverageCharSize(char_rect.Floatsize(), &avg_char_size,
                                   &non_whitespace_chars_count);

      double avg_char_width = GetRotatedCharWidth(current_angle, avg_char_size);

      double distance =
          GetDistanceBetweenPoints(char_rect.CenterPoint(),
                                   prev_char_rect.CenterPoint()) -
          GetRotatedCharWidth(current_angle, char_rect.Floatsize()) / 2 -
          GetRotatedCharWidth(current_angle, prev_char_rect.Floatsize()) / 2;

      if (distance > 2.5 * avg_char_width)
        break;

      text_run_bounds = text_run_bounds.Union(char_rect);
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
                       : PP_PRIVATEDIRECTION_NONE;
  return info;
}

uint32_t PDFiumPage::GetCharUnicode(int char_index) {
  FPDF_TEXTPAGE text_page = GetTextPage();
  return FPDFText_GetUnicode(text_page, char_index);
}

pp::FloatRect PDFiumPage::GetCharBounds(int char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  return GetFloatCharRectInPixels(page, text_page, char_index);
}

std::vector<PDFEngine::AccessibilityLinkInfo> PDFiumPage::GetLinkInfo() {
  std::vector<PDFEngine::AccessibilityLinkInfo> link_info;
  if (!available_)
    return link_info;

  CalculateLinks();

  link_info.reserve(links_.size());
  for (const Link& link : links_) {
    PDFEngine::AccessibilityLinkInfo cur_info;
    cur_info.url = link.target.url;
    cur_info.start_char_index = link.start_char_index;
    cur_info.char_count = link.char_count;

    pp::Rect link_rect;
    for (const auto& rect : link.bounding_rects)
      link_rect = link_rect.Union(rect);
    cur_info.bounds = pp::FloatRect(link_rect.x(), link_rect.y(),
                                    link_rect.width(), link_rect.height());

    link_info.push_back(std::move(cur_info));
  }
  return link_info;
}

std::vector<PDFEngine::AccessibilityImageInfo> PDFiumPage::GetImageInfo() {
  std::vector<PDFEngine::AccessibilityImageInfo> image_info;
  if (!available_)
    return image_info;

  CalculateImages();

  image_info.reserve(images_.size());
  for (const Image& image : images_) {
    PDFEngine::AccessibilityImageInfo cur_info;
    cur_info.alt_text = image.alt_text;
    cur_info.bounds = pp::FloatRect(
        image.bounding_rect.x(), image.bounding_rect.y(),
        image.bounding_rect.width(), image.bounding_rect.height());
    image_info.push_back(std::move(cur_info));
  }
  return image_info;
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

PDFiumPage::Area PDFiumPage::GetCharIndex(const pp::Point& point,
                                          PageOrientation orientation,
                                          int* char_index,
                                          int* form_type,
                                          LinkTarget* target) {
  if (!available_)
    return NONSELECTABLE_AREA;
  pp::Point point2 = point - rect_.point();
  double new_x;
  double new_y;
  FPDF_BOOL ret = FPDF_DeviceToPage(
      GetPage(), 0, 0, rect_.width(), rect_.height(),
      ToPDFiumRotation(orientation), point2.x(), point2.y(), &new_x, &new_y);
  DCHECK(ret);

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

base::char16 PDFiumPage::GetCharAtIndex(int index) {
  if (!available_)
    return L'\0';
  return static_cast<base::char16>(FPDFText_GetUnicode(GetTextPage(), index));
}

int PDFiumPage::GetCharCount() {
  if (!available_)
    return 0;
  return FPDFText_CountChars(GetTextPage());
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
      // TODO(crbug.com/55776): We don't fully support all types of the
      // in-document links.
      return NONSELECTABLE_AREA;
    }
    case PDFACTION_URI:
      return GetURITarget(action, target);
    // TODO(crbug.com/767191): Support PDFACTION_LAUNCH.
    // TODO(crbug.com/142344): Support PDFACTION_REMOTEGOTO.
    case PDFACTION_LAUNCH:
    case PDFACTION_REMOTEGOTO:
    default:
      return NONSELECTABLE_AREA;
  }
}

PDFiumPage::Area PDFiumPage::GetDestinationTarget(FPDF_DEST destination,
                                                  LinkTarget* target) {
  if (!target)
    return NONSELECTABLE_AREA;

  int page_index = FPDFDest_GetDestPageIndex(engine_->doc(), destination);
  if (page_index < 0)
    return NONSELECTABLE_AREA;

  target->page = page_index;

  base::Optional<gfx::PointF> xy;
  GetPageDestinationTarget(destination, &xy, &target->zoom);
  if (xy) {
    gfx::PointF point = TransformPageToScreenXY(xy.value());
    target->x_in_pixels = point.x();
    target->y_in_pixels = point.y();
  }

  return DOCLINK_AREA;
}

void PDFiumPage::GetPageDestinationTarget(FPDF_DEST destination,
                                          base::Optional<gfx::PointF>* xy,
                                          base::Optional<float>* zoom_value) {
  *xy = base::nullopt;
  *zoom_value = base::nullopt;
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

  if (has_x_coord && has_y_coord)
    *xy = gfx::PointF(x, y);

  if (has_zoom)
    *zoom_value = zoom;
}

gfx::PointF PDFiumPage::TransformPageToScreenXY(const gfx::PointF& xy) {
  if (!available_)
    return gfx::PointF();

  pp::FloatRect page_rect(xy.x(), xy.y(), 0, 0);
  pp::FloatRect pixel_rect(FloatPageRectToPixelRect(GetPage(), page_rect));
  return gfx::PointF(pixel_rect.x(), pixel_rect.y());
}

PDFiumPage::Area PDFiumPage::GetURITarget(FPDF_ACTION uri_action,
                                          LinkTarget* target) const {
  if (target) {
    size_t buffer_size =
        FPDFAction_GetURIPath(engine_->doc(), uri_action, nullptr, 0);
    if (buffer_size > 0) {
      PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
          &target->url, buffer_size, true);
      void* data = api_string_adapter.GetData();
      size_t bytes_written =
          FPDFAction_GetURIPath(engine_->doc(), uri_action, data, buffer_size);
      api_string_adapter.Close(bytes_written);
    }
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
  FPDFText_GetCharBox(GetTextPage(), char_index, &left, &right, &bottom, &top);

  pp::Point origin(PageToScreen(pp::Point(), 1.0, left, top, right, bottom,
                                PageOrientation::kOriginal)
                       .point());
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
    base::string16 url;
    int url_length = FPDFLink_GetURL(links.get(), i, nullptr, 0);
    if (url_length > 0) {
      PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
          &url, url_length, true);
      unsigned short* data =
          reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
      int actual_length = FPDFLink_GetURL(links.get(), i, data, url_length);
      api_string_adapter.Close(actual_length);
    }
    Link link;
    link.target.url = base::UTF16ToUTF8(url);

    IsValidLinkFunction is_valid_link_func =
        g_is_valid_link_func_for_testing ? g_is_valid_link_func_for_testing
                                         : &IsValidLink;
    if (!is_valid_link_func(link.target.url))
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
      FPDFLink_GetRect(links.get(), i, j, &left, &top, &right, &bottom);
      pp::Rect rect = PageToScreen(pp::Point(), 1.0, left, top, right, bottom,
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
    // |link_rect| to calculate the bounds instead.
    if (quad_point_count > 0) {
      for (int i = 0; i < quad_point_count; ++i) {
        FS_QUADPOINTSF point;
        if (FPDFLink_GetQuadPoints(link_annot, i, &point)) {
          // PDF Specifications: Quadpoints start from bottom left (x1, y1) and
          // runs counter clockwise.
          link.bounding_rects.push_back(
              PageToScreen(pp::Point(), 1.0, point.x4, point.y4, point.x2,
                           point.y2, PageOrientation::kOriginal));
        }
      }
    } else {
      link.bounding_rects.push_back(PageToScreen(
          pp::Point(), 1.0, link_rect.left, link_rect.top, link_rect.right,
          link_rect.bottom, PageOrientation::kOriginal));
    }

    // Calculate underlying text range of link.
    GetUnderlyingTextRangeForRect(
        pp::FloatRect(link_rect.left, link_rect.bottom,
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
    FPDF_BOOL ret =
        FPDFPageObj_GetBounds(page_object, &left, &bottom, &right, &top);
    DCHECK(ret);
    Image image;
    image.bounding_rect = PageToScreen(pp::Point(), 1.0, left, top, right,
                                       bottom, PageOrientation::kOriginal);

    if (is_tagged) {
      // Collect all marked content IDs for image objects so that they can
      // later be used to retrieve alt text from struct tree for the page.
      FPDF_IMAGEOBJ_METADATA image_metadata;
      if (FPDFImageObj_GetImageMetadata(page_object, page, &image_metadata)) {
        int marked_content_id = image_metadata.marked_content_id;
        if (marked_content_id >= 0) {
          // If |marked_content_id| is already present, ignore the one being
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
      size_t buffer_size =
          FPDF_StructElement_GetAltText(current_element, nullptr, 0);
      if (buffer_size > 0) {
        base::string16 alt_text;
        PDFiumAPIStringBufferSizeInBytesAdapter<base::string16>
            api_string_adapter(&alt_text, buffer_size, true);
        api_string_adapter.Close(FPDF_StructElement_GetAltText(
            current_element, api_string_adapter.GetData(), buffer_size));
        images_[it->second].alt_text = base::UTF16ToUTF8(alt_text);
      }
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

bool PDFiumPage::GetUnderlyingTextRangeForRect(const pp::FloatRect& rect,
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

pp::Rect PDFiumPage::PageToScreen(const pp::Point& offset,
                                  double zoom,
                                  double left,
                                  double top,
                                  double right,
                                  double bottom,
                                  PageOrientation orientation) const {
  if (!available_)
    return pp::Rect();

  double start_x = (rect_.x() - offset.x()) * zoom;
  double start_y = (rect_.y() - offset.y()) * zoom;
  double size_x = rect_.width() * zoom;
  double size_y = rect_.height() * zoom;
  if (!base::IsValueInRangeForNumericType<int>(start_x) ||
      !base::IsValueInRangeForNumericType<int>(start_y) ||
      !base::IsValueInRangeForNumericType<int>(size_x) ||
      !base::IsValueInRangeForNumericType<int>(size_y)) {
    return pp::Rect();
  }

  int new_left;
  int new_top;
  int new_right;
  int new_bottom;
  FPDF_BOOL ret = FPDF_PageToDevice(
      page(), static_cast<int>(start_x), static_cast<int>(start_y),
      static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
      ToPDFiumRotation(orientation), left, top, &new_left, &new_top);
  DCHECK(ret);
  ret = FPDF_PageToDevice(
      page(), static_cast<int>(start_x), static_cast<int>(start_y),
      static_cast<int>(ceil(size_x)), static_cast<int>(ceil(size_y)),
      ToPDFiumRotation(orientation), right, bottom, &new_right, &new_bottom);
  DCHECK(ret);

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
    return pp::Rect();

  return pp::Rect(new_left, new_top, new_size_x.ValueOrDie(),
                  new_size_y.ValueOrDie());
}

const PDFEngine::PageFeatures* PDFiumPage::GetPageFeatures() {
  // If page_features_ is cached, return the cached features.
  if (page_features_.IsInitialized())
    return &page_features_;

  FPDF_PAGE page = GetPage();
  if (!page)
    return nullptr;

  // Initialize and cache page_features_.
  page_features_.index = index_;
  int annotation_count = FPDFPage_GetAnnotCount(page);
  for (int i = 0; i < annotation_count; ++i) {
    ScopedFPDFAnnotation annotation(FPDFPage_GetAnnot(page, i));
    FPDF_ANNOTATION_SUBTYPE subtype = FPDFAnnot_GetSubtype(annotation.get());
    page_features_.annotation_types.insert(subtype);
  }

  return &page_features_;
}

PDFiumPage::ScopedUnloadPreventer::ScopedUnloadPreventer(PDFiumPage* page)
    : page_(page) {
  page_->preventing_unload_count_++;
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

int ToPDFiumRotation(PageOrientation orientation) {
  // Could static_cast<int>(orientation), but using an exhaustive switch will
  // trigger an error if we ever change the definition of PageOrientation.
  switch (orientation) {
    case PageOrientation::kOriginal:
      return 0;
    case PageOrientation::kClockwise90:
      return 1;
    case PageOrientation::kClockwise180:
      return 2;
    case PageOrientation::kClockwise270:
      return 3;
  }
  NOTREACHED();
  return 0;
}

}  // namespace chrome_pdf
