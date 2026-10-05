// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_performance.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"

namespace blink {

base::TimeDelta FontPerformance::primary_font_;
base::TimeDelta FontPerformance::primary_font_in_style_;
base::TimeDelta FontPerformance::system_fallback_;
size_t FontPerformance::system_fallback_count_ = 0;
base::TimeDelta FontPerformance::system_fallback_initial_duration_;
uint32_t FontPerformance::shape_cache_hit_count_ = 0;
uint32_t FontPerformance::shape_cache_miss_count_ = 0;
wtf_size_t FontPerformance::in_style_ = 0;

// static
void FontPerformance::MarkFirstContentfulPaint() {
  UMA_HISTOGRAM_TIMES("Renderer.Font.PrimaryFont.FCP",
                      FontPerformance::PrimaryFontTime());
  UMA_HISTOGRAM_TIMES("Renderer.Font.PrimaryFont.FCP.Style",
                      FontPerformance::PrimaryFontTimeInStyle());
  UMA_HISTOGRAM_TIMES("Renderer.Font.SystemFallback.FCP",
                      FontPerformance::SystemFallbackFontTime());
}

// static
void FontPerformance::MarkDomContentLoaded() {
  UMA_HISTOGRAM_TIMES("Renderer.Font.PrimaryFont.DomContentLoaded",
                      FontPerformance::PrimaryFontTime());
  UMA_HISTOGRAM_TIMES("Renderer.Font.PrimaryFont.DomContentLoaded.Style",
                      FontPerformance::PrimaryFontTimeInStyle());
  UMA_HISTOGRAM_TIMES("Renderer.Font.SystemFallback.DomContentLoaded",
                      FontPerformance::SystemFallbackFontTime());
}

// static
void FontPerformance::AddSystemFallbackFontTime(UScriptCode script_code,
                                                bool is_emoji,
                                                base::TimeDelta time) {
  // FontPerformance collects metrics for Page Load Metrics, which is only
  // interested in main-thread performance. Since FontCache is thread-specific
  // (via FontGlobalContext), font fallback can be triggered on worker threads
  // (e.g., via OffscreenCanvas). We safely ignore worker thread calls here
  // to avoid thread-safety issues on these global static metrics.
  if (!IsMainThread()) [[unlikely]] {
    return;
  }
  system_fallback_ += time;
  system_fallback_count_++;
  if (system_fallback_count_ == 1) {
    system_fallback_initial_duration_ = time;
  }

  GetScriptFallbackCountsMap()[{script_code, is_emoji}]++;
}

// static
const std::map<FontPerformance::ScriptKey, size_t>&
FontPerformance::GetScriptFallbackCounts() {
  return GetScriptFallbackCountsMap();
}

// static
std::map<FontPerformance::ScriptKey, size_t>&
FontPerformance::GetScriptFallbackCountsMap() {
  static base::NoDestructor<std::map<ScriptKey, size_t>> map;
  return *map;
}

}  // namespace blink
