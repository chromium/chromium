/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using ShapeCacheEntry = scoped_refptr<const ShapeResult>;

class NGShapeCache {
  USING_FAST_MALLOC(NGShapeCache);
  // Used to represent hash table keys as "small string with direction".
  class SmallStringKey {
    DISALLOW_NEW();

   public:
    static unsigned Capacity() { return kCapacity; }

    SmallStringKey()
        : text_(g_empty_string),
          direction_(static_cast<unsigned>(TextDirection::kLtr)) {}

    explicit SmallStringKey(WTF::HashTableDeletedValueType)
        : direction_(static_cast<unsigned>(TextDirection::kLtr)) {}

    SmallStringKey(const String& text, TextDirection direction)
        : text_(text), direction_(static_cast<unsigned>(direction)) {
      DCHECK(text_.length() <= kCapacity);
      // In order to get the most optimal algorithm, use base::FastHash instead
      // of the one provided by StringHasher. See http://crbug.com/735674.
      // TODO(crbug.com/1408058, crbug.com/902789): Investigate hash performance
      // improvement for NGShapeCache:
      // - Should we rely on HashTraits<String>::GetHash(text_) and avoid
      //   storing the hash_ on the class? That would still rely on the slower
      //   StringHasher but would avoid that calculation when the hash result is
      //   already stored on the String object.
      // - Should we use base::HashInts to take direction_ into account in the
      //   hash value to avoid some colisions?
      hash_ = static_cast<unsigned>(
          base::FastHash(text_.Is8Bit() ? base::as_bytes(text_.Span8())
                                        : base::as_bytes(text_.Span16())));
    }

    const String& Text() const { return text_; }
    TextDirection Direction() const {
      return static_cast<TextDirection>(direction_);
    }
    unsigned GetHash() const { return hash_; }

    bool IsHashTableDeletedValue() const { return text_.IsNull(); }
    bool IsHashTableEmptyValue() const { return text_.empty(); }

   private:
    static constexpr unsigned kCapacity = 15;

    unsigned hash_;
    String text_;
    unsigned direction_ : 1;
  };

 public:
  static unsigned MaxTextLengthOfEntries() {
    return SmallStringKey::Capacity();
  }

  NGShapeCache() {
    DCHECK(RuntimeEnabledFeatures::LayoutNGShapeCacheEnabled());
  }
  NGShapeCache(const NGShapeCache&) = delete;
  NGShapeCache& operator=(const NGShapeCache&) = delete;

  ShapeCacheEntry* Add(const String& text, TextDirection direction) {
    if (text.length() > SmallStringKey::Capacity()) {
      return nullptr;
    }
    return AddSlowCase(text, direction);
  }

  void ClearIfVersionChanged(unsigned version) {
    if (version != version_) {
      small_string_map_.clear();
      version_ = version;
    }
  }

  size_t ByteSize() const {
    size_t self_byte_size = 0;
    for (auto cache_entry : small_string_map_) {
      self_byte_size += cache_entry.value->ByteSize();
    }
    return self_byte_size;
  }

  base::WeakPtr<NGShapeCache> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  PLATFORM_EXPORT ShapeCacheEntry* AddSlowCase(const String& text,
                                               TextDirection direction);
  struct SmallStringKeyHashTraits : WTF::SimpleClassHashTraits<SmallStringKey> {
    STATIC_ONLY(SmallStringKeyHashTraits);
    static unsigned GetHash(const SmallStringKey& key) { return key.GetHash(); }
    static const bool kEmptyValueIsZero = false;
    static bool IsEmptyValue(const SmallStringKey& key) {
      return key.IsHashTableEmptyValue();
    }
    static const unsigned kMinimumTableSize = 16;
  };

  friend bool operator==(const SmallStringKey&, const SmallStringKey&);

  typedef HashMap<SmallStringKey, ShapeCacheEntry, SmallStringKeyHashTraits>
      SmallStringMap;

  SmallStringMap small_string_map_;
  unsigned version_ = 0;
  base::WeakPtrFactory<NGShapeCache> weak_factory_{this};
};

inline bool operator==(const NGShapeCache::SmallStringKey& a,
                       const NGShapeCache::SmallStringKey& b) {
  return a.Direction() == b.Direction() && a.Text() == b.Text();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_NG_SHAPE_CACHE_H_
