// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/text_search.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/i18n/string_search.h"
#include "pdf/pdfium/pdfium_engine_client.h"
#include "third_party/blink/public/platform/unicode_utilities.h"

namespace chrome_pdf {

namespace {

std::u16string Normalize(std::u16string_view input) {
  return base::i18n::IcuBridge::GetInstance().normalizer().Normalize(
      base::i18n::IcuBridge::Normalizer::NormalizationForm::NFC, input);
}

}  // namespace

std::vector<PDFiumEngineClient::SearchStringResult> TextSearch(
    const std::u16string& needle,
    const std::u16string& haystack,
    bool case_sensitive) {
  base::i18n::RepeatingStringSearch searcher(
      /*find_this=*/needle, /*in_this=*/haystack, case_sensitive);
  std::optional<std::u16string> normalized_search_text;
  if (blink::ContainsKanaLetters(needle)) {
    normalized_search_text = Normalize(needle);
  }
  std::vector<PDFiumEngineClient::SearchStringResult> results;
  int match_index;
  int match_length;
  while (searcher.NextMatchResult(match_index, match_length)) {
    if (!match_length) {
      continue;
    }
    if (normalized_search_text.has_value()) {
      std::u16string normalized_match = Normalize(
          std::u16string_view(haystack).substr(match_index, match_length));
      if (!blink::CheckOnlyKanaLettersInStrings(normalized_search_text.value(),
                                                normalized_match)) {
        continue;
      }
    }
    results.push_back({.start_index = match_index, .length = match_length});
  }
  return results;
}

}  // namespace chrome_pdf
