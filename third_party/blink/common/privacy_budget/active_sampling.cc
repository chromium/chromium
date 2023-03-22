// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/active_sampling.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

// static
bool IdentifiabilityActiveSampler::IsFontFamilyAvailable(const char* family,
                                                         SkFontMgr* fm) {
  base::ScopedAllowBaseSyncPrimitives allow;
  return !!sk_sp<SkTypeface>(fm->matchFamilyStyle(family, SkFontStyle()));
}

// static
void IdentifiabilityActiveSampler::ReportAvailableFontFamilies(
    std::vector<std::string> fonts_to_check,
    ukm::UkmRecorder* ukm_recorder) {
  sk_sp<SkFontMgr> fontManager(SkFontMgr::RefDefault());
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  blink::IdentifiabilityMetricBuilder builder(ukm_source_id);
  for (const std::string& font : fonts_to_check) {
    bool is_available = IsFontFamilyAvailable(font.c_str(), fontManager.get());

    // Compute a case-insensitive (in a unicode-compatible way) hash for the
    // surface key.
    blink::IdentifiableToken font_family_name_token(
        base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(font))));
    builder.Add(blink::IdentifiableSurface::FromTypeAndToken(
                    blink::IdentifiableSurface::Type::kFontFamilyAvailable,
                    font_family_name_token),
                is_available);
  }
  builder.Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}

// static
void IdentifiabilityActiveSampler::ActivelySampleAvailableFonts(
    ukm::UkmRecorder* recorder) {
  if (!blink::IdentifiabilityStudySettings::Get()->ShouldActivelySample())
    return;

  std::vector<std::string> font_families =
      blink::IdentifiabilityStudySettings::Get()
          ->FontFamiliesToActivelySample();

  if (font_families.empty())
    return;

  ReportAvailableFontFamilies(font_families, recorder);
}

}  // namespace blink
