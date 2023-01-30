// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

namespace WTF {

namespace {

class UCharBuffer {
 public:
  UCharBuffer(const UChar* chars,
              unsigned len,
              AtomicStringUCharEncoding encoding)
      : characters_(chars),
        length_(len),
        hash_(StringHasher::ComputeHashAndMaskTop8Bits(chars, len)),
        encoding_(encoding) {}

  const UChar* characters() const { return characters_; }
  unsigned length() const { return length_; }
  unsigned hash() const { return hash_; }
  AtomicStringUCharEncoding encoding() const { return encoding_; }

  scoped_refptr<StringImpl> CreateStringImpl() const {
    switch (encoding_) {
      case AtomicStringUCharEncoding::kUnknown:
        return StringImpl::Create8BitIfPossible(characters_, length_);
      case AtomicStringUCharEncoding::kIs8Bit:
        return String::Make8BitFrom16BitSource(characters_, length_)
            .ReleaseImpl();
      case AtomicStringUCharEncoding::kIs16Bit:
        return StringImpl::Create(characters_, length_);
    }
  }

 private:
  const UChar* characters_;
  const unsigned length_;
  const unsigned hash_;
  const AtomicStringUCharEncoding encoding_;
};

struct UCharBufferTranslator {
  static unsigned GetHash(const UCharBuffer& buf) { return buf.hash(); }

  static bool Equal(StringImpl* const& str, const UCharBuffer& buf) {
    return WTF::Equal(str, buf.characters(), buf.length());
  }

  static void Store(StringImpl*& location,
                    const UCharBuffer& buf,
                    unsigned hash) {
    location = buf.CreateStringImpl().release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

class HashAndUTF8Characters {
 public:
  HashAndUTF8Characters(const char* chars_start, const char* chars_end)
      : characters_(chars_start),
        hash_(unicode::CalculateStringHashAndLengthFromUTF8MaskingTop8Bits(
            chars_start,
            chars_end,
            length_,
            utf16_length_)) {}

  const char* characters() const { return characters_; }
  unsigned length() const { return length_; }
  unsigned utf16_length() const { return utf16_length_; }
  unsigned hash() const { return hash_; }

 private:
  const char* characters_;
  unsigned length_;
  unsigned utf16_length_;
  unsigned hash_;
};

struct HashAndUTF8CharactersTranslator {
  static unsigned GetHash(const HashAndUTF8Characters& buffer) {
    return buffer.hash();
  }

  static bool Equal(StringImpl* const& string,
                    const HashAndUTF8Characters& buffer) {
    if (buffer.utf16_length() != string->length())
      return false;

    // If buffer contains only ASCII characters UTF-8 and UTF16 length are the
    // same.
    if (buffer.utf16_length() != buffer.length()) {
      if (string->Is8Bit()) {
        const LChar* characters8 = string->Characters8();
        return unicode::EqualLatin1WithUTF8(
            characters8, characters8 + string->length(), buffer.characters(),
            buffer.characters() + buffer.length());
      }
      const UChar* characters16 = string->Characters16();
      return unicode::EqualUTF16WithUTF8(
          characters16, characters16 + string->length(), buffer.characters(),
          buffer.characters() + buffer.length());
    }

    if (string->Is8Bit()) {
      const LChar* string_characters = string->Characters8();

      for (unsigned i = 0; i < buffer.length(); ++i) {
        DCHECK(IsASCII(buffer.characters()[i]));
        if (string_characters[i] != buffer.characters()[i])
          return false;
      }

      return true;
    }

    const UChar* string_characters = string->Characters16();

    for (unsigned i = 0; i < buffer.length(); ++i) {
      DCHECK(IsASCII(buffer.characters()[i]));
      if (string_characters[i] != buffer.characters()[i])
        return false;
    }

    return true;
  }

  static void Store(StringImpl*& location,
                    const HashAndUTF8Characters& buffer,
                    unsigned hash) {
    scoped_refptr<StringImpl> new_string;
    // If buffer contains only ASCII characters, the UTF-8 and UTF-16 lengths
    // are the same.
    bool is_all_ascii = buffer.utf16_length() == buffer.length();
    if (!is_all_ascii) {
      UChar* target;
      new_string =
          StringImpl::CreateUninitialized(buffer.utf16_length(), target);

      const char* source = buffer.characters();
      if (unicode::ConvertUTF8ToUTF16(&source, source + buffer.length(),
                                      &target, target + buffer.utf16_length(),
                                      &is_all_ascii) != unicode::kConversionOK)
        NOTREACHED();
    } else {
      new_string = StringImpl::Create(buffer.characters(), buffer.length());
    }
    location = new_string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

struct StringViewLookupTranslator {
  static unsigned GetHash(const StringView& buf) {
    StringImpl* shared_impl = buf.SharedImpl();
    if (LIKELY(shared_impl))
      return shared_impl->GetHash();

    if (buf.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits(buf.Characters8(),
                                                      buf.length());
    } else {
      return StringHasher::ComputeHashAndMaskTop8Bits(buf.Characters16(),
                                                      buf.length());
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
    DCHECK(!impl_->IsLowerASCII());
    if (impl_->Is8Bit()) {
      hash_ =
          StringHasher::ComputeHashAndMaskTop8Bits<LChar,
                                                   ToASCIILowerUChar<LChar>>(
              impl_->Characters8(), impl_->length());
    } else {
      hash_ =
          StringHasher::ComputeHashAndMaskTop8Bits<UChar,
                                                   ToASCIILowerUChar<UChar>>(
              impl_->Characters16(), impl_->length());
    }
  }

  const StringImpl* impl() const { return impl_; }
  unsigned hash() const { return hash_; }

 private:
  template <typename CharType>
  static UChar ToASCIILowerUChar(CharType ch) {
    return ToASCIILower(ch);
  }

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
    if (bucket->Bytes() == query->Bytes() &&
        bucket->Is8Bit() == query->Is8Bit())
      return query->IsLowerASCII();
    return WTF::VisitCharacters(*bucket, [&](const auto* bch, wtf_size_t) {
      return WTF::VisitCharacters(*query, [&](const auto* qch, wtf_size_t len) {
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
    const UChar* s,
    unsigned length,
    AtomicStringUCharEncoding encoding) {
  if (!s)
    return nullptr;

  if (!length)
    return StringImpl::empty_;

  UCharBuffer buffer(s, length, encoding);
  return AddToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

class LCharBuffer {
 public:
  LCharBuffer(const LChar* chars, unsigned len)
      : characters_(chars),
        length_(len),
        hash_(StringHasher::ComputeHashAndMaskTop8Bits(chars, len)) {}

  const LChar* characters() const { return characters_; }
  unsigned length() const { return length_; }
  unsigned hash() const { return hash_; }

 private:
  const LChar* characters_;
  const unsigned length_;
  const unsigned hash_;
};

struct LCharBufferTranslator {
  static unsigned GetHash(const LCharBuffer& buf) { return buf.hash(); }

  static bool Equal(StringImpl* const& str, const LCharBuffer& buf) {
    return WTF::Equal(str, buf.characters(), buf.length());
  }

  static void Store(StringImpl*& location,
                    const LCharBuffer& buf,
                    unsigned hash) {
    auto string = StringImpl::Create(buf.characters(), buf.length());
    location = string.release();
    location->SetHash(hash);
    location->SetIsAtomic();
  }
};

scoped_refptr<StringImpl> AtomicStringTable::Add(const LChar* s,
                                                 unsigned length) {
  if (!s)
    return nullptr;

  if (!length)
    return StringImpl::empty_;

  LCharBuffer buffer(s, length);
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
    const char* characters_start,
    const char* characters_end) {
  HashAndUTF8Characters buffer(characters_start, characters_end);

  if (!buffer.hash())
    return nullptr;

  return AddToStringTable<HashAndUTF8Characters,
                          HashAndUTF8CharactersTranslator>(buffer);
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

}  // namespace WTF
