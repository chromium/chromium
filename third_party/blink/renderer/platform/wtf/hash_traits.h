/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TRAITS_H_

#include <string.h>

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/atomic_operations.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace WTF {

template <typename T>
struct HashTraits;

namespace internal {

template <typename T, bool = IsTraceable<T>::value>
struct ClearMemoryAtomicallyIfNeeded {
  static void Clear(T* slot) { memset(static_cast<void*>(slot), 0, sizeof(T)); }
};
template <typename T>
struct ClearMemoryAtomicallyIfNeeded<T, true> {
  static void Clear(T* slot) { AtomicMemzero<sizeof(T), alignof(T)>(slot); }
};

template <typename T>
struct GenericHashTraitsBase {
  using TraitType = T;
  using EmptyValueType = T;

  // Type for functions that do not take ownership, such as contains.
  using PeekInType = const T&;

  // Types for iterators.
  using IteratorGetType = T*;
  using IteratorConstGetType = const T*;
  using IteratorReferenceType = T&;
  using IteratorConstReferenceType = const T&;

  template <typename IncomingValueType>
  static void Store(IncomingValueType&& value, T& storage) {
    storage = std::forward<IncomingValueType>(value);
  }

  // Type for return value of functions that do not transfer ownership, such
  // as get.
  using PeekOutType = T;
  static const T& Peek(const T& value) { return value; }

  // This flag can be set to true if any of the following conditions is true:
  // 1. All bytes of EmptyValue() are zero.
  // 2. kHasEmptyValueFunction is true and IsEmptyValue() returns true for a
  //    value containing all zero bytes.
  // When this is true, the hash table can optimize allocation of empty hash
  // slots with zeroed memory without calling EmptyValue().
  static constexpr bool kEmptyValueIsZero = false;

  // If this flag is true, the hash table will call function
  //   static bool IsEmptyValue(const T&);
  // to check if an entry is an empty value. Otherwise the hash table will
  // check for the empty value with the equality operator.
  // See IsHashTraitsEmptyValue().
  static constexpr bool kHasIsEmptyValueFunction = false;

// The starting table size. Can be overridden when we know beforehand that a
// hash table will have at least N entries.
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // The allocation pool for nodes is one big chunk that ASAN has no insight
  // into, so it can cloak errors. Make it as small as possible to force nodes
  // to be allocated individually where ASAN can see them.
  static constexpr unsigned kMinimumTableSize = 1;
#else
  static constexpr unsigned kMinimumTableSize = 8;
#endif

  // When a hash table backing store is traced, its elements will be
  // traced if their class type has a trace method. However, weak-referenced
  // elements should not be traced then, but handled by the weak processing
  // phase that follows.
  template <typename U = void>
  struct IsTraceableInCollection {
    static constexpr bool value = IsTraceable<T>::value && !IsWeak<T>::value;
  };

  // The NeedsToForbidGCOnMove flag is used to make the hash table move
  // operations safe when GC is enabled: if a move constructor invokes
  // an allocation triggering the GC then it should be invoked within GC
  // forbidden scope.
  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    // TODO(yutak): Consider using of std:::is_trivially_move_constructible
    // when it is accessible.
    static constexpr bool value = !std::is_pod<T>::value;
  };

  // The kCanTraceConcurrently value is used by Oilpan concurrent marking. Only
  // type for which HashTraits<T>::kCanTraceConcurrently is true can be traced
  // on a concurrent thread.
  static constexpr bool kCanTraceConcurrently = false;
};

template <typename T, auto empty_value, auto deleted_value>
struct IntOrEnumHashTraits : internal::GenericHashTraitsBase<T> {
  static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
  static constexpr bool kEmptyValueIsZero =
      static_cast<int64_t>(empty_value) == 0;
  static constexpr T EmptyValue() { return static_cast<T>(empty_value); }
  static void ConstructDeletedValue(T& slot, bool) {
    slot = static_cast<T>(deleted_value);
  }
  static constexpr bool IsDeletedValue(T value) {
    return value == static_cast<T>(deleted_value);
  }
};

}  // namespace internal

// Default integer traits disallow both 0 and -1 as keys (max value instead of
// -1 for unsigned).
template <typename T, T empty_value = 0, T deleted_value = static_cast<T>(-1)>
struct IntHashTraits
    : internal::IntOrEnumHashTraits<T, empty_value, deleted_value> {
  static_assert(std::is_integral<T>::value);
};

// Default traits for an enum type.  0 is very popular, and -1 is also popular.
// So we use -128 and -127.
template <typename T, auto empty_value = -128, auto deleted_value = -127>
struct EnumHashTraits
    : internal::IntOrEnumHashTraits<T, empty_value, deleted_value> {
  static_assert(std::is_enum<T>::value);
};

template <typename T, typename Enable = void>
struct GenericHashTraits : internal::GenericHashTraitsBase<T> {
  static_assert(!std::is_integral<T>::value);
  static_assert(!std::is_enum<T>::value);
  static T EmptyValue() { return T(); }
};

template <typename T>
struct GenericHashTraits<T, std::enable_if_t<std::is_integral<T>::value>>
    : public IntHashTraits<T> {};

template <typename T>
struct GenericHashTraits<T, std::enable_if_t<std::is_enum<T>::value>>
    : public EnumHashTraits<T> {};

template <typename T>
struct HashTraits : GenericHashTraits<T> {};

template <typename T>
struct FloatHashTraits : GenericHashTraits<T> {
  static T EmptyValue() { return std::numeric_limits<T>::infinity(); }
  static void ConstructDeletedValue(T& slot, bool) {
    slot = -std::numeric_limits<T>::infinity();
  }
  static bool IsDeletedValue(T value) {
    return value == -std::numeric_limits<T>::infinity();
  }
};

template <>
struct HashTraits<float> : FloatHashTraits<float> {};
template <>
struct HashTraits<double> : FloatHashTraits<double> {};

// Default integral traits disallow both 0 and max as keys -- use these traits
// to allow zero and disallow max - 1.
template <typename T>
struct IntWithZeroKeyHashTraits
    : IntHashTraits<T,
                    std::numeric_limits<T>::max(),
                    std::numeric_limits<T>::max() - 1> {};

template <typename P>
struct HashTraits<P*> : GenericHashTraits<P*> {
  static constexpr bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(P*& slot, bool) {
    slot = reinterpret_cast<P*>(-1);
  }
  static bool IsDeletedValue(const P* value) {
    return value == reinterpret_cast<P*>(-1);
  }
};

template <typename T>
struct SimpleClassHashTraits : GenericHashTraits<T> {
  static constexpr bool kEmptyValueIsZero = true;
  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    static constexpr bool value = false;
  };
  static void ConstructDeletedValue(T& slot, bool) {
    new (NotNullTag::kNotNull, &slot) T(kHashTableDeletedValue);
  }
  static bool IsDeletedValue(const T& value) {
    return value.IsHashTableDeletedValue();
  }
};

// Default traits disallow both 0 and max as keys -- use these traits to allow
// all values as keys.
template <typename T>
struct HashTraits<IntegralWithAllKeys<T>>
    : SimpleClassHashTraits<IntegralWithAllKeys<T>> {};

template <typename P>
struct HashTraits<scoped_refptr<P>> : SimpleClassHashTraits<scoped_refptr<P>> {
  static_assert(sizeof(void*) == sizeof(scoped_refptr<P>),
                "Unexpected RefPtr size."
                " RefPtr needs to be single pointer to support deleted value.");

  class RefPtrValuePeeker {
    DISALLOW_NEW();

   public:
    ALWAYS_INLINE RefPtrValuePeeker(P* p) : ptr_(p) {}
    template <typename U>
    RefPtrValuePeeker(const scoped_refptr<U>& p) : ptr_(p.get()) {}

    ALWAYS_INLINE operator P*() const { return ptr_; }

   private:
    P* ptr_;
  };

  typedef std::nullptr_t EmptyValueType;
  static EmptyValueType EmptyValue() { return nullptr; }

  static constexpr bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const scoped_refptr<P>& value) { return !value; }

  static bool IsDeletedValue(const scoped_refptr<P>& value) {
    return *reinterpret_cast<void* const*>(&value) ==
           reinterpret_cast<const void*>(-1);
  }

  static void ConstructDeletedValue(scoped_refptr<P>& slot, bool zero_value) {
    *reinterpret_cast<void**>(&slot) = reinterpret_cast<void*>(-1);
  }

  typedef RefPtrValuePeeker PeekInType;
  typedef scoped_refptr<P>* IteratorGetType;
  typedef const scoped_refptr<P>* IteratorConstGetType;
  typedef scoped_refptr<P>& IteratorReferenceType;
  typedef const scoped_refptr<P>& IteratorConstReferenceType;

  static void Store(scoped_refptr<P> value, scoped_refptr<P>& storage) {
    storage = std::move(value);
  }

  typedef P* PeekOutType;
  static PeekOutType Peek(const scoped_refptr<P>& value) { return value.get(); }
};

template <typename T>
struct HashTraits<std::unique_ptr<T>>
    : SimpleClassHashTraits<std::unique_ptr<T>> {
  using EmptyValueType = std::nullptr_t;
  static EmptyValueType EmptyValue() { return nullptr; }

  static constexpr bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const std::unique_ptr<T>& value) { return !value; }

  using PeekInType = T*;

  static void Store(std::unique_ptr<T>&& value, std::unique_ptr<T>& storage) {
    storage = std::move(value);
  }

  using PeekOutType = T*;
  static PeekOutType Peek(const std::unique_ptr<T>& value) {
    return value.get();
  }

  static void ConstructDeletedValue(std::unique_ptr<T>& slot, bool) {
    // Dirty trick: implant an invalid pointer to unique_ptr. Destructor isn't
    // called for deleted buckets, so this is okay.
    new (NotNullTag::kNotNull, &slot)
        std::unique_ptr<T>(reinterpret_cast<T*>(1u));
  }
  static bool IsDeletedValue(const std::unique_ptr<T>& value) {
    return value.get() == reinterpret_cast<T*>(1u);
  }
};

template <>
struct HashTraits<String> : SimpleClassHashTraits<String> {
  static constexpr bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const String&);
  static bool IsDeletedValue(const String& value);
  static void ConstructDeletedValue(String& slot, bool zero_value);
};

// This struct template is an implementation detail of the
// isHashTraitsEmptyValue function, which selects either the emptyValue function
// or the isEmptyValue function to check for empty values.
template <typename Traits, bool hasEmptyValueFunction>
struct HashTraitsEmptyValueChecker;
template <typename Traits>
struct HashTraitsEmptyValueChecker<Traits, true> {
  template <typename T>
  static bool IsEmptyValue(const T& value) {
    return Traits::IsEmptyValue(value);
  }
};
template <typename Traits>
struct HashTraitsEmptyValueChecker<Traits, false> {
  template <typename T>
  static bool IsEmptyValue(const T& value) {
    return value == Traits::EmptyValue();
  }
};
template <typename Traits, typename T>
inline bool IsHashTraitsEmptyValue(const T& value) {
  return HashTraitsEmptyValueChecker<
      Traits, Traits::kHasIsEmptyValueFunction>::IsEmptyValue(value);
}

template <typename FirstTraitsArg, typename SecondTraitsArg>
struct PairHashTraits
    : GenericHashTraits<std::pair<typename FirstTraitsArg::TraitType,
                                  typename SecondTraitsArg::TraitType>> {
  typedef FirstTraitsArg FirstTraits;
  typedef SecondTraitsArg SecondTraits;
  typedef std::pair<typename FirstTraits::TraitType,
                    typename SecondTraits::TraitType>
      TraitType;
  typedef std::pair<typename FirstTraits::EmptyValueType,
                    typename SecondTraits::EmptyValueType>
      EmptyValueType;

  static constexpr bool kEmptyValueIsZero =
      FirstTraits::kEmptyValueIsZero && SecondTraits::kEmptyValueIsZero;
  static EmptyValueType EmptyValue() {
    return std::make_pair(FirstTraits::EmptyValue(),
                          SecondTraits::EmptyValue());
  }

  static constexpr bool kHasIsEmptyValueFunction =
      FirstTraits::kHasIsEmptyValueFunction ||
      SecondTraits::kHasIsEmptyValueFunction;
  static bool IsEmptyValue(const TraitType& value) {
    return IsHashTraitsEmptyValue<FirstTraits>(value.first) &&
           IsHashTraitsEmptyValue<SecondTraits>(value.second);
  }

  static constexpr unsigned kMinimumTableSize = FirstTraits::kMinimumTableSize;

  static void ConstructDeletedValue(TraitType& slot, bool zero_value) {
    FirstTraits::ConstructDeletedValue(slot.first, zero_value);
    // For GC collections the memory for the backing is zeroed when it is
    // allocated, and the constructors may take advantage of that,
    // especially if a GC occurs during insertion of an entry into the
    // table. This slot is being marked deleted, but If the slot is reused
    // at a later point, the same assumptions around memory zeroing must
    // hold as they did at the initial allocation.  Therefore we zero the
    // value part of the slot here for GC collections.
    if (zero_value) {
      internal::ClearMemoryAtomicallyIfNeeded<
          typename SecondTraits::TraitType>::Clear(&slot.second);
    }
  }
  static bool IsDeletedValue(const TraitType& value) {
    return FirstTraits::IsDeletedValue(value.first);
  }
};

template <typename First, typename Second>
struct HashTraits<std::pair<First, Second>>
    : public PairHashTraits<HashTraits<First>, HashTraits<Second>> {};

template <typename KeyTypeArg, typename ValueTypeArg>
struct KeyValuePair {
  typedef KeyTypeArg KeyType;

  template <typename IncomingKeyType, typename IncomingValueType>
  KeyValuePair(IncomingKeyType&& key, IncomingValueType&& value)
      : key(std::forward<IncomingKeyType>(key)),
        value(std::forward<IncomingValueType>(value)) {}

  template <typename OtherKeyType, typename OtherValueType>
  KeyValuePair(KeyValuePair<OtherKeyType, OtherValueType>&& other)
      : key(std::move(other.key)), value(std::move(other.value)) {}

  KeyTypeArg key;
  ValueTypeArg value;
};

template <typename K, typename V>
struct IsWeak<KeyValuePair<K, V>>
    : std::integral_constant<bool, IsWeak<K>::value || IsWeak<V>::value> {};

template <typename KeyTraitsArg, typename ValueTraitsArg>
struct KeyValuePairHashTraits
    : GenericHashTraits<KeyValuePair<typename KeyTraitsArg::TraitType,
                                     typename ValueTraitsArg::TraitType>> {
  typedef KeyTraitsArg KeyTraits;
  typedef ValueTraitsArg ValueTraits;
  typedef KeyValuePair<typename KeyTraits::TraitType,
                       typename ValueTraits::TraitType>
      TraitType;
  typedef KeyValuePair<typename KeyTraits::EmptyValueType,
                       typename ValueTraits::EmptyValueType>
      EmptyValueType;

  static constexpr bool kEmptyValueIsZero =
      KeyTraits::kEmptyValueIsZero && ValueTraits::kEmptyValueIsZero;
  static EmptyValueType EmptyValue() {
    return KeyValuePair<typename KeyTraits::EmptyValueType,
                        typename ValueTraits::EmptyValueType>(
        KeyTraits::EmptyValue(), ValueTraits::EmptyValue());
  }

  template <typename U = void>
  struct IsTraceableInCollection {
    static constexpr bool value =
        IsTraceableInCollectionTrait<KeyTraits>::value ||
        IsTraceableInCollectionTrait<ValueTraits>::value;
  };

  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    static constexpr bool value =
        KeyTraits::template NeedsToForbidGCOnMove<>::value ||
        ValueTraits::template NeedsToForbidGCOnMove<>::value;
  };

  static constexpr unsigned kMinimumTableSize = KeyTraits::kMinimumTableSize;

  static void ConstructDeletedValue(TraitType& slot, bool zero_value) {
    KeyTraits::ConstructDeletedValue(slot.key, zero_value);
    // See similar code in this file for why we need to do this.
    if (zero_value) {
      internal::ClearMemoryAtomicallyIfNeeded<
          typename ValueTraits::TraitType>::Clear(&slot.value);
    }
  }
  static bool IsDeletedValue(const TraitType& value) {
    return KeyTraits::IsDeletedValue(value.key);
  }

  // Even non-traceable keys need to have their trait set. This is because
  // non-traceable keys still need to be processed concurrently for checking
  // empty/deleted state.
  static constexpr bool kCanTraceConcurrently =
      KeyTraitsArg::kCanTraceConcurrently &&
      (ValueTraitsArg::kCanTraceConcurrently ||
       !IsTraceable<typename ValueTraitsArg::TraitType>::value);
};

template <typename Key, typename Value>
struct HashTraits<KeyValuePair<Key, Value>>
    : public KeyValuePairHashTraits<HashTraits<Key>, HashTraits<Value>> {};

template <typename T>
struct NullableHashTraits : public HashTraits<T> {
  static constexpr bool kEmptyValueIsZero = false;
  static T EmptyValue() { return reinterpret_cast<T>(1); }
};

}  // namespace WTF

using WTF::EnumHashTraits;
using WTF::HashTraits;
using WTF::IntHashTraits;
using WTF::IntWithZeroKeyHashTraits;
using WTF::NullableHashTraits;
using WTF::PairHashTraits;
using WTF::SimpleClassHashTraits;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TRAITS_H_
