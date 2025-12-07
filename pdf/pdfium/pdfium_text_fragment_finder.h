// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_TEXT_FRAGMENT_FINDER_H_
#define PDF_PDFIUM_PDFIUM_TEXT_FRAGMENT_FINDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "pdf/pdfium/pdfium_range.h"

namespace shared_highlighting {
class TextFragment;
}  // namespace shared_highlighting

namespace chrome_pdf {

class PDFiumEngine;

class PDFiumTextFragmentFinder {
 public:
  explicit PDFiumTextFragmentFinder(PDFiumEngine* engine);
  PDFiumTextFragmentFinder(const PDFiumTextFragmentFinder&) = delete;
  PDFiumTextFragmentFinder& operator=(const PDFiumTextFragmentFinder&) = delete;
  ~PDFiumTextFragmentFinder();

  // Finds the text fragments ranges to highlight. The strings passed to this
  // function must be unescaped, otherwise some specific text fragments will
  // fail to parse correctly. For example, `foo%2C-,bar` which will parse vs.
  // `foo,-,bar` which will fail to parse. When finished searching, the
  // highlights will be returned as a list of ranges.
  std::vector<PDFiumRange> FindTextFragments(
      base::span<const std::string> text_fragments);

 private:
  // Starts a text fragment search with the values provided in `fragment`.
  void StartTextFragmentSearch(
      const shared_highlighting::TextFragment& fragment);

  // Executes the search of the fragment prefix starting from
  // `page_to_start_search_from`. Should not be called if the prefix does not
  // exist in `fragment`.
  void FindTextFragmentPrefix(const shared_highlighting::TextFragment& fragment,
                              int page_to_start_search_from);

  // Executes the search of the fragment text start value. Takes into
  // consideration any prefixes or suffixes that should come before or after.
  void FindTextFragmentStart(const shared_highlighting::TextFragment& fragment);

  // Executes the search of the fragment text end value. Takes into
  // consideration the suffix if it exists.
  void FindTextFragmentEnd(const shared_highlighting::TextFragment& fragment);

  // Finishes a text fragment search by adding the range to highlight to
  // `text_fragment_highlights_`.
  void FinishTextFragmentSearch();

  // The last unsearched page during a text fragment search.
  int last_unsearched_page_ = 0;
  // The list of text fragment prefixes found within the PDF if the prefix
  // exists.
  std::vector<PDFiumRange> text_fragment_prefixes_;
  // The text fragment start values found within the PDF.
  std::vector<PDFiumRange> text_fragment_starts_;
  // The text fragment end value if it exists and is found within the
  // PDF.
  std::optional<PDFiumRange> text_fragment_end_;
  // The suffix of the text fragment if it exists and is found within the PDF.
  std::optional<PDFiumRange> text_fragment_suffix_;
  // Used to highlight text fragments, but does not include text within form
  // text areas.
  std::vector<PDFiumRange> text_fragment_highlights_;

  // Owns this class.
  const raw_ptr<PDFiumEngine> engine_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_TEXT_FRAGMENT_FINDER_H_
