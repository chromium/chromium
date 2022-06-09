/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_source.h"
#include "third_party/blink/renderer/platform/fonts/lock_for_parallel_text_shaping.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontDescription;
class RemoteFontFaceSource;
class SimpleFontData;

class CORE_EXPORT CSSFontFace final : public GarbageCollected<CSSFontFace> {
 public:
  CSSFontFace(FontFace* font_face, Vector<UnicodeRange>& ranges);
  CSSFontFace(const CSSFontFace&) = delete;
  CSSFontFace& operator=(const CSSFontFace&) = delete;

  // Front source is the first successfully loaded source.
  const CSSFontFaceSource* FrontSource() const LOCKS_EXCLUDED(sources_lock_) {
    AutoLockForParallelTextShaping guard(sources_lock_);
    return sources_.IsEmpty() ? nullptr : sources_.front();
  }
  CSSFontFaceSource* FrontSource() LOCKS_EXCLUDED(sources_lock_) {
    AutoLockForParallelTextShaping guard(sources_lock_);
    return sources_.IsEmpty() ? nullptr : sources_.front();
  }
  FontFace* GetFontFace() const { return font_face_; }

  scoped_refptr<UnicodeRangeSet> Ranges() { return ranges_; }

  void AddSegmentedFontFace(CSSSegmentedFontFace*);
  void RemoveSegmentedFontFace(CSSSegmentedFontFace*);

  bool IsValid() const { return FrontSource(); }
  size_t ApproximateBlankCharacterCount() const;

  void AddSource(CSSFontFaceSource*) LOCKS_EXCLUDED(sources_lock_);
  void SetDisplay(FontDisplay);

  void DidBeginLoad();
  bool FontLoaded(CSSFontFaceSource*);
  bool FallbackVisibilityChanged(RemoteFontFaceSource*);

  scoped_refptr<SimpleFontData> GetFontData(const FontDescription&)
      LOCKS_EXCLUDED(sources_lock_);

  FontFace::LoadStatusType LoadStatus() const {
    return font_face_->LoadStatus();
  }
  bool MaybeLoadFont(const FontDescription&, const StringView&);
  bool MaybeLoadFont(const FontDescription&, const FontDataForRangeSet&);
  void Load();

  // Recalculate the font loading timeline period for the font face.
  // https://drafts.csswg.org/css-fonts-4/#font-display-timeline
  // Returns true if the display period is changed.
  bool UpdatePeriod();

  bool HadBlankText() {
    if (auto* source = FrontSource())
      return source->HadBlankText();
    return false;
  }

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<CSSFontFaceSource>> GetSources() const
      LOCKS_EXCLUDED(sources_lock_);
  bool IsContextThread() const;
  void LoadInternal(const FontDescription&) LOCKS_EXCLUDED(sources_lock_);
  void SetLoadStatus(FontFace::LoadStatusType);
  void UpdateLoadStatusForActiveSource(CSSFontFaceSource*);
  void UpdateLoadStatusForNoSource();

  scoped_refptr<UnicodeRangeSet> ranges_;
  HashSet<scoped_refptr<CSSSegmentedFontFace>> segmented_font_faces_;
  mutable LockForParallelTextShaping sources_lock_;
  HeapDeque<Member<CSSFontFaceSource>> sources_ GUARDED_BY(sources_lock_);
  const Member<FontFace> font_face_;
#if defined(USE_PARALLEL_TEXT_SHAPING)
  scoped_refptr<base::SequencedTaskRunner> GetCrossThreadTaskRunner() const;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_H_
