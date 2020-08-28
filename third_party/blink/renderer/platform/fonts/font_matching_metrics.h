// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

struct LocalFontLookupKey {
  unsigned name_hash{0};
  UChar32 fallback_character{-1};
  unsigned font_selection_request_hash{0};
  bool is_deleted_value_{false};

  LocalFontLookupKey() = default;
  LocalFontLookupKey(const AtomicString& name,
                     FontSelectionRequest font_selection_request)
      : name_hash(AtomicStringHash::GetHash(name)),
        font_selection_request_hash(font_selection_request.GetHash()) {}

  LocalFontLookupKey(UChar32 fallback_character,
                     FontSelectionRequest font_selection_request)
      : fallback_character(fallback_character),
        font_selection_request_hash(font_selection_request.GetHash()) {}

  explicit LocalFontLookupKey(FontSelectionRequest font_selection_request)
      : font_selection_request_hash(font_selection_request.GetHash()) {}

  explicit LocalFontLookupKey(WTF::HashTableDeletedValueType)
      : is_deleted_value_(true) {}

  bool IsHashTableDeletedValue() const { return is_deleted_value_; }

  bool operator==(const LocalFontLookupKey& other) const {
    return name_hash == other.name_hash &&
           fallback_character == other.fallback_character &&
           font_selection_request_hash == other.font_selection_request_hash &&
           is_deleted_value_ == other.is_deleted_value_;
  }
};

struct LocalFontLookupKeyHash {
  STATIC_ONLY(LocalFontLookupKeyHash);
  static unsigned GetHash(const LocalFontLookupKey& key) {
    unsigned hash_codes[4] = {key.name_hash, key.fallback_character,
                              key.font_selection_request_hash,
                              key.is_deleted_value_};
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }
  static bool Equal(const LocalFontLookupKey& a, const LocalFontLookupKey& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct LocalFontLookupKeyHashTraits
    : WTF::SimpleClassHashTraits<LocalFontLookupKey> {
  STATIC_ONLY(LocalFontLookupKeyHashTraits);
  static const bool kEmptyValueIsZero = false;
};

enum class LocalFontLookupType {
  kAtFontFaceLocalSrc,
  kGenericFontFamilyName,
  kLocalFontFamilyName,
  kPreferredStandardFont,
  kLastResortInFontFallbackList,
  kFallbackPriorityFont,
  kSystemFallbackFont,
  kLastResortInFontFallbackIterator,
};

struct LocalFontLookupResult {
  int64_t hash;  // 0 if font was not found
  LocalFontLookupType check_type;
  bool is_loading_fallback;
};

struct GenericFontLookupKey {
  unsigned generic_font_family_name_hash;
  UScriptCode script{UScriptCode::USCRIPT_INVALID_CODE};
  FontDescription::GenericFamilyType generic_family_type;
  bool is_deleted_value_{false};

  GenericFontLookupKey() = default;
  GenericFontLookupKey(const AtomicString& generic_font_family_name,
                       UScriptCode script,
                       FontDescription::GenericFamilyType generic_family_type)
      : generic_font_family_name_hash(
            AtomicStringHash::GetHash(generic_font_family_name)),
        script(script),
        generic_family_type(generic_family_type) {}

  explicit GenericFontLookupKey(WTF::HashTableDeletedValueType)
      : is_deleted_value_(true) {}

  bool IsHashTableDeletedValue() const { return is_deleted_value_; }

  bool operator==(const GenericFontLookupKey& other) const {
    return generic_font_family_name_hash ==
               other.generic_font_family_name_hash &&
           script == other.script &&
           generic_family_type == other.generic_family_type &&
           is_deleted_value_ == other.is_deleted_value_;
  }
};

struct GenericFontLookupKeyHash {
  STATIC_ONLY(GenericFontLookupKeyHash);
  static unsigned GetHash(const GenericFontLookupKey& key) {
    unsigned hash_codes[4] = {key.generic_font_family_name_hash, key.script,
                              key.generic_family_type, key.is_deleted_value_};
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }
  static bool Equal(const GenericFontLookupKey& a,
                    const GenericFontLookupKey& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct GenericFontLookupKeyHashTraits
    : WTF::SimpleClassHashTraits<GenericFontLookupKey> {
  STATIC_ONLY(GenericFontLookupKeyHashTraits);
  static const bool kEmptyValueIsZero = false;
};

// Tracks and reports UKM metrics of attempted font family match attempts (both
// successful and not successful) by the current frame.
//
// The number of successful / not successful font family match attempts are
// reported to UKM. The class de-dupes attempts to match the same font family
// name such that they are counted as one attempt.
//
// Each local font lookup is also reported as is each mapping of generic font
// family name to its corresponding actual font family names. Local font lookups
// are deduped according to the family name looked up in the FontCache and the
// FontSelectionRequest parameters (i.e. weight, width and slope). Generic font
// family lookups are de-duped according to the generic name, the
// GenericFamilyType and the script. Both types of lookup events are reported
// regularly.
class PLATFORM_EXPORT FontMatchingMetrics {
 public:
  FontMatchingMetrics(bool top_level,
                      ukm::UkmRecorder* ukm_recorder,
                      ukm::SourceId source_id,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Called when a page attempts to match a font family, and the font family is
  // available.
  void ReportSuccessfulFontFamilyMatch(const AtomicString& font_family_name);

  // Called when a page attempts to match a font family, and the font family is
  // not available.
  void ReportFailedFontFamilyMatch(const AtomicString& font_family_name);

  // Called when a page attempts to match a system font family.
  void ReportSystemFontFamily(const AtomicString& font_family_name);

  // Called when a page attempts to match a web font family.
  void ReportWebFontFamily(const AtomicString& font_family_name);

  // Reports a font listed in a @font-face src:local rule that successfully
  // matched.
  void ReportSuccessfulLocalFontMatch(const AtomicString& font_name);

  // Reports a font listed in a @font-face src:local rule that didn't
  // successfully match.
  void ReportFailedLocalFontMatch(const AtomicString& font_name);

  // Reports a local font was looked up by a name and font description. This
  // includes lookups by a family name, by a PostScript name and by a full font
  // name.
  void ReportFontLookupByUniqueOrFamilyName(
      const AtomicString& name,
      const FontDescription& font_description,
      LocalFontLookupType check_type,
      SimpleFontData* resulting_font_data,
      bool is_loading_fallback = false);

  // Reports a font was looked up by a fallback character and font description.
  void ReportFontLookupByFallbackCharacter(
      UChar32 fallback_character,
      const FontDescription& font_description,
      LocalFontLookupType check_type,
      SimpleFontData* resulting_font_data);

  // Reports a last-resort fallback font was looked up by a font description.
  void ReportLastResortFallbackFontLookup(
      const FontDescription& font_description,
      LocalFontLookupType check_type,
      SimpleFontData* resulting_font_data);

  // Reports a generic font family name was matched according to the script and
  // the user's preferences to a font family name.
  void ReportFontFamilyLookupByGenericFamily(
      const AtomicString& generic_font_family_name,
      UScriptCode script,
      FontDescription::GenericFamilyType generic_family_type,
      const AtomicString& resulting_font_name);

  // Called on page unload and forces metrics to be flushed.
  void PublishAllMetrics();

  // Called whenever a font lookup event that will be saved in |font_tracker| or
  // |user_font_preference_mapping| occurs.
  void OnFontLookup();

  // Publishes the font lookup events. Recorded on page unload and every minute,
  // as long as additional lookups are occurring.
  void PublishIdentifiabilityMetrics();

  // Publishes the number of font family matches attempted (both successful
  // and otherwise) to UKM. Recorded on page unload.
  void PublishUkmMetrics();

 private:
  void IdentifiabilityMetricsTimerFired(TimerBase*);

  // Get a hash that uniquely represents the font data. Returns 0 if |font_data|
  // is nullptr.
  int64_t GetHashForFontData(SimpleFontData* font_data);

  // Font family names successfully matched.
  HashSet<AtomicString> successful_font_families_;

  // Font family names that weren't successfully matched.
  HashSet<AtomicString> failed_font_families_;

  // System font families the page attempted to match.
  HashSet<AtomicString> system_font_families_;

  // Web font families the page attempted to match.
  HashSet<AtomicString> web_font_families_;

  // @font-face src:local fonts that successfully matched.
  HashSet<AtomicString> local_fonts_succeeded_;

  // @font-face src:local fonts that didn't successfully match.
  HashSet<AtomicString> local_fonts_failed_;

  // True if this FontMatchingMetrics instance is for a top-level frame, false
  // otherwise.
  const bool top_level_ = false;

  HashMap<LocalFontLookupKey,
          LocalFontLookupResult,
          LocalFontLookupKeyHash,
          LocalFontLookupKeyHashTraits>
      font_lookups_;
  HashMap<GenericFontLookupKey,
          unsigned,
          GenericFontLookupKeyHash,
          GenericFontLookupKeyHashTraits>
      generic_font_lookups_;

  ukm::UkmRecorder* const ukm_recorder_;
  const ukm::SourceId source_id_;

  TaskRunnerTimer<FontMatchingMetrics> identifiability_metrics_timer_;

  const bool identifiability_study_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_
