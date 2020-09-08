// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

// A (generic) wrapper around IdentifiableToken to enable its use as a HashMap
// key. The |token| represents the parameters by which a font was looked up.
// However, if |is_deleted_value| or |is_empty_value|, this key represents an
// object for HashMap's internal use only. In that case, |token| is left as a
// default value.
struct IdentifiableTokenKey {
  IdentifiableToken token;
  bool is_deleted_value = false;
  bool is_empty_value = false;

  IdentifiableTokenKey() : is_empty_value(true) {}
  explicit IdentifiableTokenKey(const IdentifiableToken& token)
      : token(token) {}
  explicit IdentifiableTokenKey(WTF::HashTableDeletedValueType)
      : is_deleted_value(true) {}

  bool IsHashTableDeletedValue() const { return is_deleted_value; }

  bool operator==(const IdentifiableTokenKey& other) const {
    return token == other.token && is_deleted_value == other.is_deleted_value &&
           is_empty_value == other.is_empty_value;
  }
  bool operator!=(const IdentifiableTokenKey& other) const {
    return !(*this == other);
  }
};

// A helper that defines the hash and equality functions that HashMap should use
// internally for comparing IdentifiableTokenKeys.
struct IdentifiableTokenKeyHash {
  STATIC_ONLY(IdentifiableTokenKeyHash);
  static unsigned GetHash(const IdentifiableTokenKey& key) {
    IntHash<int64_t> hasher;
    return hasher.GetHash(key.token.ToUkmMetricValue()) ^
           hasher.GetHash((key.is_deleted_value << 1) + key.is_empty_value);
  }
  static bool Equal(const IdentifiableTokenKey& a,
                    const IdentifiableTokenKey& b) {
    return a == b;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

// A helper that defines the invalid 'empty value' that HashMap should use
// internally.
struct IdentifiableTokenKeyHashTraits
    : WTF::SimpleClassHashTraits<IdentifiableTokenKey> {
  STATIC_ONLY(IdentifiableTokenKeyHashTraits);
  static const bool kEmptyValueIsZero = false;
  static IdentifiableTokenKey EmptyValue() { return IdentifiableTokenKey(); }
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
  // only includes lookups where the name is allowed to match family names,
  // PostScript names and full font names.
  void ReportFontLookupByUniqueOrFamilyName(
      const AtomicString& name,
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data);

  // Reports a local font was looked up by a name and font description. This
  // only includes lookups where the name is allowed to match PostScript names
  // and full font names, but not family names.
  void ReportFontLookupByUniqueNameOnly(const AtomicString& name,
                                        const FontDescription& font_description,
                                        SimpleFontData* resulting_font_data,
                                        bool is_loading_fallback = false);

  // Reports a font was looked up by a fallback character, fallback priority,
  // and a font description.
  void ReportFontLookupByFallbackCharacter(
      UChar32 fallback_character,
      FontFallbackPriority fallback_priority,
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data);

  // Reports a last-resort fallback font was looked up by a font description.
  void ReportLastResortFallbackFontLookup(
      const FontDescription& font_description,
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

  // This HashMap generically stores details of font lookups, i.e. what was used
  // to search for the font, and what the resulting font was. The key is an
  // IdentifiableTokenKey representing a wrapper around a digest of the lookup
  // parameters. The value is an IdentifiableToken representing either a digest
  // of the returned typeface or 0, if no valid typeface was found.
  using TokenToTokenHashMap = HashMap<IdentifiableTokenKey,
                                      IdentifiableToken,
                                      IdentifiableTokenKeyHash,
                                      IdentifiableTokenKeyHashTraits>;
  TokenToTokenHashMap font_lookups_by_unique_or_family_name_;
  TokenToTokenHashMap font_lookups_by_unique_name_only_;
  TokenToTokenHashMap font_lookups_by_fallback_character_;
  TokenToTokenHashMap font_lookups_as_last_resort_;
  TokenToTokenHashMap generic_font_lookups_;

  ukm::UkmRecorder* const ukm_recorder_;
  const ukm::SourceId source_id_;

  TaskRunnerTimer<FontMatchingMetrics> identifiability_metrics_timer_;

  const bool identifiability_study_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_MATCHING_METRICS_H_
