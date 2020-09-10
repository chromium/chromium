// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_store_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_store_get_options.h"

namespace blink {

// This enum describes the MatchType value specified by the user.
enum class MatchTypeOption {
  // Do not change the meaning or ordering of these values because they are
  // being recorded in a UMA metric.
  kUnspecified = 0,
  kEquals = 1,
  kStartsWith = 2,
  kMaxValue = kStartsWith,
};

void RecordMatchType(const CookieStoreGetOptions& options) {
  MatchTypeOption uma_match_type;
  // TODO(crbug.com/1092328): Switch by V8CookieMatchType::Enum.
  if (!options.hasMatchType()) {
    uma_match_type = MatchTypeOption::kUnspecified;
  } else if (options.matchType() == "equals") {
    uma_match_type = MatchTypeOption::kEquals;
  } else if (options.matchType() == "starts-with") {
    uma_match_type = MatchTypeOption::kStartsWith;
  } else {
    NOTREACHED();
    // In case of an invalid value, we assume it's "equals" for the consistency
    // of UMA.
    uma_match_type = MatchTypeOption::kEquals;
  }
  UMA_HISTOGRAM_ENUMERATION("Blink.CookieStore.MatchType", uma_match_type);
}

}  // namespace blink
