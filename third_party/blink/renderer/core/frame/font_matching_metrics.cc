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

namespace {

template <typename T>
HashSet<T> SetIntersection(const HashSet<T>& a, const HashSet<T>& b) {
  HashSet<T> result;
  for (const T& a_value : a) {
    if (b.Contains(a_value)) {
      result.insert(a_value);
    }
  }
  return result;
}

}  // namespace

namespace blink {

namespace {

bool IdentifiabilityStudyShouldSampleFonts() {
  return IdentifiabilityStudySettings::Get()->ShouldSampleAnyType({
      IdentifiableSurface::Type::kLocalFontLookupByUniqueOrFamilyName,
      IdentifiableSurface::Type::kLocalFontLookupByUniqueNameOnly,
      IdentifiableSurface::Type::kLocalFontLookupByFallbackCharacter,
      IdentifiableSurface::Type::kLocalFontLookupAsLastResort,
      IdentifiableSurface::Type::kGenericFontLookup,
      IdentifiableSurface::Type::kLocalFontLoadPostScriptName,
      IdentifiableSurface::Type::kLocalFontExistenceByUniqueNameOnly,
      IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName,
  });
}

}  // namespace

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
  ReportLocalFontExistenceByUniqueNameOnly(font_name, /*font_exists=*/true);
}

void FontMatchingMetrics::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  if (font_name.IsNull()) {
    return;
  }
  ReportLocalFontExistenceByUniqueNameOnly(font_name, /*font_exists=*/false);
}

void FontMatchingMetrics::ReportLocalFontExistenceByUniqueOrFamilyName(
    const AtomicString& font_name,
    bool font_exists) {
  if (font_name.IsNull()) {
    return;
  }
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName)) {
    return;
  }
  IdentifiableTokenKey input_key(
      IdentifiabilityBenignCaseFoldingStringToken(font_name));
  local_font_existence_by_unique_or_family_name_.insert(input_key, font_exists);
}

void FontMatchingMetrics::ReportLocalFontExistenceByUniqueNameOnly(
    const AtomicString& font_name,
    bool font_exists) {
  if (font_name.IsNull()) {
    return;
  }
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontExistenceByUniqueNameOnly)) {
    return;
  }
  IdentifiableTokenKey input_key(
      IdentifiabilityBenignCaseFoldingStringToken(font_name));
  local_font_existence_by_unique_name_only_.insert(input_key, font_exists);
}

void FontMatchingMetrics::InsertFontHashIntoMap(IdentifiableTokenKey input_key,
                                                const SimpleFontData* font_data,
                                                TokenToTokenHashMap& hash_map) {
  DCHECK(IdentifiabilityStudyShouldSampleFonts());
  if (hash_map.Contains(input_key)) {
    return;
  }
  IdentifiableToken output_token(GetHashForFontData(font_data));
  hash_map.insert(input_key, output_token);

  // We only record postscript name metrics if both the the broader lookup's
  // type and kLocalFontLoadPostScriptName are allowed. (If the former is not,
  // InsertFontHashIntoMap would not be called.)
  if (!font_data ||
      !IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontLoadPostScriptName)) {
    return;
  }
  IdentifiableTokenKey postscript_name_key(
      GetPostScriptNameTokenForFontData(font_data));
  font_load_postscript_name_.insert(postscript_name_key, output_token);
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
    const SimpleFontData* resulting_font_data) {
  Dactyloscoper::TraceFontLookup(
      execution_context_, name, font_description,
      Dactyloscoper::FontLookupType::kUniqueOrFamilyName);
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontLookupByUniqueOrFamilyName)) {
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
    const SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  // We ignore lookups that result in loading fallbacks for now as they should
  // only be temporary.
  if (is_loading_fallback) {
    return;
  }

  Dactyloscoper::TraceFontLookup(
      execution_context_, name, font_description,
      Dactyloscoper::FontLookupType::kUniqueNameOnly);

  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontLookupByUniqueNameOnly)) {
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
    const SimpleFontData* resulting_font_data) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontLookupByFallbackCharacter)) {
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
    const SimpleFontData* resulting_font_data) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kLocalFontLookupAsLastResort)) {
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
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kGenericFontLookup)) {
    return;
  }
  OnFontLookup();

  // kStandardFamily/kWebkitBodyFamily lookups override the
  // |generic_font_family_name|. See FontSelector::FamilyNameFromSettings.
  // No need to be case-insensitive as generic names should already be
  // lowercase.
  DCHECK(generic_family_type == FontDescription::kStandardFamily ||
         generic_family_type == FontDescription::kWebkitBodyFamily ||
         generic_font_family_name == generic_font_family_name.LowerASCII());
  IdentifiableToken lookup_name_token = IdentifiabilityBenignStringToken(
      (generic_family_type == FontDescription::kStandardFamily ||
       generic_family_type == FontDescription::kWebkitBodyFamily)
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

void FontMatchingMetrics::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
  total_emoji_clusters_shaped_ += num_clusters;
  total_broken_emoji_clusters_ += num_broken_clusters;
}

void FontMatchingMetrics::PublishIdentifiabilityMetrics() {
  if (!IdentifiabilityStudyShouldSampleFonts()) {
    return;
  }

  IdentifiabilityMetricBuilder builder(source_id_);

  std::pair<TokenToTokenHashMap*, IdentifiableSurface::Type>
      hash_maps_with_corresponding_surface_types[] = {
          {&font_lookups_by_unique_or_family_name_,
           IdentifiableSurface::Type::kLocalFontLookupByUniqueOrFamilyName},
          {&font_lookups_by_unique_name_only_,
           IdentifiableSurface::Type::kLocalFontLookupByUniqueNameOnly},
          {&font_lookups_by_fallback_character_,
           IdentifiableSurface::Type::kLocalFontLookupByFallbackCharacter},
          {&font_lookups_as_last_resort_,
           IdentifiableSurface::Type::kLocalFontLookupAsLastResort},
          {&generic_font_lookups_,
           IdentifiableSurface::Type::kGenericFontLookup},
          {&font_load_postscript_name_,
           IdentifiableSurface::Type::kLocalFontLoadPostScriptName},
          {&local_font_existence_by_unique_or_family_name_,
           IdentifiableSurface::Type::kLocalFontExistenceByUniqueOrFamilyName},
          {&local_font_existence_by_unique_name_only_,
           IdentifiableSurface::Type::kLocalFontExistenceByUniqueNameOnly},
      };

  for (const auto& surface_entry : hash_maps_with_corresponding_surface_types) {
    TokenToTokenHashMap* hash_map = surface_entry.first;
    const IdentifiableSurface::Type surface_type = surface_entry.second;
    if (IdentifiabilityStudySettings::Get()->ShouldSampleType(surface_type)) {
      for (const auto& individual_lookup : *hash_map) {
        builder.Add(IdentifiableSurface::FromTypeAndToken(
                        surface_type, individual_lookup.key.token),
                    individual_lookup.value);
      }
    }
    hash_map->clear();
  }

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
  DCHECK(IdentifiabilityStudyShouldSampleFonts());
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

int64_t FontMatchingMetrics::GetHashForFontData(
    const SimpleFontData* font_data) {
  return font_data ? FontGlobalContext::Get()
                         .GetOrComputeTypefaceDigest(font_data->PlatformData())
                         .ToUkmMetricValue()
                   : 0;
}

IdentifiableToken FontMatchingMetrics::GetPostScriptNameTokenForFontData(
    const SimpleFontData* font_data) {
  DCHECK(font_data);
  return FontGlobalContext::Get().GetOrComputePostScriptNameDigest(
      font_data->PlatformData());
}

}  // namespace blink
