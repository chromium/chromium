/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FACE_SOURCE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/font_display.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

class FontDescription;
class SimpleFontData;

class CORE_EXPORT CSSFontFaceSource
    : public GarbageCollected<CSSFontFaceSource> {
 public:
  virtual ~CSSFontFaceSource();

  // Describes whether this a LocalFontFaceSource can be retrieved locally
  // without blocking. If the local lookup needs to be done asynchronously
  // because it takes longer or requires preparation steps, this return false.
  // Allows for LocalFontFaceSource to operate in two modes: synchronous and
  // asynchronously.
  virtual bool IsLocalNonBlocking() const { return false; }
  virtual bool IsLoading() const { return false; }
  virtual bool IsLoaded() const { return true; }
  virtual bool IsValid() const { return true; }

  scoped_refptr<SimpleFontData> GetFontData(const FontDescription&,
                                            const FontSelectionCapabilities&);

  // TODO(https://crbug.com/947461): IsLocalFontAvailable must not have a
  // FontDescription argument.
  virtual bool IsLocalFontAvailable(const FontDescription&) const {
    return false;
  }
  virtual void BeginLoadIfNeeded() {}
  virtual void SetDisplay(FontDisplay) {}

  virtual bool IsInBlockPeriod() const { return false; }
  virtual bool IsInFailurePeriod() const { return false; }

  // For UMA reporting
  virtual bool HadBlankText() { return false; }
  virtual void PaintRequested() {}

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  CSSFontFaceSource() = default;
  virtual scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) = 0;
  void PruneTable();

 private:
  void PruneOldestIfNeeded();
  using FontDataTable = HashMap<FontCacheKey,
                                scoped_refptr<SimpleFontData>,
                                FontCacheKeyHash,
                                FontCacheKeyTraits>;
  using FontCacheKeyAgeList =
      LinkedHashSet<FontCacheKey, FontCacheKeyHash, FontCacheKeyTraits>;

  FontDataTable font_data_table_;
  FontCacheKeyAgeList font_cache_key_age;
  DISALLOW_COPY_AND_ASSIGN(CSSFontFaceSource);
};

}  // namespace blink

#endif
