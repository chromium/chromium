// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility_helper.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/numerics/safe_math.h"
#include "pdf/accessibility_structs.h"

namespace chrome_pdf {

bool IsCharWithinTextRun(const AccessibilityTextRunInfo& text_run,
                         uint32_t text_run_start_char_index,
                         uint32_t char_index) {
  return char_index >= text_run_start_char_index &&
         char_index - text_run_start_char_index < text_run.len;
}

// If a valid text run range is not found for the char range then return the
// fallback value.
AccessibilityTextRunRangeInfo GetEnclosingTextRunRangeForCharRange(
    const std::vector<AccessibilityTextRunInfo>& text_runs,
    int start_char_index,
    int char_count) {
  // Initialize with fallback value.
  AccessibilityTextRunRangeInfo text_range = {text_runs.size(), 0};
  if (start_char_index < 0 || char_count <= 0)
    return text_range;

  base::CheckedNumeric<uint32_t> checked_end_char_index = char_count - 1;
  checked_end_char_index += start_char_index;
  if (!checked_end_char_index.IsValid())
    return text_range;
  uint32_t end_char_index = checked_end_char_index.ValueOrDie();
  uint32_t current_char_index = 0;
  std::optional<size_t> start_text_run;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    if (!start_text_run.has_value() &&
        IsCharWithinTextRun(text_runs[i], current_char_index,
                            start_char_index)) {
      start_text_run = i;
    }

    if (start_text_run.has_value() &&
        IsCharWithinTextRun(text_runs[i], current_char_index, end_char_index)) {
      text_range.index = start_text_run.value();
      text_range.count = i - text_range.index + 1;
      break;
    }
    current_char_index += text_runs[i].len;
  }
  return text_range;
}

}  // namespace chrome_pdf
