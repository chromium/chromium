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

#include <string.h>  // For memset.
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace WTF {

template <bool isInteger, typename T>
struct GenericHashTraitsBase;
template <bool is_enum, typename T>
struct EnumOrGenericHashTraits;
template <typename T>
struct HashTraits;

template <typename T>
struct GenericHashTraitsBase<false, T> {
  // The emptyValueIsZero flag is used to optimize allocation of empty hash
  // tables with zeroed memory.
  static const bool kEmptyValueIsZero = false;

  // The hasIsEmptyValueFunction flag allows the hash table to automatically
  // generate code to check for the empty value when it can be done with the
  // equality operator, but allows custom functions for cases like String that
  // need them.
  static const bool kHasIsEmptyValueFunction = false;

// The starting table size. Can be overridden when we know beforehand that a
// hash table will have at least N entries.
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // The allocation pool for nodes is one big chunk that ASAN has no insight
  // into, so it can cloak errors. Make it as small as possible to force nodes
  // to be allocated individually where ASAN can see them.
  static const unsigned kMinimumTableSize = 1;
#else
  static const unsigned kMinimumTableSize = 8;
#endif

  // When a hash table backing store is traced, its elements will be
  // traced if their class type has a trace method. However, weak-referenced
  // elements should not be traced then, but handled by the weak processing
  // phase that follows.
  template <typename U = void>
  struct IsTraceableInCollection {
    static const bool value = IsTraceable<T>::value && !IsWeak<T>::value;
  };

  // The NeedsToForbidGCOnMove flag is used to make the hash table move
  // operations safe when GC is enabled: if a move constructor invokes
  // an allocation triggering the GC then it should be invoked within GC
  // forbidden scope.
  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    // TODO(yutak): Consider using of std:::is_trivially_move_constructible
    // when it is accessible.
    static const bool value = !std::is_pod<T>::value;
  };

  static constexpr bool kCanHaveDeletedValue = true;

  // The kHasMovingCallback value is only used for HashTable backing stores.
  // Currently it is needed for LinkedHashSet to register moving callback on
  // write barrier. Users of this value have to provide RegisterMovingCallback
  // function.
  static constexpr bool kHasMovingCallback = false;
};

// Default integer traits disallow both 0 and -1 as keys (max value instead of
// -1 for unsigned).
template <typename T>
struct GenericHashTraitsBase<true, T> : GenericHashTraitsBase<false, T> {
  static const bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(T& slot, bool) {
    slot = static_cast<T>(-1);
  }
  static bool IsDeletedValue(T value) { return value == static_cast<T>(-1); }
};

template <typename T>
struct GenericHashTraits
    : GenericHashTraitsBase<std::is_integral<T>::value, T> {
  typedef T TraitType;
  typedef T EmptyValueType;

  static T EmptyValue() { return T(); }

  // Type for functions that do not take ownership, such as contains.
  typedef const T& PeekInType;
  typedef T* IteratorGetType;
  typedef const T* IteratorConstGetType;
  typedef T& IteratorReferenceType;
  typedef const T& IteratorConstReferenceType;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  template <typename IncomingValueType>
  static void Store(IncomingValueType&& value, T& storage) {
    storage = std::forward<IncomingValueType>(value);
  }

  // Type for return value of functions that do not transfer ownership, such
  // as get.
  // FIXME: We could change this type to const T& for better performance if we
  // figured out a way to handle the return value from emptyValue, which is a
  // temporary.
  typedef T PeekOutType;
  static const T& Peek(const T& value) { return value; }
};

template <typename T>
struct EnumOrGenericHashTraits<false, T> : GenericHashTraits<T> {};

// Default traits for an enum type.  0 is very popular, and -1 is also popular.
// So we use -128 and -127.
template <typename T>
struct EnumOrGenericHashTraits<true, T> : GenericHashTraits<T> {
  static const bool kEmptyValueIsZero = false;
  static T EmptyValue() { return static_cast<T>(-128); }
  static void ConstructDeletedValue(T& slot, bool) {
    slot = static_cast<T>(-127);
  }
  static bool IsDeletedValue(T value) { return value == static_cast<T>(-127); }
};

template <typename T>
struct HashTraits : EnumOrGenericHashTraits<std::is_enum<T>::value, T> {};

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

// Default unsigned traits disallow both 0 and max as keys -- use these traits
// to allow zero and disallow max - 1.
template <typename T>
struct UnsignedWithZeroKeyHashTraits : GenericHashTraits<T> {
  static const bool kEmptyValueIsZero = false;
  static T EmptyValue() { return std::numeric_limits<T>::max(); }
  static void ConstructDeletedValue(T& slot, bool) {
    slot = std::numeric_limits<T>::max() - 1;
  }
  static bool IsDeletedValue(T value) {
    return value == std::numeric_limits<T>::max() - 1;
  }
};

template <typename P>
struct HashTraits<P*> : GenericHashTraits<P*> {
  static const bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(P*& slot, bool) {
    slot = reinterpret_cast<P*>(-1);
  }
  static bool IsDeletedValue(P* value) {
    return value == reinterpret_cast<P*>(-1);
  }
};

template <typename T>
struct SimpleClassHashTraits : GenericHashTraits<T> {
  static const bool kEmptyValueIsZero = true;
  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    static const bool value = false;
  };
  static void ConstructDeletedValue(T& slot, bool) {
    new (NotNull, &slot) T(kHashTableDeletedValue);
  }
  static bool IsDeletedValue(const T& value) {
    return value.IsHashTableDeletedValue();
  }
};

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

  static const bool kHasIsEmptyValueFunction = true;
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
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

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

  static const bool kHasIsEmptyValueFunction = true;
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
    new (NotNull, &slot) std::unique_ptr<T>(reinterpret_cast<T*>(1u));
  }
  static bool IsDeletedValue(const std::unique_ptr<T>& value) {
    return value.get() == reinterpret_cast<T*>(1u);
  }
};

template <>
struct HashTraits<String> : SimpleClassHashTraits<String> {
  static const bool kHasIsEmptyValueFunction = true;
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

  static const bool kEmptyValueIsZero =
      FirstTraits::kEmptyValueIsZero && SecondTraits::kEmptyValueIsZero;
  static EmptyValueType EmptyValue() {
    return std::make_pair(FirstTraits::EmptyValue(),
                          SecondTraits::EmptyValue());
  }

  static const bool kHasIsEmptyValueFunction =
      FirstTraits::kHasIsEmptyValueFunction ||
      SecondTraits::kHasIsEmptyValueFunction;
  static bool IsEmptyValue(const TraitType& value) {
    return IsHashTraitsEmptyValue<FirstTraits>(value.first) &&
           IsHashTraitsEmptyValue<SecondTraits>(value.second);
  }

  static const unsigned kMinimumTableSize = FirstTraits::kMinimumTableSize;

  static void ConstructDeletedValue(TraitType& slot, bool zero_value) {
    FirstTraits::ConstructDeletedValue(slot.first, zero_value);
    // For GC collections the memory for the backing is zeroed when it is
    // allocated, and the constructors may take advantage of that,
    // especially if a GC occurs during insertion of an entry into the
    // table. This slot is being marked deleted, but If the slot is reused
    // at a later point, the same assumptions around memory zeroing must
    // hold as they did at the initial allocation.  Therefore we zero the
    // value part of the slot here for GC collections.
    if (zero_value)
      memset(reinterpret_cast<void*>(&slot.second), 0, sizeof(slot.second));
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

  static const bool kEmptyValueIsZero =
      KeyTraits::kEmptyValueIsZero && ValueTraits::kEmptyValueIsZero;
  static EmptyValueType EmptyValue() {
    return KeyValuePair<typename KeyTraits::EmptyValueType,
                        typename ValueTraits::EmptyValueType>(
        KeyTraits::EmptyValue(), ValueTraits::EmptyValue());
  }

  template <typename U = void>
  struct IsTraceableInCollection {
    static const bool value = IsTraceableInCollectionTrait<KeyTraits>::value ||
                              IsTraceableInCollectionTrait<ValueTraits>::value;
  };

  template <typename U = void>
  struct NeedsToForbidGCOnMove {
    static const bool value =
        KeyTraits::template NeedsToForbidGCOnMove<>::value ||
        ValueTraits::template NeedsToForbidGCOnMove<>::value;
  };

  static const unsigned kMinimumTableSize = KeyTraits::kMinimumTableSize;

  static void ConstructDeletedValue(TraitType& slot, bool zero_value) {
    KeyTraits::ConstructDeletedValue(slot.key, zero_value);
    // See similar code in this file for why we need to do this.
    if (zero_value)
      memset(reinterpret_cast<void*>(&slot.value), 0, sizeof(slot.value));
  }
  static bool IsDeletedValue(const TraitType& value) {
    return KeyTraits::IsDeletedValue(value.key);
  }
};

template <typename Key, typename Value>
struct HashTraits<KeyValuePair<Key, Value>>
    : public KeyValuePairHashTraits<HashTraits<Key>, HashTraits<Value>> {};

template <typename T>
struct NullableHashTraits : public HashTraits<T> {
  static const bool kEmptyValueIsZero = false;
  static T EmptyValue() { return reinterpret_cast<T>(1); }
};

}  // namespace WTF

using WTF::HashTraits;
using WTF::PairHashTraits;
using WTF::NullableHashTraits;
using WTF::SimpleClassHashTraits;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_TRAITS_H_
