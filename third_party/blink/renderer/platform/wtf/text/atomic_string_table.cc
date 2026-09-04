// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "base/bit_cast.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_lower_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/convert_to_8bit_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

constexpr auto kGoldenRatio64 = 0x9e3779b97f4a7c15ull;

// A thread-local, direct-mapped cache for small 8-bit AtomicStrings (<= 16
// bytes) that avoids the overhead of table locking, hash computation, and
// pointer dereferences of the main AtomicStringTable. The cache keeps the
// strings strongly.
// TODO(537744910): The cache keeps the strings strongly. Since it's
// thread-local, it's safe to clean the entries with use-count being 1.
// Consider doing this from an idle task if memory becomes an issue.
// Alternatively, use the cache only for the main thread.
struct alignas(64) SmallStringCache {
  // The cache size is 2^13 = 8192 entries (256 KB per thread).
  static constexpr size_t kLogSize = 13;
  static constexpr size_t kSize = 1 << kLogSize;
  static constexpr size_t kHashShift = 64 - kLogSize;

  struct alignas(32) Entry {
    String string;
    uint64_t sig_low = 0;
    uint64_t sig_high = 0;
    uint32_t length = 0;
    uint32_t unused = 0;
  };

  alignas(64) std::array<Entry, kSize> entries;
};

NOINLINE SmallStringCache* SmallStringCacheInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<SmallStringCache>, cache, ());
  return cache;
}

constinit thread_local SmallStringCache* g_small_string_cache = nullptr;

template <typename Generator>
ALWAYS_INLINE String SmallStringCacheGetOrInsert(uint64_t sig_low,
                                                 uint64_t sig_high,
                                                 uint32_t length,
                                                 Generator generator) {
  // Fibonacci hash using the golden ratio constant to distribute strings
  // evenly across the cache.
  uint64_t hash_key = sig_low ^ sig_high ^ length;
  const size_t index =
      (hash_key * kGoldenRatio64) >> SmallStringCache::kHashShift;

  SmallStringCache* cache = g_small_string_cache;
  if (!cache) [[unlikely]] {
    g_small_string_cache = SmallStringCacheInstance();
    cache = g_small_string_cache;
  }

  auto& entry = cache->entries[index];
  if (entry.sig_low == sig_low && entry.sig_high == sig_high &&
      entry.length == length) [[likely]] {
    return entry.string;
  }

  String result = generator();
  entry.string = result;
  entry.sig_low = sig_low;
  entry.sig_high = sig_high;
  entry.length = length;
  return result;
}

template <typename T, typename U, size_t N>
ALWAYS_INLINE static T BitCastRead(base::span<const U, N> span) {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(sizeof(T) == N * sizeof(U));
  using Array = std::array<U, N>;
  // SAFETY: `span` has fixed extent N matching exactly `sizeof(T)`.
  return base::bit_cast<T>(*reinterpret_cast<const Array*>(span.data()));
}

ALWAYS_INLINE static uint16_t Compress2UCharsToUint16(
    base::span<const UChar, 2> chars) {
  return static_cast<uint16_t>(static_cast<uint8_t>(chars[0])) |
         (static_cast<uint16_t>(static_cast<uint8_t>(chars[1])) << 8);
}

ALWAYS_INLINE static uint32_t Compress4UCharsToUint32(
    base::span<const UChar, 4> chars) {
  return static_cast<uint32_t>(static_cast<uint8_t>(chars[0])) |
         (static_cast<uint32_t>(static_cast<uint8_t>(chars[1])) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(chars[2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(chars[3])) << 24);
}

ALWAYS_INLINE static uint64_t Compress8UCharsToUint64(
    base::span<const UChar, 8> chars) {
  return static_cast<uint64_t>(static_cast<uint8_t>(chars[0])) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[1])) << 8) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[2])) << 16) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[3])) << 24) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[4])) << 32) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[5])) << 40) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[6])) << 48) |
         (static_cast<uint64_t>(static_cast<uint8_t>(chars[7])) << 56);
}

ALWAYS_INLINE static std::pair<uint64_t, uint64_t> ComputeSmallStringSignature(
    base::span<const LChar> chars) {
  const size_t length = chars.size();
  DCHECK(length >= 1 && length <= 16);
  if (length >= 8) {
    return {BitCastRead<uint64_t>(chars.first<8>()),
            BitCastRead<uint64_t>(chars.last<8>())};
  }
  if (length >= 4) {
    uint32_t low32 = BitCastRead<uint32_t>(chars.first<4>());
    uint32_t high32 = BitCastRead<uint32_t>(chars.last<4>());
    return {low32 | (static_cast<uint64_t>(high32) << 32), 0};
  }
  if (length >= 2) {
    uint16_t low16 = BitCastRead<uint16_t>(chars.first<2>());
    uint16_t high16 = BitCastRead<uint16_t>(chars.last<2>());
    return {low16 | (static_cast<uint64_t>(high16) << 16), 0};
  }
  return {chars[0], 0};
}

ALWAYS_INLINE static std::pair<uint64_t, uint64_t> ComputeSmallStringSignature(
    base::span<const UChar> chars) {
  const size_t length = chars.size();
  DCHECK(length >= 1 && length <= 16);
  if (length >= 8) {
    return {Compress8UCharsToUint64(chars.first<8>()),
            Compress8UCharsToUint64(chars.last<8>())};
  }
  if (length >= 4) {
    uint32_t low32 = Compress4UCharsToUint32(chars.first<4>());
    uint32_t high32 = Compress4UCharsToUint32(chars.last<4>());
    return {low32 | (static_cast<uint64_t>(high32) << 32), 0};
  }
  if (length >= 2) {
    uint16_t low16 = Compress2UCharsToUint16(chars.first<2>());
    uint16_t high16 = Compress2UCharsToUint16(chars.last<2>());
    return {low16 | (static_cast<uint64_t>(high16) << 16), 0};
  }
  return {static_cast<uint8_t>(chars[0]), 0};
}

// The compiler will conveniently combine this into a single 64-bit load for us,
// as long as it is reasonably obvious that it can elide the bounds checks.
ALWAYS_INLINE static uint64_t Read4Chars(base::span<const UChar> chars,
                                         size_t start) {
  static_assert(std::is_unsigned_v<UChar>);
  return static_cast<uint64_t>(chars[start]) |
         (static_cast<uint64_t>(chars[start + 1]) << 16) |
         (static_cast<uint64_t>(chars[start + 2]) << 32) |
         (static_cast<uint64_t>(chars[start + 3]) << 48);
}

ALWAYS_INLINE static bool IsOnly8Bit(base::span<const UChar> chars) {
  if (chars.size() >= 4) {
    for (size_t i = 0; i + 3 < chars.size(); i += 4) {
      if (Read4Chars(chars, i) & 0xFF00FF00FF00FF00ULL) {
        return false;
      }
    }
    // NOTE: The tail will overlap already-tested characters,
    // but that is completely OK.
    return !(Read4Chars(chars, chars.size() - 4) & 0xFF00FF00FF00FF00ULL);
  } else {
    return !std::ranges::any_of(chars, [](UChar ch) { return ch & 0xFF00; });
  }
}

class UCharBuffer {
 public:
  ALWAYS_INLINE static unsigned ComputeHashAndMaskTop8Bits(
      base::span<const UChar> chars,
      AtomicStringUCharEncoding encoding) {
    base::span<const char> bytes = base::as_chars(chars);
    switch (encoding) {
      case AtomicStringUCharEncoding::kUnknown:
        // encoding is always resolved in the constructor.
        NOTREACHED();
      case AtomicStringUCharEncoding::kIs8Bit: {
        using Reader = ConvertTo8BitHashReader;
        // This is a very common case from HTML parsing, so we take
        // the size penalty from inlining.
        return StringHasher::ComputeHashAndMaskTop8BitsInline<Reader>(
            UNSAFE_TODO({base::unchecked, base::as_bytes(bytes).data(),
                         bytes.size() / Reader::kCompressionFactor}));
      }
      case AtomicStringUCharEncoding::kIs16Bit:
        return StringHasher::ComputeHashAndMaskTop8Bits(bytes.data(),
                                                        bytes.size());
    }
  }

  ALWAYS_INLINE UCharBuffer(base::span<const UChar> chars,
                            AtomicStringUCharEncoding encoding)
      : characters_(chars),
        encoding_(encoding == AtomicStringUCharEncoding::kUnknown
                      ? (IsOnly8Bit(chars)
                             ? AtomicStringUCharEncoding::kIs8Bit
                             : AtomicStringUCharEncoding::kIs16Bit)
                      : encoding),
        hash_(ComputeHashAndMaskTop8Bits(chars, encoding_)) {}

  ALWAYS_INLINE UCharBuffer(base::span<const UChar> chars,
                            unsigned hash,
                            AtomicStringUCharEncoding encoding)
      : characters_(chars),
        encoding_(encoding == AtomicStringUCharEncoding::kUnknown
                      ? (IsOnly8Bit(chars)
                             ? AtomicStringUCharEncoding::kIs8Bit
                             : AtomicStringUCharEncoding::kIs16Bit)
                      : encoding),
        hash_(hash) {}

  base::span<const UChar> characters() const { return characters_; }
  unsigned hash() const { return hash_; }
  AtomicStringUCharEncoding encoding() const { return encoding_; }

  scoped_refptr<StringImpl> CreateStringImpl() const {
    switch (encoding_) {
      case AtomicStringUCharEncoding::kUnknown:
        // encoding_ is always resolved in the constructor.
        NOTREACHED();
      case AtomicStringUCharEncoding::kIs8Bit:
        return String::Make8BitFrom16BitSource(characters_).ReleaseImpl();
      case AtomicStringUCharEncoding::kIs16Bit:
        return StringImpl::Create(characters_);
    }
  }

 private:
  const base::span<const UChar> characters_;
  const AtomicStringUCharEncoding encoding_;
  const unsigned hash_;
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

    base::span<const char> bytes = base::as_chars(buf.RawByteSpan());
    if (buf.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits(bytes.data(),
                                                      bytes.size());
    } else if (IsOnly8Bit(buf.Span16())) {
      using Reader = ConvertTo8BitHashReader;
      return StringHasher::ComputeHashAndMaskTop8Bits<Reader>(
          bytes.data(), bytes.size() / Reader::kCompressionFactor);
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits(bytes.data(),
                                                      bytes.size());
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

class HashTranslatorLowercaseBuffer {
 public:
  explicit HashTranslatorLowercaseBuffer(const StringImpl* impl) : impl_(impl) {
    // We expect already lowercase strings to take another path in
    // Element::WeakLowercaseIfNecessary.
    DCHECK(!impl_->ContainsNoAsciiUpper());
    base::span<const char> bytes = base::as_chars(impl->RawByteSpan());
    if (impl_->Is8Bit()) {
      hash_ =
          StringHasher::ComputeHashAndMaskTop8Bits<AsciiLowerHashReader<LChar>>(
              bytes.data(), bytes.size());
    } else {
      if (IsOnly8Bit(impl_->Span16())) {
        using Reader = AsciiConvertTo8AndLowerHashReader;
        hash_ = StringHasher::ComputeHashAndMaskTop8Bits<Reader>(
            bytes.data(), bytes.size() / Reader::kCompressionFactor);
      } else {
        hash_ = StringHasher::ComputeHashAndMaskTop8Bits<
            AsciiLowerHashReader<UChar>>(bytes.data(), bytes.size());
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
    // This is similar to EqualIgnoringAsciiCase, but not the same.
    // In particular, it validates that |bucket| is a lowercase version of
    // |buf.impl()|.
    //
    // Unlike EqualIgnoringAsciiCase, it returns false if they are equal
    // ignoring ASCII case but |bucket| contains an uppercase ASCII character.
    //
    // However, similar optimizations are used here as there, so these should
    // have generally similar correctness and performance constraints.
    const StringImpl* query = buf.impl();
    if (bucket->length() != query->length())
      return false;
    if (bucket->RawByteSpan().data() == query->RawByteSpan().data() &&
        bucket->Is8Bit() == query->Is8Bit()) {
      return query->ContainsNoAsciiUpper();
    }
    return VisitCharacters(*bucket, [&](auto bch) {
      return VisitCharacters(*query, [&](auto qch) {
        wtf_size_t len = query->length();
        for (wtf_size_t i = 0; i < len; ++i) {
          if (bch[i] != ToAsciiLower(qch[i])) {
            return false;
          }
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
String AtomicStringTable::AddToStringTable(const T& value) {
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

String AtomicStringTable::Add(base::span<const UChar> chars,
                              AtomicStringUCharEncoding encoding) {
  if (!chars.data()) {
    return String();
  }

  if (chars.empty()) {
    return StringImpl::empty_;
  }

  if (encoding == AtomicStringUCharEncoding::kUnknown) {
    encoding = IsOnly8Bit(chars) ? AtomicStringUCharEncoding::kIs8Bit
                                 : AtomicStringUCharEncoding::kIs16Bit;
  }

  const auto length = chars.size();
  if (encoding == AtomicStringUCharEncoding::kIs8Bit && length <= 16) {
    const auto [sig_low, sig_high] = ComputeSmallStringSignature(chars);
    return SmallStringCacheGetOrInsert(
        sig_low, sig_high, static_cast<uint32_t>(length), [this, &chars]() {
          return AddToStringTable<UCharBuffer, UCharBufferTranslator>(
              UCharBuffer(chars, AtomicStringUCharEncoding::kIs8Bit));
        });
  }

  UCharBuffer buffer(chars, encoding);
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

namespace {

class LCharBuffer {
 public:
  ALWAYS_INLINE explicit LCharBuffer(base::span<const LChar> chars)
      : characters_(chars),
        // This is a common path from V8 strings, so inlining is worth it.
        hash_(StringHasher::ComputeHashAndMaskTop8BitsInline(chars)) {}

  ALWAYS_INLINE LCharBuffer(base::span<const LChar> chars, unsigned hash)
      : characters_(chars), hash_(hash) {}

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

}  // namespace

String AtomicStringTable::Add(const StringView& string_view) {
  if (string_view.IsNull()) {
    return String();
  }

  if (string_view.empty()) {
    return StringImpl::empty_;
  }

  if (StringImpl* impl = string_view.SharedImpl(); impl && impl->IsAtomic()) {
    return String(impl);
  }

  const auto length = string_view.length();
  if (length <= 16 && string_view.Is8Bit()) {
    base::span<const LChar> chars = string_view.Span8();
    const auto [sig_low, sig_high] = ComputeSmallStringSignature(chars);
    return SmallStringCacheGetOrInsert(
        sig_low, sig_high, static_cast<uint32_t>(length), [this, &chars]() {
          return AddToStringTable<LCharBuffer, LCharBufferTranslator>(
              LCharBuffer(chars));
        });
  }

  if (string_view.Is8Bit()) {
    return AddToStringTable<LCharBuffer, LCharBufferTranslator>(
        LCharBuffer(string_view.Span8()));
  }

  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(
      UCharBuffer(string_view.Span16(), AtomicStringUCharEncoding::kUnknown));
}

String AtomicStringTable::Add(base::span<const LChar> chars) {
  if (!chars.data()) {
    return String();
  }

  if (chars.empty()) {
    return StringImpl::empty_;
  }

  const auto length = chars.size();
  if (length <= 16) {
    const auto [sig_low, sig_high] = ComputeSmallStringSignature(chars);
    return SmallStringCacheGetOrInsert(
        sig_low, sig_high, static_cast<uint32_t>(length), [this, &chars]() {
          return AddToStringTable<LCharBuffer, LCharBufferTranslator>(
              LCharBuffer(chars));
        });
  }

  return AddToStringTable<LCharBuffer, LCharBufferTranslator>(
      LCharBuffer(chars));
}

StringImpl* AtomicStringTable::AddNoLock(StringImpl* string) {
  auto result = table_.insert(string);
  StringImpl* entry = *result.stored_value;
  if (result.is_new_entry)
    entry->SetIsAtomic();

  DCHECK(!string->IsStatic() || entry->IsStatic());
  return entry;
}

String AtomicStringTable::Add(StringImpl* string) {
  if (!string->length())
    return StringImpl::empty_;

  // Lock not only protects access to the table, it also guarantees
  // mutual exclusion with the refcount decrement on removal.
  base::AutoLock auto_lock(lock_);
  return base::WrapRefCounted(AddNoLock(string));
}

String AtomicStringTable::Add(String&& string) {
  if (!string.length()) {
    return StringImpl::empty_;
  }

  // Lock not only protects access to the table, it also guarantees
  // mutual exclusion with the refcount decrement on removal.
  base::AutoLock auto_lock(lock_);
  StringImpl* entry = AddNoLock(string.Impl());
  if (entry == string.Impl()) {
    return std::move(string);
  }

  return base::WrapRefCounted(entry);
}

String AtomicStringTable::AddUtf8(base::span<const uint8_t> characters_span) {
  bool seen_non_ascii = false;
  bool seen_non_latin1 = false;

  unsigned utf16_length = blink::unicode::CalculateStringLengthFromUtf8(
      characters_span, seen_non_ascii, seen_non_latin1);
  if (!seen_non_ascii) {
    return Add(characters_span);
  }

  // If CalculateStringLengthFromUtf8() detects invalid UTF-8, it will return
  // 0. Calling ConvertUtf8ToUtf16() with a zero-length UTF-16 buffer will
  // cause it to return a status of kTargetExhausted. Return a null String in
  // this case instead. This matches String::FromUtf8(). If there are no
  // characters, `seen_non_ascii` will be false, and thus the ASCII code-path
  // will have been taken.
  if (utf16_length == 0) {
    return String();
  }

  auto utf16_buf = base::HeapArray<UChar>::Uninit(utf16_length);
  if (!unicode::ConvertUtf8ToUtf16(characters_span, utf16_buf).IsSuccess()) {
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
  DCHECK(!string.ContainsNoAsciiUpper());
  DCHECK(string.length());
  HashTranslatorLowercaseBuffer buffer(string.Impl());
  base::AutoLock auto_lock(lock_);
  const auto& it = table_.Find<LowercaseLookupTranslator>(buffer);
  if (it == table_.end())
    return WeakResult();
  DCHECK(StringView(*it).ContainsNoAsciiUpper());
  DCHECK(EqualIgnoringAsciiCase(*it, string));
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
