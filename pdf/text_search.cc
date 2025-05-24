// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/text_search.h"

#include <string>
#include <vector>

#include "base/i18n/string_search.h"
#include "pdf/pdfium/pdfium_engine_client.h"

namespace chrome_pdf {

std::vector<PDFiumEngineClient::SearchStringResult> TextSearch(
    const std::u16string& needle,
    const std::u16string& haystack,
    bool case_sensitive) {
  base::i18n::RepeatingStringSearch searcher(
      /*find_this=*/needle, /*in_this=*/haystack, case_sensitive);
  std::vector<PDFiumEngineClient::SearchStringResult> results;
  int match_index;
  int match_length;
  while (searcher.NextMatchResult(match_index, match_length)) {
    results.push_back({.start_index = match_index, .length = match_length});
  }
  return results;
}

}  // namespace chrome_pdf
