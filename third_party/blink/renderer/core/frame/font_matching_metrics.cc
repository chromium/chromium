// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/font_matching_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

FontMatchingMetrics::FontMatchingMetrics(
    ExecutionContext* execution_context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ukm_recorder_(execution_context->UkmRecorder()),
      source_id_(execution_context->UkmSourceID()),
      execution_context_(execution_context),
      identifiability_metrics_timer_(
          task_runner,
          this,
          &FontMatchingMetrics::IdentifiabilityMetricsTimerFired) {}

void FontMatchingMetrics::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  if (font_family_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_family_name,
                                               /*font_exists=*/true);
}

void FontMatchingMetrics::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  if (font_family_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_family_name,
                                               /*font_exists=*/false);
}

void FontMatchingMetrics::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  if (font_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_name, /*font_exists=*/true);
}

void FontMatchingMetrics::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  if (font_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueOrFamilyName(font_name,
                                               /*font_exists=*/false);
}

void FontMatchingMetrics::ReportLocalFontExistenceByUniqueOrFamilyName(
    const AtomicString& font_name,
    bool font_exists) {
  if (font_name.IsNull()) {
    return;
  }
  Dactyloscoper::TraceFontLookup(
      execution_context_, font_name,
      Dactyloscoper::FontLookupType::kUniqueOrFamilyName);
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName)) {
    return;
  }
  IdentifiableTokenKey input_key(
      IdentifiabilityBenignCaseFoldingStringToken(font_name));
  local_font_existence_by_unique_or_family_name_.insert(input_key, font_exists);
  OnFontLookup();
}

void FontMatchingMetrics::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
  total_emoji_clusters_shaped_ += num_clusters;
  total_broken_emoji_clusters_ += num_broken_clusters;
}

void FontMatchingMetrics::PublishIdentifiabilityMetrics() {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName)) {
    return;
  }

  IdentifiabilityMetricBuilder builder(source_id_);

  for (const auto& individual_lookup :
       local_font_existence_by_unique_or_family_name_) {
    builder.Add(
        IdentifiableSurface::FromTypeAndToken(
            IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName,
            individual_lookup.key.token),
        individual_lookup.value);
  }
  local_font_existence_by_unique_or_family_name_.clear();

  builder.Record(ukm_recorder_);
}

void FontMatchingMetrics::PublishEmojiGlyphMetrics() {
  DCHECK_LE(total_broken_emoji_clusters_, total_emoji_clusters_shaped_);
  if (total_emoji_clusters_shaped_) {
    double percentage = static_cast<double>(total_broken_emoji_clusters_) /
                        total_emoji_clusters_shaped_;
    UMA_HISTOGRAM_PERCENTAGE("Blink.Fonts.EmojiClusterBrokenness",
                             static_cast<int>(round(percentage * 100)));
  }
}

void FontMatchingMetrics::OnFontLookup() {
  if (!identifiability_metrics_timer_.IsActive()) {
    identifiability_metrics_timer_.StartOneShot(base::Minutes(1), FROM_HERE);
  }
}

void FontMatchingMetrics::IdentifiabilityMetricsTimerFired(TimerBase*) {
  PublishIdentifiabilityMetrics();
}

void FontMatchingMetrics::PublishAllMetrics() {
  PublishIdentifiabilityMetrics();
  PublishEmojiGlyphMetrics();
}

}  // namespace blink
