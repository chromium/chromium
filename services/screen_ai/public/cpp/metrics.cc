// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/public/cpp/metrics.h"

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/language/core/common/language_util.h"
#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai {

void RecordMostDetectedLanguageInOcrData(
    const std::string& metric_name,
    const ui::AXTreeUpdate& tree_update_with_ocr_data) {
  if (tree_update_with_ocr_data.nodes.empty()) {
    return;
  }

  // Count each detected language and find out the most detected language in
  // OCR result. Then record the most detected language in UMA.
  std::map<std::string, size_t> detected_language_count_map;
  for (const auto& node : tree_update_with_ocr_data.nodes) {
    // Count languages detected in OCR results. It will be used in UMA.
    if (node.HasStringAttribute(ax::mojom::StringAttribute::kLanguage)) {
      const std::string& detected_language =
          node.GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
      detected_language_count_map[detected_language]++;
    }
  }

  // Get the most detected language and record it UMA.
  std::string most_detected_language;
  size_t most_detected_language_count = 0u;
  for (const auto& elem : detected_language_count_map) {
    if (elem.second > most_detected_language_count) {
      most_detected_language = elem.first;
      most_detected_language_count = elem.second;
    }
  }

  if (most_detected_language_count == 0u) {
    return;
  }

  // Convert to a Chrome language code synonym. Then pass it to
  // `base::HashMetricName()` that maps this code to a `LocaleCodeISO639` enum
  // value expected by this histogram. See tools/metrics/histograms/enums.xml
  // enum LocaleCodeISO639. The enum there doesn't always have locales where
  // the base lang and the locale are the same (e.g. they don't have id-id, but
  // do have id). So if the base lang and the locale are the same, just use the
  // base lang.
  std::string language_to_log = most_detected_language;
  std::vector<std::string> lang_split =
      base::SplitString(base::ToLowerASCII(language_to_log), "-",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lang_split.size() == 2 && lang_split[0] == lang_split[1]) {
    language_to_log = lang_split[0];
  }
  language::ToChromeLanguageSynonym(&language_to_log);
  base::UmaHistogramSparse(metric_name, base::HashMetricName(language_to_log));
}

}  // namespace screen_ai
