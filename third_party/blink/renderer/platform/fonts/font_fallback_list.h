/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_LIST_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/fonts/fallback_list_composite_key.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class FontDescription;

const int kCAllFamiliesScanned = -1;

class PLATFORM_EXPORT FontFallbackList : public RefCounted<FontFallbackList> {
  USING_FAST_MALLOC(FontFallbackList);

 public:
  static scoped_refptr<FontFallbackList> Create() {
    return base::AdoptRef(new FontFallbackList());
  }

  ~FontFallbackList() { ReleaseFontData(); }
  bool IsValid() const;
  void Invalidate(FontSelector*);

  bool LoadingCustomFonts() const;
  bool ShouldSkipDrawing() const;

  FontSelector* GetFontSelector() const { return font_selector_.Get(); }
  // FIXME: It should be possible to combine fontSelectorVersion and generation.
  unsigned FontSelectorVersion() const { return font_selector_version_; }
  uint16_t Generation() const { return generation_; }

  ShapeCache* GetShapeCache(const FontDescription& font_description) const {
    if (!shape_cache_) {
      FallbackListCompositeKey key = CompositeKey(font_description);
      shape_cache_ =
          FontCache::GetFontCache()->GetShapeCache(key)->GetWeakPtr();
    }
    DCHECK(shape_cache_);
    if (GetFontSelector())
      shape_cache_->ClearIfVersionChanged(GetFontSelector()->Version());
    return shape_cache_.get();
  }

  const SimpleFontData* PrimarySimpleFontData(
      const FontDescription& font_description) {
    if (!cached_primary_simple_font_data_) {
      cached_primary_simple_font_data_ =
          DeterminePrimarySimpleFontData(font_description);
      DCHECK(cached_primary_simple_font_data_);
    }
    return cached_primary_simple_font_data_;
  }
  const FontData* FontDataAt(const FontDescription&, unsigned index) const;

  FallbackListCompositeKey CompositeKey(const FontDescription&) const;

 private:
  FontFallbackList();

  scoped_refptr<FontData> GetFontData(const FontDescription&, int& family_index) const;

  const SimpleFontData* DeterminePrimarySimpleFontData(
      const FontDescription&) const;

  void ReleaseFontData();

  mutable Vector<scoped_refptr<FontData>, 1> font_list_;
  mutable const SimpleFontData* cached_primary_simple_font_data_;
  Persistent<FontSelector> font_selector_;
  unsigned font_selector_version_;
  mutable int family_index_;
  uint16_t generation_;
  mutable bool has_loading_fallback_ : 1;
  mutable base::WeakPtr<ShapeCache> shape_cache_;

  DISALLOW_COPY_AND_ASSIGN(FontFallbackList);
};

}  // namespace blink

#endif
