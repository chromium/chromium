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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class FontDescription;

const int kCAllFamiliesScanned = -1;

// FontFallbackList caches FontData from FontSelector and FontCache. If font
// updates occur (e.g., @font-face rule changes, web font is loaded, etc.),
// the cached data becomes stale and hence, invalid.
class PLATFORM_EXPORT FontFallbackList : public RefCounted<FontFallbackList> {
  USING_FAST_MALLOC(FontFallbackList);

 public:
  static scoped_refptr<FontFallbackList> Create(FontSelector* font_selector) {
    return base::AdoptRef(new FontFallbackList(font_selector));
  }

  ~FontFallbackList() { ReleaseFontData(); }

  // Returns whether the cached data is valid. We can use a FontFallbackList
  // only when it's valid.
  bool IsValid() const;

  // Called when font updates (see class comment) have made the cached data
  // invalid. Once marked, a Font object cannot reuse |this|, but have to work
  // on a new instance obtained from FontFallbackMap.
  void MarkInvalid() {
    DCHECK(RuntimeEnabledFeatures::
               CSSReducedFontLoadingLayoutInvalidationsEnabled());
    is_invalid_ = true;
  }

  // Clears all the stale data, and reset the state for replenishment. Note that
  // this is a deprecated function, and will be removed after we launch feature
  // CSSReducedFontLoadingLayoutInvalidations. With the feature, we'll never
  // revalidate a FontFallbackList, but create a new FontFallbackList instead.
  void RevalidateDeprecated();

  bool ShouldSkipDrawing() const;

  FontSelector* GetFontSelector() const { return font_selector_.Get(); }
  // FIXME: It should be possible to combine fontSelectorVersion and generation.
  unsigned FontSelectorVersion() const { return font_selector_version_; }
  uint16_t Generation() const { return generation_; }

  ShapeCache* GetShapeCache(const FontDescription& font_description) {
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
  const FontData* FontDataAt(const FontDescription&, unsigned index);

  bool CanShapeWordByWord(const FontDescription&);

  void SetCanShapeWordByWordForTesting(bool b) {
    can_shape_word_by_word_ = b;
    can_shape_word_by_word_computed_ = true;
  }

  bool HasLoadingFallback() const { return has_loading_fallback_; }
  bool HasCustomFont() const { return has_custom_font_; }
  bool HasAdvanceOverride() const { return has_advance_override_; }

 private:
  explicit FontFallbackList(FontSelector* font_selector);

  scoped_refptr<FontData> GetFontData(const FontDescription&);

  const SimpleFontData* DeterminePrimarySimpleFontData(const FontDescription&);

  FallbackListCompositeKey CompositeKey(const FontDescription&) const;

  void ReleaseFontData();
  bool ComputeCanShapeWordByWord(const FontDescription&);

  Vector<scoped_refptr<FontData>, 1> font_list_;
  const SimpleFontData* cached_primary_simple_font_data_;
  const Persistent<FontSelector> font_selector_;
  unsigned font_selector_version_;
  int family_index_;
  uint16_t generation_;
  bool has_loading_fallback_ : 1;
  bool has_custom_font_ : 1;
  bool has_advance_override_ : 1;
  bool can_shape_word_by_word_ : 1;
  bool can_shape_word_by_word_computed_ : 1;
  bool is_invalid_ : 1;

  base::WeakPtr<ShapeCache> shape_cache_;

  DISALLOW_COPY_AND_ASSIGN(FontFallbackList);
};

}  // namespace blink

#endif
