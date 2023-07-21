// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_ITERATOR_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/fonts/font_data_for_range_set.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontDescription;
class FontFallbackList;
class SimpleFontData;

class FontFallbackIterator {
  STACK_ALLOCATED();

 public:
  FontFallbackIterator(const FontDescription&,
                       scoped_refptr<FontFallbackList>,
                       FontFallbackPriority);
  FontFallbackIterator(FontFallbackIterator&&) = default;
  FontFallbackIterator(const FontFallbackIterator&) = delete;
  FontFallbackIterator& operator=(const FontFallbackIterator&) = delete;

  bool HasNext() const { return fallback_stage_ != kOutOfLuck; }
  // Returns whether the next call to Next() needs a full hint list, or whether
  // a single character is sufficient. Intended to serve as an optimization in
  // HarfBuzzShaper to avoid spending too much time and resources collecting a
  // full hint character list. Returns true when the next font in line is a
  // segmented font, i.e. one that requires the hint list to work out which
  // unicode range segment should be used.
  bool NeedsHintList() const;

  // Some system fallback APIs (Windows, Android) require a character, or a
  // portion of the string to be passed.  On Mac and Linux, we get a list of
  // fonts without passing in characters.
  scoped_refptr<FontDataForRangeSet> Next(const Vector<UChar32>& hint_list);

 private:
  bool RangeSetContributesForHint(const Vector<UChar32>& hint_list,
                                  const FontDataForRangeSet*);
  bool AlreadyLoadingRangeForHintChar(UChar32 hint_char);
  void WillUseRange(const AtomicString& family, const FontDataForRangeSet&);

  scoped_refptr<FontDataForRangeSet> UniqueOrNext(
      scoped_refptr<FontDataForRangeSet> candidate,
      const Vector<UChar32>& hint_list);

  scoped_refptr<SimpleFontData> FallbackPriorityFont(UChar32 hint);
  scoped_refptr<SimpleFontData> UniqueSystemFontForHintList(
      const Vector<UChar32>& hint_list);

  const FontDescription& font_description_;
  scoped_refptr<FontFallbackList> font_fallback_list_;
  int current_font_data_index_;
  unsigned segmented_face_index_;

  enum FallbackStage {
    kFallbackPriorityFonts,
    kFontGroupFonts,
    kSegmentedFace,
    kPreferencesFonts,
    kSystemFonts,
    kFirstCandidateForNotdefGlyph,
    kOutOfLuck
  };

  FallbackStage fallback_stage_;
  HashSet<UChar32> previously_asked_for_hint_;
  // FontFallbackIterator is meant for single use by HarfBuzzShaper,
  // traversing through the fonts for shaping only once. We must not return
  // duplicate FontDataForRangeSet objects from the Next() iteration function
  // as returning a duplicate value causes a shaping run that won't return any
  // results. The exception is that if all fonts fail, we return the first
  // candidate to be used for rendering the .notdef glyph, and set HasNext() to
  // false.
  HashSet<uint32_t> unique_font_data_for_range_sets_returned_;
  scoped_refptr<FontDataForRangeSet> first_candidate_;
  Vector<scoped_refptr<FontDataForRangeSet>> tracked_loading_range_sets_;
  FontFallbackPriority font_fallback_priority_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_ITERATOR_H_
