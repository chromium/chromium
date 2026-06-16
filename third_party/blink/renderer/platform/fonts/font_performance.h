// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PERFORMANCE_H_

#include "base/check.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// This class collects performance data for font-related operations in main
// thread.
class PLATFORM_EXPORT FontPerformance {
 public:
  static void Reset() {
    primary_font_ = base::TimeDelta();
    primary_font_in_style_ = base::TimeDelta();
    system_fallback_ = base::TimeDelta();
    system_fallback_count_ = 0;
    system_fallback_initial_duration_ = base::TimeDelta();
    shape_cache_hit_count_ = 0;
    shape_cache_miss_count_ = 0;
  }

  // The aggregated time spent in |DeterminePrimarySimpleFontData|.
  static base::TimeDelta PrimaryFontTime() {
    return primary_font_ + primary_font_in_style_;
  }
  static const base::TimeDelta& PrimaryFontTimeInStyle() {
    return primary_font_in_style_;
  }
  static void AddPrimaryFontTime(base::TimeDelta time) {
    if (!IsMainThread()) [[unlikely]] {
      return;
    }
    if (in_style_)
      primary_font_in_style_ += time;
    else
      primary_font_ += time;
  }

  // The aggregated time spent in |FallbackFontForCharacter|.
  static base::TimeDelta SystemFallbackFontTime() { return system_fallback_; }
  static uint32_t SystemFallbackFontCount() { return system_fallback_count_; }
  static base::TimeDelta SystemFallbackFontInitialDuration() {
    return system_fallback_initial_duration_;
  }

  static void AddSystemFallbackFontTime(base::TimeDelta time) {
    if (!IsMainThread()) [[unlikely]] {
      return;
    }
    system_fallback_ += time;
    system_fallback_count_++;
    if (system_fallback_count_ == 1) {
      system_fallback_initial_duration_ = time;
    }
  }

  static void AddShapeCacheHit() {
    if (!IsMainThread()) [[unlikely]] {
      return;
    }
    shape_cache_hit_count_++;
  }
  static void AddShapeCacheMiss() {
    if (!IsMainThread()) [[unlikely]] {
      return;
    }
    shape_cache_miss_count_++;
  }
  static uint32_t ShapeCacheHitCount() { return shape_cache_hit_count_; }
  static uint32_t ShapeCacheMissCount() { return shape_cache_miss_count_; }

  static void MarkFirstContentfulPaint();
  static void MarkDomContentLoaded();

  class StyleScope {
   public:
    StyleScope() { ++in_style_; }
    ~StyleScope() {
      DCHECK(in_style_);
      --in_style_;
    }
  };

 private:
  static base::TimeDelta primary_font_;
  static base::TimeDelta primary_font_in_style_;
  static base::TimeDelta system_fallback_;
  static uint32_t system_fallback_count_;
  static base::TimeDelta system_fallback_initial_duration_;
  static uint32_t shape_cache_hit_count_;
  static uint32_t shape_cache_miss_count_;
  static unsigned in_style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PERFORMANCE_H_
