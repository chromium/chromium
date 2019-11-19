/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_CACHE_H_

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"

namespace blink {

struct ShapeCacheEntry {
  DISALLOW_NEW();
  ShapeCacheEntry() { shape_result_ = nullptr; }
  scoped_refptr<const ShapeResult> shape_result_;
};

class ShapeCache {
  USING_FAST_MALLOC(ShapeCache);
  // Used to optimize small strings as hash table keys. Avoids malloc'ing an
  // out-of-line StringImpl.
  class SmallStringKey {
    DISALLOW_NEW();

   public:
    static unsigned Capacity() { return kCapacity; }

    SmallStringKey()
        : length_(kEmptyValueLength),
          direction_(static_cast<unsigned>(TextDirection::kLtr)) {}

    SmallStringKey(WTF::HashTableDeletedValueType)
        : length_(kDeletedValueLength),
          direction_(static_cast<unsigned>(TextDirection::kLtr)) {}

    SmallStringKey(base::span<const LChar> characters, TextDirection direction)
        : length_(static_cast<uint16_t>(characters.size())),
          direction_(static_cast<unsigned>(direction)) {
      DCHECK(characters.size() <= kCapacity);
      // Up-convert from LChar to UChar.
      for (uint16_t i = 0; i < characters.size(); ++i) {
        characters_[i] = characters[i];
      }

      hash_ = static_cast<unsigned>(base::FastHash(
          base::as_bytes(base::make_span(characters_, length_))));
    }

    SmallStringKey(base::span<const UChar> characters, TextDirection direction)
        : length_(static_cast<uint16_t>(characters.size())),
          direction_(static_cast<unsigned>(direction)) {
      DCHECK(characters.size() <= kCapacity);
      memcpy(characters_, characters.data(), characters.size_bytes());
      hash_ = static_cast<unsigned>(base::FastHash(
          base::as_bytes(base::make_span(characters_, length_))));
    }

    const UChar* Characters() const { return characters_; }
    uint16_t length() const { return length_; }
    TextDirection Direction() const {
      return static_cast<TextDirection>(direction_);
    }
    unsigned GetHash() const { return hash_; }

    bool IsHashTableDeletedValue() const {
      return length_ == kDeletedValueLength;
    }
    bool IsHashTableEmptyValue() const { return length_ == kEmptyValueLength; }

   private:
    static const unsigned kCapacity = 15;
    static const unsigned kEmptyValueLength = kCapacity + 1;
    static const unsigned kDeletedValueLength = kCapacity + 2;

    unsigned hash_;
    unsigned length_ : 15;
    unsigned direction_ : 1;
    UChar characters_[kCapacity];
  };

 public:
  ShapeCache() {
    // TODO(cavalcantii): Investigate tradeoffs of reserving space
    // in short_string_map.
  }

  ShapeCacheEntry* Add(const TextRun& run, ShapeCacheEntry entry) {
    if (run.length() > SmallStringKey::Capacity())
      return nullptr;

    return AddSlowCase(run, entry);
  }

  void ClearIfVersionChanged(unsigned version) {
    if (version != version_) {
      Clear();
      version_ = version;
    }
  }

  void Clear() {
    single_char_map_.clear();
    short_string_map_.clear();
  }

  unsigned size() const {
    return single_char_map_.size() + short_string_map_.size();
  }

  size_t ByteSize() const {
    size_t self_byte_size = 0;
    for (auto cache_entry : single_char_map_) {
      self_byte_size += cache_entry.value.shape_result_->ByteSize();
    }
    for (auto cache_entry : short_string_map_) {
      self_byte_size += cache_entry.value.shape_result_->ByteSize();
    }
    return self_byte_size;
  }

  base::WeakPtr<ShapeCache> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  ShapeCacheEntry* AddSlowCase(const TextRun& run, ShapeCacheEntry entry) {
    bool is_new_entry;
    ShapeCacheEntry* value;
    if (run.length() == 1) {
      uint32_t key = run[0];
      // All current codepoints in UTF-32 are bewteen 0x0 and 0x10FFFF,
      // as such use bit 31 (zero-based) to indicate direction.
      if (run.Direction() == TextDirection::kRtl)
        key |= (1u << 31);
      SingleCharMap::AddResult add_result = single_char_map_.insert(key, entry);
      is_new_entry = add_result.is_new_entry;
      value = &add_result.stored_value->value;
    } else {
      SmallStringKey small_string_key;
      if (run.Is8Bit()) {
        small_string_key = SmallStringKey(run.Span8(), run.Direction());
      } else {
        small_string_key = SmallStringKey(run.Span16(), run.Direction());
      }

      SmallStringMap::AddResult add_result =
          short_string_map_.insert(small_string_key, entry);
      is_new_entry = add_result.is_new_entry;
      value = &add_result.stored_value->value;
    }

    if ((!is_new_entry) || (size() < kMaxSize)) {
      return value;
    }

    // No need to be fancy: we're just trying to avoid pathological growth.
    single_char_map_.clear();
    short_string_map_.clear();

    return nullptr;
  }

  struct SmallStringKeyHash {
    STATIC_ONLY(SmallStringKeyHash);
    static unsigned GetHash(const SmallStringKey& key) { return key.GetHash(); }
    static bool Equal(const SmallStringKey& a, const SmallStringKey& b) {
      return a == b;
    }
    // Empty and deleted values have lengths that are not equal to any valid
    // length.
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };

  struct SmallStringKeyHashTraits : WTF::SimpleClassHashTraits<SmallStringKey> {
    STATIC_ONLY(SmallStringKeyHashTraits);
    static const bool kHasIsEmptyValueFunction = true;
    static bool IsEmptyValue(const SmallStringKey& key) {
      return key.IsHashTableEmptyValue();
    }
    static const unsigned kMinimumTableSize = 16;
  };

  friend bool operator==(const SmallStringKey&, const SmallStringKey&);

  typedef HashMap<SmallStringKey,
                  ShapeCacheEntry,
                  SmallStringKeyHash,
                  SmallStringKeyHashTraits>
      SmallStringMap;
  typedef HashMap<uint32_t,
                  ShapeCacheEntry,
                  DefaultHash<uint32_t>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>
      SingleCharMap;

  // Hard limit to guard against pathological growth. The expected number of
  // cache entries is a lot lower given the average word count for a web page
  // is well below 1,000 and even full length books rarely have over 10,000
  // unique words [1]. 1: http://www.mine-control.com/zack/guttenberg/
  // Our definition of a word is somewhat different from the norm in that we
  // only segment on space. Thus "foo", "foo-", and "foo)" would count as
  // three separate words. Given that 10,000 seems like a reasonable maximum.
  static const unsigned kMaxSize = 10000;

  SingleCharMap single_char_map_;
  SmallStringMap short_string_map_;
  unsigned version_ = 0;
  base::WeakPtrFactory<ShapeCache> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShapeCache);
};

inline bool operator==(const ShapeCache::SmallStringKey& a,
                       const ShapeCache::SmallStringKey& b) {
  if (a.length() != b.length() || a.Direction() != b.Direction())
    return false;
  return WTF::Equal(a.Characters(), b.Characters(), a.length());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_CACHE_H_
