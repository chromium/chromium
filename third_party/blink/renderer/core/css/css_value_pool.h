/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_POOL_H_

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CORE_EXPORT CSSValuePool final : public GarbageCollected<CSSValuePool> {
 public:
  using PassKey = base::PassKey<CSSValuePool>;

  // TODO(sashab): Make all the value pools store const CSSValues.
  static const int kMaximumCacheableIntegerValue = 255;
  using CSSColor = cssvalue::CSSColor;
  using CSSUnsetValue = cssvalue::CSSUnsetValue;
  using CSSRevertValue = cssvalue::CSSRevertValue;
  using CSSRevertLayerValue = cssvalue::CSSRevertLayerValue;

  // Special keys for deleted and empty values. Use white and transparent as
  // they're common colors and worth having an early-out for.
  struct ColorHashTraitsForCSSValuePool : WTF::GenericHashTraits<Color> {
    STATIC_ONLY(ColorHashTraitsForCSSValuePool);
    static unsigned GetHash(const Color& key) { return key.GetHash(); }
    static Color EmptyValue() { return Color::kTransparent; }
    static Color DeletedValue() { return Color::kWhite; }
  };
  using ColorValueCache =
      HeapHashMap<Color, Member<CSSColor>, ColorHashTraitsForCSSValuePool>;
  static const unsigned kMaximumColorCacheSize = 512;
  using FontFaceValueCache =
      HeapHashMap<AtomicString, Member<const CSSValueList>>;
  static const unsigned kMaximumFontFaceCacheSize = 128;
  using FontFamilyValueCache = HeapHashMap<String, Member<CSSFontFamilyValue>>;

  CSSValuePool();
  CSSValuePool(const CSSValuePool&) = delete;
  CSSValuePool& operator=(const CSSValuePool&) = delete;

  // Cached individual values.
  CSSColor* TransparentColor() { return color_transparent_; }
  CSSColor* WhiteColor() { return color_white_; }
  CSSColor* BlackColor() { return color_black_; }
  CSSInheritedValue* InheritedValue() { return inherited_value_; }
  CSSInitialValue* InitialValue() { return initial_value_; }
  CSSUnsetValue* UnsetValue() { return unset_value_; }
  CSSRevertValue* RevertValue() { return revert_value_; }
  CSSRevertLayerValue* RevertLayerValue() { return revert_layer_value_; }
  CSSInvalidVariableValue* InvalidVariableValue() {
    return invalid_variable_value_;
  }
  CSSCyclicVariableValue* CyclicVariableValue() {
    return cyclic_variable_value_;
  }
  CSSInitialColorValue* InitialColorValue() { return initial_color_value_; }

  // Vector caches.
  CSSIdentifierValue* IdentifierCacheValue(CSSValueID ident) {
    return identifier_value_cache_[static_cast<int>(ident)];
  }
  CSSIdentifierValue* SetIdentifierCacheValue(CSSValueID ident,
                                              CSSIdentifierValue* css_value) {
    return identifier_value_cache_[static_cast<int>(ident)] = css_value;
  }
  CSSNumericLiteralValue* PixelCacheValue(int int_value) {
    return pixel_value_cache_[int_value];
  }
  CSSNumericLiteralValue* SetPixelCacheValue(
      int int_value,
      CSSNumericLiteralValue* css_value) {
    return pixel_value_cache_[int_value] = css_value;
  }
  CSSNumericLiteralValue* PercentCacheValue(int int_value) {
    return percent_value_cache_[int_value];
  }
  CSSNumericLiteralValue* SetPercentCacheValue(
      int int_value,
      CSSNumericLiteralValue* css_value) {
    return percent_value_cache_[int_value] = css_value;
  }
  CSSNumericLiteralValue* NumberCacheValue(int int_value) {
    return number_value_cache_[int_value];
  }
  CSSNumericLiteralValue* SetNumberCacheValue(
      int int_value,
      CSSNumericLiteralValue* css_value) {
    return number_value_cache_[int_value] = css_value;
  }

  // Hash map caches.
  CSSColor* GetOrCreateColor(const Color& color) {
    // These are the empty and deleted values of the hash table.
    // See ColorHashTraitsForCSSValuePool.
    if (color == Color::kTransparent) {
      return TransparentColor();
    }
    if (color == Color::kWhite) {
      return WhiteColor();
    }

    // Just because it is common.
    if (color == Color::kBlack) {
      return BlackColor();
    }

    // Just wipe out the cache and start rebuilding if it gets too big.
    if (color_value_cache_.size() > kMaximumColorCacheSize) {
      color_value_cache_.clear();
    }

    ColorValueCache::AddResult entry =
        color_value_cache_.insert(color, nullptr);
    if (entry.is_new_entry) {
      entry.stored_value->value = MakeGarbageCollected<CSSColor>(color);
    }
    return entry.stored_value->value;
  }
  FontFamilyValueCache::AddResult GetFontFamilyCacheEntry(
      const String& family_name) {
    return font_family_value_cache_.insert(family_name, nullptr);
  }
  FontFaceValueCache::AddResult GetFontFaceCacheEntry(
      const AtomicString& string) {
    // Just wipe out the cache and start rebuilding if it gets too big.
    if (font_face_value_cache_.size() > kMaximumFontFaceCacheSize) {
      font_face_value_cache_.clear();
    }
    return font_face_value_cache_.insert(string, nullptr);
  }

  void Trace(Visitor*) const;

 private:
  // Cached individual values.
  Member<CSSInheritedValue> inherited_value_;
  Member<CSSInitialValue> initial_value_;
  Member<CSSUnsetValue> unset_value_;
  Member<CSSRevertValue> revert_value_;
  Member<CSSRevertLayerValue> revert_layer_value_;
  Member<CSSInvalidVariableValue> invalid_variable_value_;
  Member<CSSCyclicVariableValue> cyclic_variable_value_;
  Member<CSSInitialColorValue> initial_color_value_;
  Member<CSSColor> color_transparent_;
  Member<CSSColor> color_white_;
  Member<CSSColor> color_black_;

  // Vector caches.
  HeapVector<Member<CSSIdentifierValue>, numCSSValueKeywords>
      identifier_value_cache_;
  HeapVector<Member<CSSNumericLiteralValue>, kMaximumCacheableIntegerValue + 1>
      pixel_value_cache_;
  HeapVector<Member<CSSNumericLiteralValue>, kMaximumCacheableIntegerValue + 1>
      percent_value_cache_;
  HeapVector<Member<CSSNumericLiteralValue>, kMaximumCacheableIntegerValue + 1>
      number_value_cache_;

  // Hash map caches.
  ColorValueCache color_value_cache_;
  FontFaceValueCache font_face_value_cache_;
  FontFamilyValueCache font_family_value_cache_;

  friend CORE_EXPORT CSSValuePool& CssValuePool();
};

CORE_EXPORT CSSValuePool& CssValuePool();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_POOL_H_
