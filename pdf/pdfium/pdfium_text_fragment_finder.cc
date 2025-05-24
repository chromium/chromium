// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_text_fragment_finder.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_range.h"

namespace chrome_pdf {

namespace {

// Adds the `prefix_result` to the list of prefixes found in the PDF.
void AddTextFragmentPrefixResult(
    std::vector<PDFiumRange>& text_fragment_prefixes,
    PDFiumRange prefix_result) {
  text_fragment_prefixes.emplace_back(std::move(prefix_result));
}

// Sets the `suffix_result` to be the suffix of the fragment if it comes after
// the `before_suffix_range`.
void AddTextFragmentSuffixResult(
    PDFiumEngine* engine,
    std::optional<PDFiumRange>& text_fragment_suffix,
    const PDFiumRange& before_suffix_range,
    PDFiumRange suffix_result) {
  // TODO(crbug.com/393166468): Modify TextSearch() to return one result rather
  // than all of them.
  // If an appropriate suffix was already found, then do nothing.
  if (text_fragment_suffix) {
    return;
  }

  const int suffix_boundary_start =
      before_suffix_range.char_index() + before_suffix_range.char_count();
  const int suffix_boundary_count =
      suffix_result.char_index() - suffix_boundary_start;
  const auto suffix_boundary =
      PDFiumRange(engine->GetPage(before_suffix_range.page_index()),
                  suffix_boundary_start, suffix_boundary_count);

  for (const auto& c : suffix_boundary.GetText()) {
    if (!base::IsUnicodeWhitespace(c)) {
      return;
    }
  }

  text_fragment_suffix = std::move(suffix_result);
}

// Executes the search of the fragment suffix value. Takes into consideration
// the range that should come before it if it exists. Returns a null optional or
// the range representing the suffix if it exists.
std::optional<PDFiumRange> FindTextFragmentSuffix(
    PDFiumEngine* engine,
    const shared_highlighting::TextFragment& fragment,
    const PDFiumRange& end_range) {
  std::optional<PDFiumRange> text_fragment_suffix = std::nullopt;
  engine->SearchForFragment(
      base::UTF8ToUTF16(fragment.suffix()),
      /*character_to_start_searching_from=*/end_range.char_index() +
          end_range.char_count(),
      /*last_character_index_to_search=*/-1,
      /*page_to_search=*/end_range.page_index(),
      base::BindRepeating(&AddTextFragmentSuffixResult, engine,
                          std::ref(text_fragment_suffix), std::ref(end_range)));
  return text_fragment_suffix;
}

// Adds the `start_result` to the list of text starts found in the PDF. This
// search utilizes the provided prefix and suffix, if present, to locate the
// fragment within the text. If only one is provided, the search proceeds
// accordingly, ignoring the missing component. The suffix is only checked
// when provided if there does not exist a `text_end` value in the fragment.
void AddTextFragmentStartResult(
    PDFiumEngine* engine,
    std::vector<PDFiumRange>& text_fragment_starts,
    std::optional<PDFiumRange>& text_fragment_suffix,
    const shared_highlighting::TextFragment& fragment,
    std::optional<const PDFiumRange> prefix_range,
    PDFiumRange start_result) {
  // If there is a prefix range, only add the result to `text_fragment_starts_`
  // if the result comes immediately after a word boundary.
  if (prefix_range) {
    const int prefix_end =
        prefix_range->char_index() + prefix_range->char_count();
    const int boundary_start = prefix_end;
    const int boundary_count = start_result.char_index() - prefix_end;
    const auto boundary =
        PDFiumRange(engine->GetPage(start_result.page_index()), boundary_start,
                    boundary_count);
    for (const auto& c : boundary.GetText()) {
      if (!base::IsUnicodeWhitespace(c)) {
        return;
      }
    }
  }

  if (fragment.text_end().empty() && !fragment.suffix().empty()) {
    text_fragment_suffix =
        FindTextFragmentSuffix(engine, fragment, start_result);
    if (!text_fragment_suffix) {
      return;
    }
  }

  text_fragment_starts.emplace_back(std::move(start_result));
}

// Sets the `end_result` to be the text end of the fragment. If a suffix is
// provided, it is also checked to come after the text end.
void AddTextFragmentEndResult(PDFiumEngine* engine,
                              std::optional<PDFiumRange>& text_fragment_end,
                              std::optional<PDFiumRange>& text_fragment_suffix,
                              const shared_highlighting::TextFragment& fragment,
                              PDFiumRange end_result) {
  // If an appropriate text fragment end was already found, do nothing.
  if (text_fragment_end) {
    return;
  }

  if (!fragment.suffix().empty()) {
    text_fragment_suffix = FindTextFragmentSuffix(engine, fragment, end_result);
    if (!text_fragment_suffix) {
      return;
    }
  }
  text_fragment_end = std::move(end_result);
}

}  // namespace

PDFiumTextFragmentFinder::PDFiumTextFragmentFinder(PDFiumEngine* engine)
    : engine_(engine) {}
PDFiumTextFragmentFinder::~PDFiumTextFragmentFinder() = default;

std::vector<PDFiumRange> PDFiumTextFragmentFinder::FindTextFragments(
    base::span<const std::string> text_fragments) {
  text_fragment_highlights_.clear();

  for (const std::string& fragment : text_fragments) {
    const auto text_fragment =
        shared_highlighting::TextFragment::FromEscapedString(fragment);
    CHECK(text_fragment.has_value());
    StartTextFragmentSearch(text_fragment.value());
  }

  return std::move(text_fragment_highlights_);
}

void PDFiumTextFragmentFinder::StartTextFragmentSearch(
    const shared_highlighting::TextFragment& fragment) {
  // Clear any state from previous searches.
  last_unsearched_page_ = 0;
  text_fragment_prefixes_.clear();
  text_fragment_starts_.clear();
  text_fragment_end_ = std::nullopt;
  text_fragment_suffix_ = std::nullopt;

  // If StartTextFragmentSearch() gets called before `engine_` has any page
  // information (i.e. before the first call to LoadDocument has happened).
  // Handle this case.
  if (engine_->GetNumberOfPages() == 0) {
    return;
  }

  // If the fragment contains a prefix, start the search there.
  if (!fragment.prefix().empty()) {
    FindTextFragmentPrefix(fragment, /*page_to_start_search_from=*/0);
    return;
  }

  // Otherwise, start the search from the the text fragment start value as it is
  // a required value of the fragment.
  FindTextFragmentStart(fragment);
}

void PDFiumTextFragmentFinder::FindTextFragmentPrefix(
    const shared_highlighting::TextFragment& fragment,
    int page_to_start_search_from) {
  text_fragment_prefixes_.clear();
  const auto prefix_unicode = base::UTF8ToUTF16(fragment.prefix());
  for (int current_page = page_to_start_search_from;
       current_page < engine_->GetNumberOfPages(); current_page++) {
    last_unsearched_page_ = current_page + 1;
    engine_->SearchForFragment(
        prefix_unicode,
        /*character_to_start_searching_from=*/0,
        /*last_character_index_to_search=*/-1, current_page,
        base::BindRepeating(&AddTextFragmentPrefixResult,
                            std::ref(text_fragment_prefixes_)));

    if (!text_fragment_prefixes_.empty()) {
      FindTextFragmentStart(fragment);
      return;
    }
  }
}

void PDFiumTextFragmentFinder::FindTextFragmentStart(
    const shared_highlighting::TextFragment& fragment) {
  text_fragment_starts_.clear();
  const auto start_unicode = base::UTF8ToUTF16(fragment.text_start());
  // If there are no text fragment prefixes then none were expected as part of
  // the text fragment. In this case, searching for the start term itself should
  // be adequate.
  if (text_fragment_prefixes_.empty()) {
    for (int current_page = 0; current_page < engine_->GetNumberOfPages();
         current_page++) {
      engine_->SearchForFragment(
          start_unicode,
          /*character_to_start_searching_from=*/0,
          /*last_character_index_to_search=*/-1, current_page,
          base::BindRepeating(&AddTextFragmentStartResult, engine_,
                              std::ref(text_fragment_starts_),
                              std::ref(text_fragment_suffix_), fragment,
                              std::nullopt));
    }

    // If no text fragments were found, then return early as the text fragment
    // does exist in the text of this PDF.
    if (text_fragment_starts_.empty()) {
      return;
    }

    // If text fragment starts were found, continue the search. If there is no
    // text end value in the fragment, then the FindTextFragmentEnd() function
    // will conclude the search.
    FindTextFragmentEnd(fragment);
    return;
  }

  // If there are text fragment prefixes, then search through them to determine
  // if the `text_start` value comes after as expected.
  for (const auto& prefix_range : text_fragment_prefixes_) {
    engine_->SearchForFragment(
        start_unicode,
        /*character_to_start_searching_from=*/prefix_range.char_index(),
        /*last_character_index_to_search=*/-1, prefix_range.page_index(),
        base::BindRepeating(&AddTextFragmentStartResult, engine_,
                            std::ref(text_fragment_starts_),
                            std::ref(text_fragment_suffix_), fragment,
                            prefix_range));

    // If no text fragments were found, then continue on to the next prefix
    // found.
    if (text_fragment_starts_.empty()) {
      continue;
    }

    // If text fragment starts were found, continue the search. If there is no
    // text end value in the fragment, then the FindTextFragmentEnd() function
    // will conclude the search.
    FindTextFragmentEnd(fragment);
    return;
  }

  // If the `text_start` value could not be found and the fragment contains a
  // prefix, search again for the text fragment prefix in case the fragment is
  // actually on an unsearched page.
  if (text_fragment_starts_.empty() && !fragment.prefix().empty() &&
      last_unsearched_page_ < engine_->GetNumberOfPages()) {
    FindTextFragmentPrefix(fragment, last_unsearched_page_);
  }
}

void PDFiumTextFragmentFinder::FindTextFragmentEnd(
    const shared_highlighting::TextFragment& fragment) {
  if (fragment.text_end().empty()) {
    FinishTextFragmentSearch();
    return;
  }

  text_fragment_end_ = std::nullopt;
  const auto end_unicode = base::UTF8ToUTF16(fragment.text_end());
  for (const auto& start_range : text_fragment_starts_) {
    engine_->SearchForFragment(
        end_unicode,
        /*character_to_start_searching_from=*/start_range.char_index() +
            start_range.char_count(),
        /*last_character_index_to_search=*/-1,
        /*page_to_search=*/start_range.page_index(),
        base::BindRepeating(&AddTextFragmentEndResult, engine_,
                            std::ref(text_fragment_end_),
                            std::ref(text_fragment_suffix_), fragment));

    if (text_fragment_end_) {
      // If a text fragment end was found, then the text fragment start list
      // should be cleared except for the start range that was used in the
      // search.
      text_fragment_starts_ = {start_range};
      FinishTextFragmentSearch();
      return;
    }
  }

  // If no text end was found and the fragment contains a prefix, search again
  // for the text fragment prefix in case the fragment is on an unsearched page.
  if (!text_fragment_end_ && !fragment.prefix().empty() &&
      last_unsearched_page_ < engine_->GetNumberOfPages()) {
    FindTextFragmentPrefix(fragment, last_unsearched_page_);
  }
}

void PDFiumTextFragmentFinder::FinishTextFragmentSearch() {
  if (text_fragment_starts_.empty()) {
    return;
  }

  PDFiumRange highlight = text_fragment_starts_[0];
  if (text_fragment_end_) {
    // The search for `text_fragment_end_` always starts at the character
    // index that would be represented by `highlight.char_index() +
    // highlight.char_count()`. Because of this
    // `text_fragment_end_->char_index()` should always be greater than
    // `highlight.char_index()`.
    CHECK_GT(text_fragment_end_->char_index(), highlight.char_index());
    base::CheckedNumeric<int> new_char_count = text_fragment_end_->char_index();
    new_char_count -= highlight.char_index();
    new_char_count += text_fragment_end_->char_count();
    highlight.SetCharCount(new_char_count.ValueOrDie());
  }

  text_fragment_highlights_.emplace_back(std::move(highlight));
}

}  // namespace chrome_pdf
