// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"

#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace {

constexpr double kUkmFontLoadCountBucketSpacing = 1.3;

enum FontLoadContext { kTopLevel = 0, kSubFrame };

template <typename T>
HashSet<T> SetIntersection(const HashSet<T>& a, const HashSet<T>& b) {
  HashSet<T> result;
  for (const T& a_value : a) {
    if (b.Contains(a_value))
      result.insert(a_value);
  }
  return result;
}

}  // namespace

namespace blink {

FontMatchingMetrics::FontMatchingMetrics(
    bool top_level,
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : top_level_(top_level),
      ukm_recorder_(ukm_recorder),
      source_id_(source_id),
      identifiability_metrics_timer_(
          task_runner,
          this,
          &FontMatchingMetrics::IdentifiabilityMetricsTimerFired),
      identifiability_study_enabled_(
          IdentifiabilityStudySettings::Get()->IsActive()) {
  // Estimate of average page font use from anecdotal browsing session.
  constexpr unsigned kEstimatedFontCount = 7;
  local_fonts_succeeded_.ReserveCapacityForSize(kEstimatedFontCount);
  local_fonts_failed_.ReserveCapacityForSize(kEstimatedFontCount);
}

void FontMatchingMetrics::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  successful_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  failed_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportSystemFontFamily(
    const AtomicString& font_family_name) {
  system_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportWebFontFamily(
    const AtomicString& font_family_name) {
  web_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  local_fonts_succeeded_.insert(font_name);
}

void FontMatchingMetrics::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  local_fonts_failed_.insert(font_name);
}

void FontMatchingMetrics::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();
  LocalFontLookupKey key(name, font_description.GetFontSelectionRequest());
  if (font_lookups_.Contains(key))
    return;
  int64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupResult result{hash, check_type, is_loading_fallback};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();
  LocalFontLookupKey key(fallback_character,
                         font_description.GetFontSelectionRequest());
  if (font_lookups_.Contains(key))
    return;
  int64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupResult result{hash, check_type,
                               false /* is_loading_fallback */};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();
  LocalFontLookupKey key(font_description.GetFontSelectionRequest());
  if (font_lookups_.Contains(key))
    return;
  int64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupResult result{hash, check_type,
                               false /* is_loading_fallback */};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportFontFamilyLookupByGenericFamily(
    const AtomicString& generic_font_family_name,
    UScriptCode script,
    FontDescription::GenericFamilyType generic_family_type,
    const AtomicString& resulting_font_name) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();
  GenericFontLookupKey key(generic_font_family_name, script,
                           generic_family_type);
  generic_font_lookups_.insert(key,
                               AtomicStringHash::GetHash(resulting_font_name));
}

void FontMatchingMetrics::PublishIdentifiabilityMetrics() {
  DCHECK(identifiability_study_enabled_);

  IdentifiabilityMetricBuilder builder(source_id_);

  for (const auto& entry : font_lookups_) {
    const LocalFontLookupKey& key = entry.key;
    const LocalFontLookupResult& result = entry.value;

    IdentifiableToken input_token(key.name_hash, key.fallback_character,
                                  key.font_selection_request_hash);
    IdentifiableToken output_token(result.hash, result.check_type,
                                   result.is_loading_fallback);

    builder.Set(IdentifiableSurface::FromTypeAndToken(
                    IdentifiableSurface::Type::kLocalFontLookup, input_token),
                output_token);
  }
  font_lookups_.clear();

  for (const auto& entry : generic_font_lookups_) {
    const GenericFontLookupKey& key = entry.key;
    const unsigned& result = entry.value;

    IdentifiableToken input_token(key.generic_font_family_name_hash, key.script,
                                  key.generic_family_type);
    IdentifiableToken output_token(result);

    builder.Set(IdentifiableSurface::FromTypeAndToken(
                    IdentifiableSurface::Type::kGenericFontLookup, input_token),
                output_token);
  }
  generic_font_lookups_.clear();

  builder.Record(ukm_recorder_);
}

void FontMatchingMetrics::PublishUkmMetrics() {
  ukm::builders::FontMatchAttempts(source_id_)
      .SetLoadContext(top_level_ ? kTopLevel : kSubFrame)
      .SetSystemFontFamilySuccesses(ukm::GetExponentialBucketMin(
          SetIntersection(successful_font_families_, system_font_families_)
              .size(),
          kUkmFontLoadCountBucketSpacing))
      .SetSystemFontFamilyFailures(ukm::GetExponentialBucketMin(
          SetIntersection(failed_font_families_, system_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilySuccesses(ukm::GetExponentialBucketMin(
          SetIntersection(successful_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilyFailures(ukm::GetExponentialBucketMin(
          SetIntersection(failed_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetLocalFontFailures(ukm::GetExponentialBucketMin(
          local_fonts_failed_.size(), kUkmFontLoadCountBucketSpacing))
      .SetLocalFontSuccesses(ukm::GetExponentialBucketMin(
          local_fonts_succeeded_.size(), kUkmFontLoadCountBucketSpacing))
      .Record(ukm_recorder_);
}

void FontMatchingMetrics::OnFontLookup() {
  DCHECK(identifiability_study_enabled_);
  if (!identifiability_metrics_timer_.IsActive()) {
    identifiability_metrics_timer_.StartOneShot(base::TimeDelta::FromMinutes(1),
                                                FROM_HERE);
  }
}

void FontMatchingMetrics::IdentifiabilityMetricsTimerFired(TimerBase*) {
  PublishIdentifiabilityMetrics();
}

void FontMatchingMetrics::PublishAllMetrics() {
  if (identifiability_study_enabled_) {
    PublishIdentifiabilityMetrics();
  }
  PublishUkmMetrics();
}

int64_t FontMatchingMetrics::GetHashForFontData(SimpleFontData* font_data) {
  return font_data ? FontGlobalContext::Get()
                         ->GetOrComputeTypefaceDigest(font_data->PlatformData())
                         .ToUkmMetricValue()
                   : 0;
}

}  // namespace blink
