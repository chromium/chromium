// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_

#include "base/task/single_thread_task_runner.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

// A helper that defines the hash function and the invalid 'empty value' that
// HashMap should use internally.
struct IdentifiableTokenKeyHashTraits
    : WTF::SimpleClassHashTraits<IdentifiableTokenKey> {
  static unsigned GetHash(const IdentifiableTokenKey& key) {
    return WTF::GetHash(key.token.ToUkmMetricValue()) ^
           WTF::GetHash((key.is_deleted_value << 1) + key.is_empty_value);
  }
  static const bool kEmptyValueIsZero = false;
  static IdentifiableTokenKey EmptyValue() { return IdentifiableTokenKey(); }
};

// Tracks and reports UKM metrics of attempted font family match attempts (both
// successful and not successful) by the current frame.
//
// Each local font lookup is also reported as is each mapping of generic font
// family name to its corresponding actual font family names. Local font lookups
// are deduped according to the family name looked up in the FontCache and the
// FontSelectionRequest parameters (i.e. weight, width and slope). Generic font
// family lookups are de-duped according to the generic name, the
// GenericFamilyType and the script. Both types of lookup events are reported
// regularly.
class FontMatchingMetrics {
 public:
  enum FontLoadContext { kTopLevelFrame = 0, kSubframe, kWorker };

  // Create a FontMatchingMetrics objects for a document or a worker. The
  // corresponding ExecutionContext `execution_context` must outlive this.
  FontMatchingMetrics(ExecutionContext* execution_context,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Called when a page attempts to match a font family, and the font family is
  // available.
  void ReportSuccessfulFontFamilyMatch(const AtomicString& font_family_name);

  // Called when a page attempts to match a font family, and the font family is
  // not available.
  void ReportFailedFontFamilyMatch(const AtomicString& font_family_name);

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
      const SimpleFontData* resulting_font_data);

  // Reports a local font was looked up by a name and font description. This
  // only includes lookups where the name is allowed to match PostScript names
  // and full font names, but not family names.
  void ReportFontLookupByUniqueNameOnly(
      const AtomicString& name,
      const FontDescription& font_description,
      const SimpleFontData* resulting_font_data,
      bool is_loading_fallback = false);

  // Reports a font was looked up by a fallback character, fallback priority,
  // and a font description.
  void ReportFontLookupByFallbackCharacter(
      UChar32 fallback_character,
      FontFallbackPriority fallback_priority,
      const FontDescription& font_description,
      const SimpleFontData* resulting_font_data);

  // Reports a last-resort fallback font was looked up by a font description.
  void ReportLastResortFallbackFontLookup(
      const FontDescription& font_description,
      const SimpleFontData* resulting_font_data);

  // Reports a generic font family name was matched according to the script and
  // the user's preferences to a font family name.
  void ReportFontFamilyLookupByGenericFamily(
      const AtomicString& generic_font_family_name,
      UScriptCode script,
      FontDescription::GenericFamilyType generic_family_type,
      const AtomicString& resulting_font_name);

  // Reports for each shaped emoji segment the number of total clusters and the
  // number of clusters that either contain a .notdef/tofu glyph or that is
  // shaped as multiple glyphs, which means the emoji displays incorrectly.
  void ReportEmojiSegmentGlyphCoverage(unsigned num_clusters,
                                       unsigned num_broken_clusters);

  // Called on page unload and forces metrics to be flushed.
  void PublishAllMetrics();

  // Called whenever a font lookup event that will be saved in |font_tracker| or
  // |user_font_preference_mapping| occurs.
  void OnFontLookup();

  // Publishes the font lookup events. Recorded on document shutdown/worker
  // destruction and every minute, as long as additional lookups are occurring.
  void PublishIdentifiabilityMetrics();

  // Publishes the ratio of correctly shaped to incorrectly shaped emoji
  // segments during the lifetime of this metrics recorder, which usually is
  // coupled to the lifetime of a document or WorkerGlobalContext.
  void PublishEmojiGlyphMetrics();

 private:
  void IdentifiabilityMetricsTimerFired(TimerBase*);

  // This HashMap generically stores details of font lookups, i.e. what was used
  // to search for the font, and what the resulting font was. The key is an
  // IdentifiableTokenKey representing a wrapper around a digest of the lookup
  // parameters. The value is an IdentifiableToken representing either a digest
  // of the returned typeface or 0, if no valid typeface was found.
  using TokenToTokenHashMap = HashMap<IdentifiableTokenKey,
                                      IdentifiableToken,
                                      IdentifiableTokenKeyHashTraits>;

  // Adds a digest of the |font_data|'s typeface to |hash_map| using the key
  // |input_key|, unless that key is already present. If |font_data| is not
  // nullptr, then the typeface digest will also be saved with its PostScript
  // name in |font_load_postscript_name_|.
  void InsertFontHashIntoMap(IdentifiableTokenKey input_key,
                             const SimpleFontData* font_data,
                             TokenToTokenHashMap& hash_map);

  // Reports a local font's existence was looked up by a name, but its actual
  // font data may or may not have been loaded. This only includes lookups where
  // the name is allowed to match PostScript names and full font names, but not
  // family names.
  void ReportLocalFontExistenceByUniqueNameOnly(const AtomicString& font_name,
                                                bool font_exists);

  // Reports a local font's existence was looked up by a name, but its actual
  // font data may or may not have been loaded. This includes lookups where the
  // name is allowed to match full font names or family names.
  void ReportLocalFontExistenceByUniqueOrFamilyName(
      const AtomicString& font_name,
      bool font_exists);

  // Constructs a builder with a hash of the FontSelectionRequest already added.
  IdentifiableTokenBuilder GetTokenBuilderWithFontSelectionRequest(
      const FontDescription& font_description);

  // Get a hash that uniquely represents the font data. Returns 0 if |font_data|
  // is nullptr.
  int64_t GetHashForFontData(const SimpleFontData* font_data);

  void Initialize();

  // Get a token that uniquely represents the typeface's PostScript name. May
  // represent the empty string if no PostScript name was found.
  IdentifiableToken GetPostScriptNameTokenForFontData(
      const SimpleFontData* font_data);

  TokenToTokenHashMap font_lookups_by_unique_or_family_name_;
  TokenToTokenHashMap font_lookups_by_unique_name_only_;
  TokenToTokenHashMap font_lookups_by_fallback_character_;
  TokenToTokenHashMap font_lookups_as_last_resort_;
  TokenToTokenHashMap generic_font_lookups_;
  TokenToTokenHashMap font_load_postscript_name_;
  TokenToTokenHashMap local_font_existence_by_unique_or_family_name_;
  TokenToTokenHashMap local_font_existence_by_unique_name_only_;

  uint64_t total_emoji_clusters_shaped_ = 0;
  uint64_t total_broken_emoji_clusters_ = 0;

  ukm::UkmRecorder* const ukm_recorder_;
  const ukm::SourceId source_id_;

  WeakPersistent<ExecutionContext> execution_context_;

  TaskRunnerTimer<FontMatchingMetrics> identifiability_metrics_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_
