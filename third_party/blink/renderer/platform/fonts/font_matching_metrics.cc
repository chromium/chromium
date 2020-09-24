// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"

#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
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

void FontMatchingMetrics::InsertFontHashIntoMap(IdentifiableTokenKey input_key,
                                                SimpleFontData* font_data,
                                                TokenToTokenHashMap hash_map) {
  if (hash_map.Contains(input_key))
    return;
  IdentifiableToken output_token(GetHashForFontData(font_data));
  hash_map.insert(input_key, output_token);
}

IdentifiableTokenBuilder
FontMatchingMetrics::GetTokenBuilderWithFontSelectionRequest(
    const FontDescription& font_description) {
  IdentifiableTokenBuilder builder;
  builder.AddValue(font_description.GetFontSelectionRequest().GetHash());
  return builder;
}

void FontMatchingMetrics::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();

  IdentifiableTokenBuilder builder =
      GetTokenBuilderWithFontSelectionRequest(font_description);

  // Font name lookups are case-insensitive.
  builder.AddToken(IdentifiabilityBenignCaseFoldingStringToken(name));

  IdentifiableTokenKey input_key(builder.GetToken());
  InsertFontHashIntoMap(input_key, resulting_font_data,
                        font_lookups_by_unique_or_family_name_);
}

void FontMatchingMetrics::ReportFontLookupByUniqueNameOnly(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  // We ignore lookups that result in loading fallbacks for now as they should
  // only be temporary.
  if (!identifiability_study_enabled_ || is_loading_fallback) {
    return;
  }
  OnFontLookup();

  IdentifiableTokenBuilder builder =
      GetTokenBuilderWithFontSelectionRequest(font_description);

  // Font name lookups are case-insensitive.
  builder.AddToken(IdentifiabilityBenignCaseFoldingStringToken(name));

  IdentifiableTokenKey input_key(builder.GetToken());
  InsertFontHashIntoMap(input_key, resulting_font_data,
                        font_lookups_by_unique_name_only_);
}

void FontMatchingMetrics::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    FontFallbackPriority fallback_priority,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();

  IdentifiableTokenBuilder builder =
      GetTokenBuilderWithFontSelectionRequest(font_description);
  builder.AddValue(fallback_character)
      .AddToken(IdentifiableToken(fallback_priority));

  IdentifiableTokenKey input_key(builder.GetToken());
  InsertFontHashIntoMap(input_key, resulting_font_data,
                        font_lookups_by_fallback_character_);
}

void FontMatchingMetrics::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  if (!identifiability_study_enabled_) {
    return;
  }
  OnFontLookup();

  IdentifiableTokenBuilder builder =
      GetTokenBuilderWithFontSelectionRequest(font_description);

  IdentifiableTokenKey input_key(builder.GetToken());
  InsertFontHashIntoMap(input_key, resulting_font_data,
                        font_lookups_as_last_resort_);
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

  // kStandardFamily lookups override the |generic_font_family_name|. See
  // FontSelector::FamilyNameFromSettings. No need to be case-insensitive as
  // generic names should already be lowercase.
  DCHECK(generic_family_type == FontDescription::kStandardFamily ||
         generic_font_family_name == generic_font_family_name.LowerASCII());
  IdentifiableToken lookup_name_token = IdentifiabilityBenignStringToken(
      generic_family_type == FontDescription::kStandardFamily
          ? font_family_names::kWebkitStandard
          : generic_font_family_name);

  IdentifiableTokenBuilder builder;
  builder.AddToken(lookup_name_token).AddToken(IdentifiableToken(script));
  IdentifiableTokenKey input_key(builder.GetToken());

  // Font name lookups are case-insensitive.
  generic_font_lookups_.insert(
      input_key,
      IdentifiabilityBenignCaseFoldingStringToken(resulting_font_name));
}

void FontMatchingMetrics::PublishIdentifiabilityMetrics() {
  DCHECK(identifiability_study_enabled_);

  IdentifiabilityMetricBuilder builder(source_id_);

  std::pair<TokenToTokenHashMap*, IdentifiableSurface::Type>
      hash_maps_with_corresponding_surface_types[] = {
          {&font_lookups_by_unique_or_family_name_,
           IdentifiableSurface::Type::kLocalFontLookupByUniqueOrFamilyName},
          {&font_lookups_by_unique_name_only_,
           IdentifiableSurface::Type::kLocalFontLookupByUniqueOrFamilyName},
          {&font_lookups_by_fallback_character_,
           IdentifiableSurface::Type::kLocalFontLookupByFallbackCharacter},
          {&font_lookups_as_last_resort_,
           IdentifiableSurface::Type::kLocalFontLookupAsLastResort},
          {&generic_font_lookups_,
           IdentifiableSurface::Type::kGenericFontLookup},
      };

  for (const auto& surface_entry : hash_maps_with_corresponding_surface_types) {
    TokenToTokenHashMap* hash_map = surface_entry.first;
    const IdentifiableSurface::Type& surface_type = surface_entry.second;
    for (const auto& individual_lookup : *hash_map) {
      builder.Set(IdentifiableSurface::FromTypeAndToken(
                      surface_type, individual_lookup.key.token),
                  individual_lookup.value);
    }
    hash_map->clear();
  }

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
