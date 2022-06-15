/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SEGMENTED_FONT_FACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SEGMENTED_FONT_FACE_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/lock_for_parallel_text_shaping.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/lru_cache.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontData;
class FontDescription;
class FontFace;
class SegmentedFontData;

// CSSSegmentedFontFace stores two FontFace list-sets, CSS-connected and
// non-CSS-connected. This class combines the two such that they can be
// conveniently operated as one.
// Note however, |Insert| has to be instructed which sub-list to insert it to.
// Iterating over the combined set, behaves as if all non-CSS-connected
// FontFaces were stored after the CSS-connected ones.
class FontFaceList final {
  DISALLOW_NEW();

  // TODO(yosin): Once Oilpan allowed mixed thread heap, we should make
  // |FontFaceList| as |HeapLinkedHashSet<T>|.
  class FontFaceListPart : public LinkedHashSet<Member<FontFace>> {
   public:
    void Trace(Visitor* visitor) const {
      for (auto& entry : *this)
        visitor->Trace(*entry);
    }
  };

 public:
  bool IsEmpty() const;
  void Insert(FontFace* font_face, bool css_connected);
  bool Erase(FontFace* font_face);

  // Iterate over CSS-connected FontFaces first, and then non-CSS-connected
  // ones.
  // Modifying the collection is not allowed during iteration.
  bool ForEachUntilTrue(
      const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const;
  bool ForEachUntilFalse(
      const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const;
  void ForEach(
      const base::RepeatingCallback<void(Member<FontFace>)>& callback) const;

  // Iterate (in reverse order) over non-CSS-connected FontFaces first, and
  // then CSS-connected ones.
  // Modifying the collection is not allowed during iteration.
  bool ForEachReverseUntilTrue(
      const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const;
  bool ForEachReverseUntilFalse(
      const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const;
  void ForEachReverse(
      const base::RepeatingCallback<void(Member<FontFace>)>& callback) const;

  void Trace(Visitor* visitor) const;

 private:
  FontFaceListPart css_connected_face_;
  FontFaceListPart non_css_connected_face_;
};

// TODO(yosin): Once Oilpan allowed mixed thread heap, we should make
// |CSSSegmentedFontFace| as |GarbageCollected<T>|.
class CORE_EXPORT CSSSegmentedFontFace final
    : public RefCountedWillBeThreadSafeForParallelTextShaping<
          CSSSegmentedFontFace> {
 public:
  static scoped_refptr<CSSSegmentedFontFace> Create(FontSelectionCapabilities);
  ~CSSSegmentedFontFace();

  FontSelectionCapabilities GetFontSelectionCapabilities() const {
    return font_selection_capabilities_;
  }

  // Called when status of a FontFace has changed (e.g. loaded or timed out)
  // so cached FontData must be discarded.
  void FontFaceInvalidated() LOCKS_EXCLUDED(lock_);

  void AddFontFace(FontFace*, bool css_connected) LOCKS_EXCLUDED(lock_);
  void RemoveFontFace(FontFace*) LOCKS_EXCLUDED(lock_);
  bool IsEmpty() const LOCKS_EXCLUDED(lock_) {
    AutoLockForParallelTextShaping guard(lock_);
    return font_faces_.IsEmpty();
  }

  scoped_refptr<FontData> GetFontData(const FontDescription&)
      LOCKS_EXCLUDED(lock_);

  bool CheckFont(const String&) const LOCKS_EXCLUDED(lock_);
  void Match(const String&, HeapVector<Member<FontFace>>*) const
      LOCKS_EXCLUDED(lock_);
  void WillUseFontData(const FontDescription&, const StringView& text)
      LOCKS_EXCLUDED(lock_);
  void WillUseRange(const FontDescription&, const blink::FontDataForRangeSet&)
      LOCKS_EXCLUDED(lock_);
  size_t ApproximateCharacterCount() const {
    return approximate_character_count_;
  }

  void Trace(Visitor*) const;

 private:
  explicit CSSSegmentedFontFace(FontSelectionCapabilities);

  void PruneTable() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsValid() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  FontSelectionCapabilities font_selection_capabilities_;

  mutable LockForParallelTextShaping lock_;
  WTF::LruCache<FontCacheKey, scoped_refptr<SegmentedFontData>> font_data_table_
      GUARDED_BY(lock_);

  // All non-CSS-connected FontFaces are stored after the CSS-connected ones.
  FontFaceList font_faces_ GUARDED_BY(lock_);

  // Approximate number of characters styled with this CSSSegmentedFontFace.
  // LayoutText::StyleDidChange() increments this on the first
  // CSSSegmentedFontFace in the style's font family list, so this is not
  // counted if this font is used as a fallback font. Also, this may be double
  // counted by style recalcs.
  // TODO(ksakamoto): Revisit the necessity of this. crbug.com/613500
  size_t approximate_character_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SEGMENTED_FONT_FACE_H_
