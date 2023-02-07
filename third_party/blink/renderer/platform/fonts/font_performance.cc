// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_performance.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

base::TimeDelta FontPerformance::primary_font_;
base::TimeDelta FontPerformance::primary_font_in_style_;
base::TimeDelta FontPerformance::system_fallback_;
unsigned FontPerformance::in_style_ = 0;

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

}  // namespace blink
