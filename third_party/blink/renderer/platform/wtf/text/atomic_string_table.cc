// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include <cstdint>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/convert_to_8bit_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

namespace blink {

namespace {

ALWAYS_INLINE static bool IsOnly8Bit(base::span<const UChar> chars) {
  return std::ranges::all_of(
      chars, [](UChar ch) { return static_cast<uint16_t>(ch) <= 255; });
}

class UCharBuffer {
 public:
  ALWAYS_INLINE static unsigned ComputeHashAndMaskTop8Bits(
      base::span<const UChar> chars,
      AtomicStringUCharEncoding encoding) {
    if (encoding == AtomicStringUCharEncoding::kIs8Bit ||
        (encoding == AtomicStringUCharEncoding::kUnknown &&
         IsOnly8Bit(chars))) {
      // This is a very common case from HTML parsing, so we take
      // the size penalty from inlining.
      return StringHasher::ComputeHashAndMaskTop8BitsInline<
          ConvertTo8BitHashReader>(UNSAFE_TODO(
          {reinterpret_cast<const uint8_t*>(chars.data()), chars.size()}));
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits(
          reinterpret_cast<const char*>(chars.data()), chars.size() * 2);
    }
  }

  ALWAYS_INLINE UCharBuffer(base::span<const UChar> chars,
                            AtomicStringUCharEncoding encoding)
      : characters_(chars),
        hash_(ComputeHashAndMaskTop8Bits(chars, encoding)),
        encoding_(encoding) {}

  base::span<const UChar> characters() const { return characters_; }
  unsigned hash() const { return hash_; }
  AtomicStringUCharEncoding encoding() const { return encoding_; }

  scoped_refptr<StringImpl> CreateStringImpl() const {
    switch (encoding_) {
      case AtomicStringUCharEncoding::kUnknown:
        return StringImpl::Create8BitIfPossible(characters_);
      case AtomicStringUCharEncoding::kIs8Bit:
        return String::Make8BitFrom16BitSource(characters_).ReleaseImpl();
      case AtomicStringUCharEncoding::kIs16Bit:
        return StringImpl::Create(characters_);
    }
  }

 private:
  const base::span<const UChar> characters_;
  const unsigned hash_;
  const AtomicStringUCharEncoding encoding_;
};

struct UCharBufferTranslator {
  static unsigned GetHash(const UCharBuffer& buf) { return buf.hash(); }

  static bool Equal(StringImpl* const& str, const UCharBuffer& buf) {
    return blink::Equal(str, buf.characters());
  }

  static void Store(StringImpl*& location,
                    const UCharBuffer& buf,
                    unsigned hash) {
    location = buf.CreateStringImpl().release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

struct StringViewLookupTranslator {
  static unsigned GetHash(const StringView& buf) {
    StringImpl* shared_impl = buf.SharedImpl();
    if (shared_impl) [[likely]] {
      return shared_impl->GetHash();
    }

    if (buf.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits(
          base::as_chars(buf.Span8()).data(), buf.length());
    } else if (IsOnly8Bit(buf.Span16())) {
      return StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
          base::as_chars(buf.RawByteSpan()).data(), buf.length());
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits(
          base::as_chars(buf.RawByteSpan()).data(), buf.length() * 2);
    }
  }

  static bool Equal(StringImpl* const& str, const StringView& buf) {
    return *str == buf;
  }
};

// Allows lookups of the ASCII-lowercase version of a string without actually
// allocating memory to store it. Instead, the translator computes the results
// of hash and equality computations as if we had done so. Strings reaching
// these methods are expected to not be lowercase.

// NOTE: Interestingly, the SIMD paths here improve on code size, not just
// on performance.
template <typename CharType>
struct ASCIILowerHashReader {
  static constexpr unsigned kCompressionFactor = 1;
  static constexpr unsigned kExpansionFactor = 1;

  ALWAYS_INLINE static uint64_t Lowercase(CharType ch) {
    return ToASCIILower(ch);
  }

  ALWAYS_INLINE static uint64_t Read64(const uint8_t* ptr) {
    const CharType* p = reinterpret_cast<const CharType*>(ptr);
#if defined(__SSE2__) || defined(__ARM_NEON__)
    CharType b __attribute__((vector_size(8)));
    UNSAFE_TODO(memcpy(&b, p, sizeof(b)));
    b |= (b >= 'A' & b <= 'Z') & 0x20;
    uint64_t ret;
    UNSAFE_TODO(memcpy(&ret, &b, sizeof(b)));
    return ret;
#else
    if constexpr (sizeof(CharType) == 2) {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 16) |
             (Lowercase(p[2]) << 32) | (Lowercase(p[3]) << 48);
    } else {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 8) |
             (Lowercase(p[2]) << 16) | (Lowercase(p[3]) << 24) |
             (Lowercase(p[4]) << 32) | (Lowercase(p[5]) << 40) |
             (Lowercase(p[6]) << 48) | (Lowercase(p[7]) << 56);
    }
#endif
  }
  ALWAYS_INLINE static uint64_t Read32(const uint8_t* ptr) {
    const CharType* p = reinterpret_cast<const CharType*>(ptr);
#if defined(__SSE2__) || defined(__ARM_NEON__)
    CharType b __attribute__((vector_size(4)));
    UNSAFE_TODO(memcpy(&b, p, sizeof(b)));
    b |= (b >= 'A' & b <= 'Z') & 0x20;
    uint32_t ret;
    UNSAFE_TODO(memcpy(&ret, &b, sizeof(b)));
    return ret;
#else
    if constexpr (sizeof(CharType) == 2) {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 16);
    } else {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 8) |
             (Lowercase(p[2]) << 16) | (Lowercase(p[3]) << 24);
    }
#endif
  }

  ALWAYS_INLINE static uint64_t ReadSmall(const uint8_t* p, size_t k) {
    if constexpr (sizeof(CharType) == 2) {
      // This is fine, but the reasoning is a bit subtle. If we get here,
      // we have to be a UTF-16 string, and since ReadSmall can only be called
      // with 1, 2 or 3, it means we must be a UTF-16 string with a single
      // code point (i.e., two bytes). Furthermore, we know that this code point
      // must be above 0xFF, or the HashTranslatorLowercaseBuffer constructor
      // would not have called us. Thus, ToASCIILower() on this code point would
      // do nothing, and this, we should just hash it exactly as PlainHashReader
      // would have done.
      DCHECK_EQ(k, 2u);
      k = 2;
      return (uint64_t{p[0]} << 56) | (uint64_t{UNSAFE_TODO(p[k >> 1])} << 32) |
             uint64_t{UNSAFE_TODO(p[k - 1])};
    } else {
      return (Lowercase(p[0]) << 56) |
             (Lowercase(UNSAFE_TODO(p[k >> 1])) << 32) |
             Lowercase(UNSAFE_TODO(p[k - 1]));
    }
  }
};

// Combines ASCIILowerHashReader and ConvertTo8BitHashReader into one.
// This is an obscure case that we only need for completeness,
// so it is fine that it's not all that optimized.
struct ASCIIConvertTo8AndLowerHashReader {
  static constexpr unsigned kCompressionFactor = 2;
  static constexpr unsigned kExpansionFactor = 1;

  static uint64_t Lowercase(uint16_t ch) { return ToASCIILower(ch); }

  static uint64_t Read64(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return Lowercase(p[0]) | (Lowercase(UNSAFE_TODO(p[1])) << 8) |
           (Lowercase(UNSAFE_TODO(p[2])) << 16) |
           (Lowercase(UNSAFE_TODO(p[3])) << 24) |
           (Lowercase(UNSAFE_TODO(p[4])) << 32) |
           (Lowercase(UNSAFE_TODO(p[5])) << 40) |
           (Lowercase(UNSAFE_TODO(p[6])) << 48) |
           (Lowercase(UNSAFE_TODO(p[7])) << 56);
  }
  static uint64_t Read32(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return Lowercase(p[0]) | (Lowercase(UNSAFE_TODO(p[1])) << 8) |
           (Lowercase(UNSAFE_TODO(p[2])) << 16) |
           (Lowercase(UNSAFE_TODO(p[3])) << 24);
  }
  static uint64_t ReadSmall(const uint8_t* ptr, size_t k) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return (Lowercase(p[0]) << 56) | (Lowercase(UNSAFE_TODO(p[k >> 1])) << 32) |
           Lowercase(UNSAFE_TODO(p[k - 1]));
  }
};

class HashTranslatorLowercaseBuffer {
 public:
  explicit HashTranslatorLowercaseBuffer(const StringImpl* impl) : impl_(impl) {
    // We expect already lowercase strings to take another path in
    // Element::WeakLowercaseIfNecessary.
    DCHECK(!impl_->IsLowerASCII());
    if (impl_->Is8Bit()) {
      hash_ =
          StringHasher::ComputeHashAndMaskTop8Bits<ASCIILowerHashReader<LChar>>(
              (const char*)UNSAFE_TODO(impl_->Characters8()), impl_->length());
    } else {
      if (IsOnly8Bit(impl_->Span16())) {
        hash_ = StringHasher::ComputeHashAndMaskTop8Bits<
            ASCIIConvertTo8AndLowerHashReader>(
            (const char*)UNSAFE_TODO(impl_->Characters16()), impl_->length());
      } else {
        hash_ = StringHasher::ComputeHashAndMaskTop8Bits<
            ASCIILowerHashReader<UChar>>(
            (const char*)UNSAFE_TODO(impl_->Characters16()),
            impl_->length() * 2);
      }
    }
  }

  const StringImpl* impl() const { return impl_; }
  unsigned hash() const { return hash_; }

 private:
  const StringImpl* impl_;
  unsigned hash_;
};
struct LowercaseLookupTranslator {
  // Computes the hash that |query| would have if it were first converted to
  // ASCII lowercase.
  static unsigned GetHash(const HashTranslatorLowercaseBuffer& buf) {
    return buf.hash();
  }

  // Returns true if the hashtable |bucket| contains a string which is the ASCII
  // lowercase version of |query|.
  static bool Equal(StringImpl* const& bucket,
                    const HashTranslatorLowercaseBuffer& buf) {
    // This is similar to EqualIgnoringASCIICase, but not the same.
    // In particular, it validates that |bucket| is a lowercase version of
    // |buf.impl()|.
    //
    // Unlike EqualIgnoringASCIICase, it returns false if they are equal
    // ignoring ASCII case but |bucket| contains an uppercase ASCII character.
    //
    // However, similar optimizations are used here as there, so these should
    // have generally similar correctness and performance constraints.
    const StringImpl* query = buf.impl();
    if (bucket->length() != query->length())
      return false;
    if (bucket->RawByteSpan().data() == query->RawByteSpan().data() &&
        bucket->Is8Bit() == query->Is8Bit()) {
      return query->IsLowerASCII();
    }
    return VisitCharacters(*bucket, [&](auto bch) {
      return VisitCharacters(*query, [&](auto qch) {
        wtf_size_t len = query->length();
        for (wtf_size_t i = 0; i < len; ++i) {
          if (bch[i] != ToASCIILower(qch[i]))
            return false;
        }
        return true;
      });
    });
  }
};

}  // namespace

AtomicStringTable& AtomicStringTable::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicStringTable, table, ());
  return table;
}

AtomicStringTable::AtomicStringTable() {
  base::AutoLock auto_lock(lock_);
  for (StringImpl* string : StringImpl::AllStaticStrings().Values()) {
    DCHECK(string->length());
    AddNoLock(string);
  }
}

void AtomicStringTable::ReserveCapacity(unsigned size) {
  base::AutoLock auto_lock(lock_);
  table_.ReserveCapacityForSize(size);
}

template <typename T, typename HashTranslator>
scoped_refptr<StringImpl> AtomicStringTable::AddToStringTable(const T& value) {
  // Lock not only protects access to the table, it also guarantees
  // mutual exclusion with the refcount decrement on removal.
  base::AutoLock auto_lock(lock_);
  HashSet<StringImpl*>::AddResult add_result =
      table_.AddWithTranslator<HashTranslator>(value);

  // If the string is newly-translated, then we need to adopt it.
  // The boolean in the pair tells us if that is so.
  return add_result.is_new_entry
             ? base::AdoptRef(*add_result.stored_value)
             : base::WrapRefCounted(*add_result.stored_value);
}

scoped_refptr<StringImpl> AtomicStringTable::Add(
    base::span<const UChar> chars,
    AtomicStringUCharEncoding encoding) {
  if (!chars.data()) {
    return nullptr;
  }

  if (chars.empty()) {
    return StringImpl::empty_;
  }

  UCharBuffer buffer(chars, encoding);
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

class LCharBuffer {
 public:
  ALWAYS_INLINE explicit LCharBuffer(base::span<const LChar> chars)
      : characters_(chars),
        // This is a common path from V8 strings, so inlining is worth it.
        hash_(StringHasher::ComputeHashAndMaskTop8BitsInline(chars)) {}

  base::span<const LChar> characters() const { return characters_; }
  unsigned hash() const { return hash_; }

 private:
  const base::span<const LChar> characters_;
  const unsigned hash_;
};

struct LCharBufferTranslator {
  static unsigned GetHash(const LCharBuffer& buf) { return buf.hash(); }

  static bool Equal(StringImpl* const& str, const LCharBuffer& buf) {
    return blink::Equal(str, buf.characters());
  }

  static void Store(StringImpl*& location,
                    const LCharBuffer& buf,
                    unsigned hash) {
    auto string = StringImpl::Create(buf.characters());
    location = string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

scoped_refptr<StringImpl> AtomicStringTable::Add(
    const StringView& string_view) {
  if (string_view.IsNull()) {
    return nullptr;
  }

  if (string_view.empty()) {
    return StringImpl::empty_;
  }

  if (string_view.Is8Bit()) {
    LCharBuffer buffer(string_view.Span8());
    return AddToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
  }
  UCharBuffer buffer(string_view.Span16(), AtomicStringUCharEncoding::kUnknown);
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

scoped_refptr<StringImpl> AtomicStringTable::Add(
    base::span<const LChar> chars) {
  if (!chars.data()) {
    return nullptr;
  }

  if (chars.empty()) {
    return StringImpl::empty_;
  }

  LCharBuffer buffer(chars);
  return AddToStringTable<LCharBuffer, LCharBufferTranslator>(buffer);
}

StringImpl* AtomicStringTable::AddNoLock(StringImpl* string) {
  auto result = table_.insert(string);
  StringImpl* entry = *result.stored_value;
  if (result.is_new_entry)
    entry->SetIsAtomic();

  DCHECK(!string->IsStatic() || entry->IsStatic());
  return entry;
}

scoped_refptr<StringImpl> AtomicStringTable::Add(StringImpl* string) {
  if (!string->length())
    return StringImpl::empty_;

  // Lock not only protects access to the table, it also guarantess
  // mutual exclusion with the refcount decrement on removal.
  base::AutoLock auto_lock(lock_);
  return base::WrapRefCounted(AddNoLock(string));
}

scoped_refptr<StringImpl> AtomicStringTable::Add(
    scoped_refptr<StringImpl>&& string) {
  if (!string->length())
    return StringImpl::empty_;

  // Lock not only protects access to the table, it also guarantess
  // mutual exclusion with the refcount decrement on removal.
  base::AutoLock auto_lock(lock_);
  StringImpl* entry = AddNoLock(string.get());
  if (entry == string.get())
    return std::move(string);

  return base::WrapRefCounted(entry);
}

scoped_refptr<StringImpl> AtomicStringTable::AddUTF8(
    base::span<const uint8_t> characters_span) {
  bool seen_non_ascii = false;
  bool seen_non_latin1 = false;

  unsigned utf16_length = blink::unicode::CalculateStringLengthFromUtf8(
      characters_span, seen_non_ascii, seen_non_latin1);
  if (!seen_non_ascii) {
    return Add(characters_span);
  }

  auto utf16_buf = base::HeapArray<UChar>::Uninit(utf16_length);
  if (blink::unicode::ConvertUtf8ToUtf16(characters_span, utf16_buf).status !=
      blink::unicode::kConversionOK) {
    NOTREACHED();
  }

  UCharBuffer buffer(utf16_buf, seen_non_latin1
                                    ? AtomicStringUCharEncoding::kIs16Bit
                                    : AtomicStringUCharEncoding::kIs8Bit);
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFindSlowForTesting(
    const StringView& string) {
  DCHECK(string.length());
  base::AutoLock auto_lock(lock_);
  const auto& it = table_.Find<StringViewLookupTranslator>(string);
  if (it == table_.end())
    return WeakResult();
  return WeakResult(*it);
}

AtomicStringTable::WeakResult AtomicStringTable::WeakFindLowercase(
    const AtomicString& string) {
  DCHECK(!string.empty());
  DCHECK(!string.IsLowerASCII());
  DCHECK(string.length());
  HashTranslatorLowercaseBuffer buffer(string.Impl());
  base::AutoLock auto_lock(lock_);
  const auto& it = table_.Find<LowercaseLookupTranslator>(buffer);
  if (it == table_.end())
    return WeakResult();
  DCHECK(StringView(*it).IsLowerASCII());
  DCHECK(EqualIgnoringASCIICase(*it, string));
  return WeakResult(*it);
}

bool AtomicStringTable::ReleaseAndRemoveIfNeeded(StringImpl* string) {
  DCHECK(string->IsAtomic());
  base::AutoLock auto_lock(lock_);
  // Double check that the refcount is still 1. Because Add() could
  // have added a new reference after the load in StringImpl::Release.
  if (string->ref_count_.fetch_sub(1, std::memory_order_acq_rel) != 1)
    return false;

  auto iterator = table_.find(string);
  CHECK_NE(iterator, table_.end());
  table_.erase(iterator);
  // Indicate that something was removed.
  return true;
}

}  // namespace blink
