// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_math.h"
#include "pdf/pdf_engine.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "ppapi/cpp/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

bool IsCharWithinTextRun(
    const pp::PDF::PrivateAccessibilityTextRunInfo& text_run,
    uint32_t text_run_start_char_index,
    uint32_t char_index) {
  return char_index >= text_run_start_char_index &&
         char_index - text_run_start_char_index < text_run.len;
}

bool GetEnclosingTextRunRangeForCharRange(
    const std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>& text_runs,
    int start_char_index,
    int char_count,
    uint32_t* start_text_run_index,
    uint32_t* text_run_count) {
  if (start_char_index < 0 || char_count <= 0)
    return false;

  base::CheckedNumeric<uint32_t> checked_end_char_index = char_count - 1;
  checked_end_char_index += start_char_index;
  if (!checked_end_char_index.IsValid())
    return false;
  uint32_t end_char_index = checked_end_char_index.ValueOrDie();
  uint32_t current_char_index = 0;
  base::Optional<uint32_t> start_text_run;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    if (!start_text_run.has_value() &&
        IsCharWithinTextRun(text_runs[i], current_char_index,
                            start_char_index)) {
      start_text_run = i;
    }

    if (start_text_run.has_value() &&
        IsCharWithinTextRun(text_runs[i], current_char_index, end_char_index)) {
      *start_text_run_index = start_text_run.value();
      *text_run_count = i - *start_text_run_index + 1;
      return true;
    }
    current_char_index += text_runs[i].len;
  }
  return false;
}

template <typename T>
bool CompareTextRuns(const T& a, const T& b) {
  return a.text_run_index < b.text_run_index;
}

void GetAccessibilityLinkInfo(
    PDFEngine* engine,
    int32_t page_index,
    const std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>& text_runs,
    std::vector<pp::PDF::PrivateAccessibilityLinkInfo>* links) {
  std::vector<PDFEngine::AccessibilityLinkInfo> engine_link_info =
      engine->GetLinkInfo(page_index);
  for (size_t i = 0; i < engine_link_info.size(); ++i) {
    auto& cur_engine_info = engine_link_info[i];
    pp::PDF::PrivateAccessibilityLinkInfo link_info;
    link_info.url = std::move(cur_engine_info.url);
    link_info.index_in_page = i;
    link_info.bounds = PPFloatRectFromRectF(cur_engine_info.bounds);

    if (!GetEnclosingTextRunRangeForCharRange(
            text_runs, cur_engine_info.start_char_index,
            cur_engine_info.char_count, &link_info.text_run_index,
            &link_info.text_run_count)) {
      // If a valid text run range is not found for the link, set the fallback
      // values of |text_run_index| and |text_run_count| for |link_info|.
      link_info.text_run_index = text_runs.size();
      link_info.text_run_count = 0;
    }
    links->push_back(std::move(link_info));
  }
  std::sort(links->begin(), links->end(),
            [](const pp::PDF::PrivateAccessibilityLinkInfo& a,
               const pp::PDF::PrivateAccessibilityLinkInfo& b) {
              return a.text_run_index < b.text_run_index;
            });
}

void GetAccessibilityImageInfo(
    PDFEngine* engine,
    int32_t page_index,
    uint32_t text_run_count,
    std::vector<pp::PDF::PrivateAccessibilityImageInfo>* images) {
  std::vector<PDFEngine::AccessibilityImageInfo> engine_image_info =
      engine->GetImageInfo(page_index);
  for (auto& cur_engine_info : engine_image_info) {
    pp::PDF::PrivateAccessibilityImageInfo image_info;
    image_info.alt_text = std::move(cur_engine_info.alt_text);
    image_info.bounds = PPFloatRectFromRectF(cur_engine_info.bounds);
    // TODO(mohitb): Update text run index to nearest text run to image bounds.
    image_info.text_run_index = text_run_count;
    images->push_back(std::move(image_info));
  }
}

void GetAccessibilityHighlightInfo(
    PDFEngine* engine,
    int32_t page_index,
    const std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>& text_runs,
    std::vector<pp::PDF::PrivateAccessibilityHighlightInfo>* highlights) {
  std::vector<PDFEngine::AccessibilityHighlightInfo> engine_highlight_info =
      engine->GetHighlightInfo(page_index);
  for (size_t i = 0; i < engine_highlight_info.size(); ++i) {
    auto& cur_highlight_info = engine_highlight_info[i];
    pp::PDF::PrivateAccessibilityHighlightInfo highlight_info;
    highlight_info.index_in_page = i;
    highlight_info.bounds = PPFloatRectFromRectF(cur_highlight_info.bounds);
    highlight_info.color = cur_highlight_info.color;
    highlight_info.note_text = std::move(cur_highlight_info.note_text);

    if (!GetEnclosingTextRunRangeForCharRange(
            text_runs, cur_highlight_info.start_char_index,
            cur_highlight_info.char_count, &highlight_info.text_run_index,
            &highlight_info.text_run_count)) {
      // If a valid text run range is not found for the highlight, set the
      // fallback values of |text_run_index| and |text_run_count| for
      // |highlight_info|.
      highlight_info.text_run_index = text_runs.size();
      highlight_info.text_run_count = 0;
    }
    highlights->push_back(std::move(highlight_info));
  }

  std::sort(highlights->begin(), highlights->end(),
            CompareTextRuns<pp::PDF::PrivateAccessibilityHighlightInfo>);
}

void GetAccessibilityTextFieldInfo(
    PDFEngine* engine,
    int32_t page_index,
    uint32_t text_run_count,
    std::vector<pp::PDF::PrivateAccessibilityTextFieldInfo>* text_fields) {
  std::vector<PDFEngine::AccessibilityTextFieldInfo> engine_text_field_info =
      engine->GetTextFieldInfo(page_index);
  for (size_t i = 0; i < engine_text_field_info.size(); ++i) {
    auto& cur_text_field_info = engine_text_field_info[i];
    pp::PDF::PrivateAccessibilityTextFieldInfo text_field_info;
    text_field_info.name = std::move(cur_text_field_info.name);
    text_field_info.value = std::move(cur_text_field_info.value);
    text_field_info.index_in_page = i;
    text_field_info.is_read_only = cur_text_field_info.is_read_only;
    text_field_info.is_required = cur_text_field_info.is_required;
    text_field_info.is_password = cur_text_field_info.is_password;
    // TODO(crbug.com/1030242): Update text run index to nearest text run to
    // text field bounds.
    text_field_info.text_run_index = text_run_count;
    text_field_info.bounds = PPFloatRectFromRectF(cur_text_field_info.bounds);
    text_fields->push_back(std::move(text_field_info));
  }
}

void GetAccessibilityFormFieldInfo(
    PDFEngine* engine,
    int32_t page_index,
    uint32_t text_run_count,
    pp::PDF::PrivateAccessibilityFormFieldInfo* form_fields) {
  GetAccessibilityTextFieldInfo(engine, page_index, text_run_count,
                                &form_fields->text_fields);
}

}  // namespace

bool GetAccessibilityInfo(
    PDFEngine* engine,
    int32_t page_index,
    PP_PrivateAccessibilityPageInfo* page_info,
    std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>* text_runs,
    std::vector<PP_PrivateAccessibilityCharInfo>* chars,
    pp::PDF::PrivateAccessibilityPageObjects* page_objects) {
  int page_count = engine->GetNumberOfPages();
  if (page_index < 0 || page_index >= page_count)
    return false;

  int char_count = engine->GetCharCount(page_index);

  // Treat a char count of -1 (error) as 0 (an empty page), since
  // other pages might have valid content.
  if (char_count < 0)
    char_count = 0;

  page_info->page_index = page_index;
  page_info->bounds = PPRectFromRect(engine->GetPageBoundsRect(page_index));
  page_info->char_count = char_count;

  chars->resize(page_info->char_count);
  for (uint32_t i = 0; i < page_info->char_count; ++i) {
    (*chars)[i].unicode_character = engine->GetCharUnicode(page_index, i);
  }

  int char_index = 0;
  while (char_index < char_count) {
    base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo>
        text_run_info_result = engine->GetTextRunInfo(page_index, char_index);
    DCHECK(text_run_info_result.has_value());
    const auto& text_run_info = text_run_info_result.value();
    uint32_t text_run_end = char_index + text_run_info.len;
    DCHECK_LE(text_run_end, static_cast<uint32_t>(char_count));
    text_runs->push_back(text_run_info);

    // We need to provide enough information to draw a bounding box
    // around any arbitrary text range, but the bounding boxes of characters
    // we get from PDFium don't necessarily "line up".
    // Example for LTR text direction: walk through the
    // characters in each text run and let the width of each character be
    // the difference between the x coordinate of one character and the
    // x coordinate of the next. The rest of the bounds of each character
    // can be computed from the bounds of the text run.
    // The same idea is used for RTL, TTB and BTT text direction.
    gfx::RectF char_bounds = engine->GetCharBounds(page_index, char_index);
    for (uint32_t i = char_index; i < text_run_end - 1; i++) {
      DCHECK_LT(i + 1, static_cast<uint32_t>(char_count));
      gfx::RectF next_char_bounds = engine->GetCharBounds(page_index, i + 1);
      double& char_width = (*chars)[i].char_width;
      switch (text_run_info.direction) {
        case PP_PRIVATEDIRECTION_NONE:
        case PP_PRIVATEDIRECTION_LTR:
          char_width = next_char_bounds.x() - char_bounds.x();
          break;
        case PP_PRIVATEDIRECTION_TTB:
          char_width = next_char_bounds.y() - char_bounds.y();
          break;
        case PP_PRIVATEDIRECTION_RTL:
          char_width = char_bounds.right() - next_char_bounds.right();
          break;
        case PP_PRIVATEDIRECTION_BTT:
          char_width = char_bounds.bottom() - next_char_bounds.bottom();
          break;
      }
      char_bounds = next_char_bounds;
    }
    double& char_width = (*chars)[text_run_end - 1].char_width;
    if (text_run_info.direction == PP_PRIVATEDIRECTION_BTT ||
        text_run_info.direction == PP_PRIVATEDIRECTION_TTB) {
      char_width = char_bounds.height();
    } else {
      char_width = char_bounds.width();
    }

    char_index += text_run_info.len;
  }

  page_info->text_run_count = text_runs->size();
  GetAccessibilityLinkInfo(engine, page_index, *text_runs,
                           &page_objects->links);
  GetAccessibilityImageInfo(engine, page_index, page_info->text_run_count,
                            &page_objects->images);
  GetAccessibilityHighlightInfo(engine, page_index, *text_runs,
                                &page_objects->highlights);
  GetAccessibilityFormFieldInfo(engine, page_index, page_info->text_run_count,
                                &page_objects->form_fields);
  return true;
}

}  // namespace chrome_pdf
