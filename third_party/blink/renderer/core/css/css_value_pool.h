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

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CORE_EXPORT CSSValuePool final : public GarbageCollected<CSSValuePool> {
 public:
  using PassKey = util::PassKey<CSSValuePool>;

  // TODO(sashab): Make all the value pools store const CSSValues.
  static const int kMaximumCacheableIntegerValue = 255;
  using CSSColorValue = cssvalue::CSSColorValue;
  using CSSUnsetValue = cssvalue::CSSUnsetValue;
  using CSSPendingInterpolationValue = cssvalue::CSSPendingInterpolationValue;
  using ColorValueCache = HeapHashMap<unsigned, Member<CSSColorValue>>;
  static const unsigned kMaximumColorCacheSize = 512;
  using FontFaceValueCache =
      HeapHashMap<AtomicString, Member<const CSSValueList>>;
  static const unsigned kMaximumFontFaceCacheSize = 128;
  using FontFamilyValueCache = HeapHashMap<String, Member<CSSFontFamilyValue>>;

  CSSValuePool();

  // Cached individual values.
  CSSColorValue* TransparentColor() { return color_transparent_; }
  CSSColorValue* WhiteColor() { return color_white_; }
  CSSColorValue* BlackColor() { return color_black_; }
  CSSInheritedValue* InheritedValue() { return inherited_value_; }
  CSSInitialValue* InitialValue() { return initial_value_; }
  CSSUnsetValue* UnsetValue() { return unset_value_; }
  CSSInvalidVariableValue* InvalidVariableValue() {
    return invalid_variable_value_;
  }
  CSSPendingInterpolationValue* PendingInterpolationValue(
      CSSPendingInterpolationValue::Type type) {
    DCHECK_GE(static_cast<size_t>(type), 0u);
    DCHECK_LE(static_cast<size_t>(type), 1u);
    return pending_interpolation_values_[static_cast<size_t>(type)];
  }

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
  ColorValueCache::AddResult GetColorCacheEntry(RGBA32 rgb_value) {
    // Just wipe out the cache and start rebuilding if it gets too big.
    if (color_value_cache_.size() > kMaximumColorCacheSize)
      color_value_cache_.clear();
    return color_value_cache_.insert(rgb_value, nullptr);
  }
  FontFamilyValueCache::AddResult GetFontFamilyCacheEntry(
      const String& family_name) {
    return font_family_value_cache_.insert(family_name, nullptr);
  }
  FontFaceValueCache::AddResult GetFontFaceCacheEntry(
      const AtomicString& string) {
    // Just wipe out the cache and start rebuilding if it gets too big.
    if (font_face_value_cache_.size() > kMaximumFontFaceCacheSize)
      font_face_value_cache_.clear();
    return font_face_value_cache_.insert(string, nullptr);
  }

  void Trace(blink::Visitor*);

 private:
  // Cached individual values.
  Member<CSSInheritedValue> inherited_value_;
  Member<CSSInitialValue> initial_value_;
  Member<CSSUnsetValue> unset_value_;
  Member<CSSInvalidVariableValue> invalid_variable_value_;
  Member<CSSPendingInterpolationValue> pending_interpolation_values_[2];
  Member<CSSColorValue> color_transparent_;
  Member<CSSColorValue> color_white_;
  Member<CSSColorValue> color_black_;

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
  DISALLOW_COPY_AND_ASSIGN(CSSValuePool);
};

CORE_EXPORT CSSValuePool& CssValuePool();

}  // namespace blink

#endif
