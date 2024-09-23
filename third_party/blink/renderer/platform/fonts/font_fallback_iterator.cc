// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

FontFallbackIterator::FontFallbackIterator(
    const FontDescription& description,
    FontFallbackList* fallback_list,
    FontFallbackPriority font_fallback_priority)
    : font_description_(description),
      font_fallback_list_(fallback_list),
      current_font_data_index_(0),
      segmented_face_index_(0),
      fallback_stage_(kFontGroupFonts),
      font_fallback_priority_(font_fallback_priority) {
  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled()) {
    HarfBuzzFace::SetIsSystemFallbackStage(false);
  }
}

void FontFallbackIterator::Reset() {
  DCHECK(RuntimeEnabledFeatures::FontVariationSequencesEnabled());
  current_font_data_index_ = 0;
  segmented_face_index_ = 0;
  fallback_stage_ = kFontGroupFonts;
  previously_asked_for_hint_.clear();
  unique_font_data_for_range_sets_returned_.clear();
  first_candidate_ = nullptr;
  tracked_loading_range_sets_.clear();
  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled()) {
    HarfBuzzFace::SetIsSystemFallbackStage(false);
  }
}

bool FontFallbackIterator::AlreadyLoadingRangeForHintChar(UChar32 hint_char) {
  for (const auto& range : tracked_loading_range_sets_) {
    if (range->Contains(hint_char)) {
      return true;
    }
  }
  return false;
}

bool FontFallbackIterator::RangeSetContributesForHint(
    const HintCharList& hint_list,
    const FontDataForRangeSet* segmented_face) {
  for (const auto& hint : hint_list) {
    if (segmented_face->Contains(hint)) {
      // If it's a pending custom font, we need to make sure it can render any
      // new characters, otherwise we may trigger a redundant load. In other
      // cases (already loaded or not a custom font), we can use it right away.
      // Loading data url fonts doesn't incur extra network cost, so we always
      // load them.
      if (!segmented_face->IsPendingCustomFont() ||
          segmented_face->IsPendingDataUrlCustomFont() ||
          !AlreadyLoadingRangeForHintChar(hint)) {
        return true;
      }
    }
  }
  return false;
}

void FontFallbackIterator::WillUseRange(const AtomicString& family,
                                        const FontDataForRangeSet& range_set) {
  FontSelector* selector = font_fallback_list_->GetFontSelector();
  if (!selector)
    return;

  selector->WillUseRange(font_description_, family, range_set);
}

FontDataForRangeSet* FontFallbackIterator::UniqueOrNext(
    FontDataForRangeSet* candidate,
    const HintCharList& hint_list) {
  if (!candidate->HasFontData())
    return Next(hint_list);

  SkTypeface* candidate_typeface =
      candidate->FontData()->PlatformData().Typeface();
  if (!candidate_typeface)
    return Next(hint_list);

  uint32_t candidate_id = candidate_typeface->uniqueID();
  if (unique_font_data_for_range_sets_returned_.Contains(candidate_id)) {
    return Next(hint_list);
  }

  // We don't want to skip subsetted ranges because HarfBuzzShaper's behavior
  // depends on the subsetting.
  if (candidate->IsEntireRange())
    unique_font_data_for_range_sets_returned_.insert(candidate_id);

  // Save first candidate to be returned if all other fonts fail, and we need
  // it to render the .notdef glyph.
  if (!first_candidate_)
    first_candidate_ = candidate;
  return candidate;
}

bool FontFallbackIterator::NeedsHintList() const {
  if (fallback_stage_ == kSegmentedFace)
    return true;

  if (fallback_stage_ != kFontGroupFonts)
    return false;

  const FontData* font_data = font_fallback_list_->FontDataAt(
      font_description_, current_font_data_index_);

  if (!font_data)
    return false;

  return font_data->IsSegmented();
}

FontDataForRangeSet* FontFallbackIterator::Next(const HintCharList& hint_list) {
  if (fallback_stage_ == kOutOfLuck)
    return MakeGarbageCollected<FontDataForRangeSet>();

  if (fallback_stage_ == kFallbackPriorityFonts) {
    // Only try one fallback priority font,
    // then proceed to regular system fallback.
    fallback_stage_ = kSystemFonts;
    FontDataForRangeSet* fallback_priority_font_range =
        MakeGarbageCollected<FontDataForRangeSet>(
            FallbackPriorityFont(hint_list[0]));
    if (fallback_priority_font_range->HasFontData())
      return UniqueOrNext(std::move(fallback_priority_font_range), hint_list);
    return Next(hint_list);
  }

  if (fallback_stage_ == kSystemFonts) {
    // We've reached pref + system fallback.
    const SimpleFontData* system_font = UniqueSystemFontForHintList(hint_list);
    if (system_font) {
      // Fallback fonts are not retained in the FontDataCache.
      return UniqueOrNext(
          MakeGarbageCollected<FontDataForRangeSet>(system_font), hint_list);
    }

    // If we don't have options from the system fallback anymore or had
    // previously returned them, we only have the last resort font left.
    // TODO: crbug.com/42217 Improve this by doing the last run with a last
    // resort font that has glyphs for everything, for example the Unicode
    // LastResort font, not just Times or Arial.
    FontCache& font_cache = FontCache::Get();
    fallback_stage_ = kFirstCandidateForNotdefGlyph;
    const SimpleFontData* last_resort =
        font_cache.GetLastResortFallbackFont(font_description_);

    if (FontSelector* font_selector = font_fallback_list_->GetFontSelector()) {
      font_selector->ReportLastResortFallbackFontLookup(font_description_,
                                                        last_resort);
    }

    return UniqueOrNext(MakeGarbageCollected<FontDataForRangeSet>(last_resort),
                        hint_list);
  }

  if (fallback_stage_ == kFirstCandidateForNotdefGlyph) {
    fallback_stage_ = kOutOfLuck;
    if (!first_candidate_)
      FontCache::CrashWithFontInfo(&font_description_);
    return first_candidate_;
  }

  DCHECK(fallback_stage_ == kFontGroupFonts ||
         fallback_stage_ == kSegmentedFace);
  const FontData* font_data = font_fallback_list_->FontDataAt(
      font_description_, current_font_data_index_);

  if (!font_data) {
    // If there is no fontData coming from the fallback list, it means
    // we are now looking at system fonts, either for prioritized symbol
    // or emoji fonts or by calling system fallback API.
    fallback_stage_ = IsNonTextFallbackPriority(font_fallback_priority_)
                          ? kFallbackPriorityFonts
                          : kSystemFonts;
    if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled()) {
      HarfBuzzFace::SetIsSystemFallbackStage(true);
    }
    return Next(hint_list);
  }

  // Otherwise we've received a fontData from the font-family: set of fonts,
  // and a non-segmented one in this case.
  if (!font_data->IsSegmented()) {
    // Skip forward to the next font family for the next call to next().
    current_font_data_index_++;
    if (!font_data->IsLoading()) {
      SimpleFontData* non_segmented =
          const_cast<SimpleFontData*>(To<SimpleFontData>(font_data));
      // The fontData object that we have here is tracked in m_fontList of
      // FontFallbackList and gets released in the font cache when the
      // FontFallbackList is destroyed.
      return UniqueOrNext(
          MakeGarbageCollected<FontDataForRangeSet>(non_segmented), hint_list);
    }
    return Next(hint_list);
  }

  // Iterate over ranges of a segmented font below.

  const auto* segmented = To<SegmentedFontData>(font_data);
  if (fallback_stage_ != kSegmentedFace) {
    segmented_face_index_ = 0;
    fallback_stage_ = kSegmentedFace;
  }

  DCHECK_LT(segmented_face_index_, segmented->NumFaces());
  FontDataForRangeSet* current_segmented_face =
      segmented->FaceAt(segmented_face_index_);
  segmented_face_index_++;

  if (segmented_face_index_ == segmented->NumFaces()) {
    // Switch from iterating over a segmented face to the next family from
    // the font-family: group of fonts.
    fallback_stage_ = kFontGroupFonts;
    current_font_data_index_++;
  }

  if (RangeSetContributesForHint(hint_list, current_segmented_face)) {
    const SimpleFontData* current_segmented_face_font_data =
        current_segmented_face->FontData();
    if (const CustomFontData* current_segmented_face_custom_font_data =
            current_segmented_face_font_data->GetCustomFontData())
      current_segmented_face_custom_font_data->BeginLoadIfNeeded();
    if (!current_segmented_face_font_data->IsLoading())
      return UniqueOrNext(current_segmented_face, hint_list);
    tracked_loading_range_sets_.push_back(current_segmented_face);
  }

  return Next(hint_list);
}

const SimpleFontData* FontFallbackIterator::FallbackPriorityFont(UChar32 hint) {
  const SimpleFontData* font_data = FontCache::Get().FallbackFontForCharacter(
      font_description_, hint,
      font_fallback_list_->PrimarySimpleFontData(font_description_),
      font_fallback_priority_);

  if (FontSelector* font_selector = font_fallback_list_->GetFontSelector()) {
    font_selector->ReportFontLookupByFallbackCharacter(
        hint, font_fallback_priority_, font_description_, font_data);
  }
  return font_data;
}

static inline unsigned ChooseHintIndex(
    const FontFallbackIterator::HintCharList& hint_list) {
  // crbug.com/618178 has a test case where no Myanmar font is ever found,
  // because the run starts with a punctuation character with a script value of
  // common. Our current font fallback code does not find a very meaningful
  // result for this.
  // TODO crbug.com/668706 - Improve this situation.
  // So if we have multiple hint characters (which indicates that a
  // multi-character grapheme or more failed to shape, then we can try to be
  // smarter and select the first character that has an actual script value.
  DCHECK(hint_list.size());
  if (hint_list.size() <= 1)
    return 0;

  for (wtf_size_t i = 1; i < hint_list.size(); ++i) {
    if (Character::HasDefiniteScript(hint_list[i]))
      return i;
  }
  return 0;
}

const SimpleFontData* FontFallbackIterator::UniqueSystemFontForHintList(
    const HintCharList& hint_list) {
  // When we're asked for a fallback for the same characters again, we give up
  // because the shaper must have previously tried shaping with the font
  // already.
  if (!hint_list.size())
    return nullptr;

  FontCache& font_cache = FontCache::Get();
  UChar32 hint = hint_list[ChooseHintIndex(hint_list)];

  if (!hint || previously_asked_for_hint_.Contains(hint))
    return nullptr;
  previously_asked_for_hint_.insert(hint);

  const SimpleFontData* font_data = font_cache.FallbackFontForCharacter(
      font_description_, hint,
      font_fallback_list_->PrimarySimpleFontData(font_description_));

  if (FontSelector* font_selector = font_fallback_list_->GetFontSelector()) {
    font_selector->ReportFontLookupByFallbackCharacter(
        hint, FontFallbackPriority::kText, font_description_, font_data);
  }
  return font_data;
}

bool FontFallbackIterator::operator==(const FontFallbackIterator& other) const {
  return fallback_stage_ == other.fallback_stage_ &&
         font_fallback_priority_ == other.font_fallback_priority_ &&
         current_font_data_index_ == other.current_font_data_index_ &&
         segmented_face_index_ == other.segmented_face_index_ &&
         font_description_ == other.font_description_ &&
         previously_asked_for_hint_ == other.previously_asked_for_hint_ &&
         unique_font_data_for_range_sets_returned_ ==
             other.unique_font_data_for_range_sets_returned_ &&
         tracked_loading_range_sets_ == other.tracked_loading_range_sets_;
}

}  // namespace blink
