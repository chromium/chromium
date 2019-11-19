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

#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontData;
class FontDescription;
class FontFace;
class SegmentedFontData;

class CSSSegmentedFontFace final
    : public GarbageCollected<CSSSegmentedFontFace> {
 public:
  CSSSegmentedFontFace(FontSelectionCapabilities);
  ~CSSSegmentedFontFace();

  FontSelectionCapabilities GetFontSelectionCapabilities() const {
    return font_selection_capabilities_;
  }

  // Called when status of a FontFace has changed (e.g. loaded or timed out)
  // so cached FontData must be discarded.
  void FontFaceInvalidated();

  void AddFontFace(FontFace*, bool css_connected);
  void RemoveFontFace(FontFace*);
  bool IsEmpty() const { return font_faces_.IsEmpty(); }

  scoped_refptr<FontData> GetFontData(const FontDescription&);

  bool CheckFont(const String&) const;
  void Match(const String&, HeapVector<Member<FontFace>>&) const;
  void WillUseFontData(const FontDescription&, const String& text);
  void WillUseRange(const FontDescription&, const blink::FontDataForRangeSet&);
  size_t ApproximateCharacterCount() const {
    return approximate_character_count_;
  }

  void Trace(blink::Visitor*);

 private:
  void PruneTable();
  bool IsValid() const;

  using FontFaceList = HeapListHashSet<Member<FontFace>>;

  FontSelectionCapabilities font_selection_capabilities_;
  HashMap<FontCacheKey,
          scoped_refptr<SegmentedFontData>,
          FontCacheKeyHash,
          FontCacheKeyTraits>
      font_data_table_;
  // All non-CSS-connected FontFaces are stored after the CSS-connected ones.
  FontFaceList font_faces_;
  FontFaceList::iterator first_non_css_connected_face_;

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
