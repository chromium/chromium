/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_MAP_H_

#include <initializer_list>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"

namespace WTF {

template <typename KeyTraits, typename MappedTraits>
struct HashMapValueTraits;

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename Allocator>
class HashCountedSet;

struct KeyValuePairKeyExtractor {
  STATIC_ONLY(KeyValuePairKeyExtractor);
  template <typename T>
  static const typename T::KeyType& Extract(const T& p) {
    return p.key;
  }
  // Assumes out points to a buffer of size at least sizeof(T::KeyType).
  template <typename T>
  static void ExtractSafe(const T& p, void* out) {
    AtomicReadMemcpy<sizeof(typename T::KeyType), alignof(typename T::KeyType)>(
        out, &p.key);
  }
};

// Note: empty or deleted key values are not allowed, using them may lead to
// undefined behavior. For pointer keys this means that null pointers are not
// allowed; for integer keys 0 or -1 can't be used as a key. This restriction
// can be lifted if you supply custom key traits.
template <typename KeyArg,
          typename MappedArg,
          typename HashArg = DefaultHash<KeyArg>,
          typename KeyTraitsArg = HashTraits<KeyArg>,
          typename MappedTraitsArg = HashTraits<MappedArg>,
          typename Allocator = PartitionAllocator>
class HashMap {
  USE_ALLOCATOR(HashMap, Allocator);
  template <typename T, typename U, typename V, typename W>
  friend class HashCountedSet;

 private:
  typedef KeyTraitsArg KeyTraits;
  typedef MappedTraitsArg MappedTraits;
  typedef HashMapValueTraits<KeyTraits, MappedTraits> ValueTraits;

 public:
  typedef typename KeyTraits::TraitType KeyType;
  typedef const typename KeyTraits::PeekInType& KeyPeekInType;
  typedef typename MappedTraits::TraitType MappedType;
  typedef typename ValueTraits::TraitType ValueType;
  using value_type = ValueType;

 private:
  typedef typename MappedTraits::PeekOutType MappedPeekType;

  typedef HashArg HashFunctions;

  typedef HashTable<KeyType,
                    ValueType,
                    KeyValuePairKeyExtractor,
                    HashFunctions,
                    ValueTraits,
                    KeyTraits,
                    Allocator>
      HashTableType;

  class HashMapKeysProxy;
  class HashMapValuesProxy;

 public:
  HashMap() {
    static_assert(Allocator::kIsGarbageCollected ||
                      !IsPointerToGarbageCollectedType<KeyArg>::value,
                  "Cannot put raw pointers to garbage-collected classes into "
                  "an off-heap HashMap.  Use HeapHashMap<> instead.");
    static_assert(Allocator::kIsGarbageCollected ||
                      !IsPointerToGarbageCollectedType<MappedArg>::value,
                  "Cannot put raw pointers to garbage-collected classes into "
                  "an off-heap HashMap.  Use HeapHashMap<> instead.");
  }

#if DUMP_HASHTABLE_STATS_PER_TABLE
  void DumpStats() { impl_.DumpStats(); }
#endif
  HashMap(const HashMap&) = default;
  HashMap& operator=(const HashMap&) = default;
  HashMap(HashMap&&) = default;
  HashMap& operator=(HashMap&&) = default;

  // For example, HashMap<int, int>({{1, 11}, {2, 22}, {3, 33}}) will give you
  // a HashMap containing a mapping {1 -> 11, 2 -> 22, 3 -> 33}.
  HashMap(std::initializer_list<ValueType> elements);
  HashMap& operator=(std::initializer_list<ValueType> elements);

  typedef HashTableIteratorAdapter<HashTableType, ValueType> iterator;
  typedef HashTableConstIteratorAdapter<HashTableType, ValueType>
      const_iterator;
  typedef typename HashTableType::AddResult AddResult;

  void swap(HashMap& ref) { impl_.swap(ref.impl_); }

  unsigned size() const;
  unsigned Capacity() const;
  void ReserveCapacityForSize(unsigned size) {
    impl_.ReserveCapacityForSize(size);
  }

  bool empty() const;

  // iterators iterate over pairs of keys and values
  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;

  HashMapKeysProxy& Keys() { return static_cast<HashMapKeysProxy&>(*this); }
  const HashMapKeysProxy& Keys() const {
    return static_cast<const HashMapKeysProxy&>(*this);
  }

  HashMapValuesProxy& Values() {
    return static_cast<HashMapValuesProxy&>(*this);
  }
  const HashMapValuesProxy& Values() const {
    return static_cast<const HashMapValuesProxy&>(*this);
  }

  iterator find(KeyPeekInType);
  const_iterator find(KeyPeekInType) const;
  bool Contains(KeyPeekInType) const;
  // Returns a reference to the mapped value. Crashes if no mapped value exists.
  MappedPeekType at(KeyPeekInType) const;

  // Replaces value but not key if key is already present. Return value is a
  // pair of the iterator to the key location, and a boolean that's true if a
  // new value was actually added.
  template <typename IncomingKeyType, typename IncomingMappedType>
  AddResult Set(IncomingKeyType&&, IncomingMappedType&&);

  // Does nothing if key is already present. Return value is a pair of the
  // iterator to the key location, and a boolean that's true if a new value
  // was actually added.
  template <typename IncomingKeyType, typename IncomingMappedType>
  AddResult insert(IncomingKeyType&&, IncomingMappedType&&);

  void erase(KeyPeekInType);
  void erase(iterator);
  void clear();
  template <typename Collection>
  void RemoveAll(const Collection& to_be_removed) {
    WTF::RemoveAll(*this, to_be_removed);
  }

  MappedType Take(KeyPeekInType);  // efficient combination of get with remove

  // An alternate version of find() that finds the object by hashing and
  // comparing with some other type, to avoid the cost of type
  // conversion. HashTranslator must have the following function members:
  //   static unsigned hash(const T&);
  //   static bool equal(const ValueType&, const T&);
  template <typename HashTranslator, typename T>
  iterator Find(const T&);
  template <typename HashTranslator, typename T>
  const_iterator Find(const T&) const;
  template <typename HashTranslator, typename T>
  bool Contains(const T&) const;

  template <typename IncomingKeyType>
  static bool IsValidKey(const IncomingKeyType&);

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(
      VisitorDispatcher visitor) const {
    impl_.Trace(visitor);
  }

 protected:
  ValueType** GetBufferSlot() { return impl_.GetBufferSlot(); }

 private:
  template <typename IncomingKeyType, typename IncomingMappedType>
  AddResult InlineAdd(IncomingKeyType&&, IncomingMappedType&&);

  HashTableType impl_;
};

template <typename KeyArg,
          typename MappedArg,
          typename HashArg,
          typename KeyTraitsArg,
          typename MappedTraitsArg,
          typename Allocator>
class HashMap<KeyArg,
              MappedArg,
              HashArg,
              KeyTraitsArg,
              MappedTraitsArg,
              Allocator>::HashMapKeysProxy : private HashMap<KeyArg,
                                                             MappedArg,
                                                             HashArg,
                                                             KeyTraitsArg,
                                                             MappedTraitsArg,
                                                             Allocator> {
  DISALLOW_NEW();

 public:
  typedef HashMap<KeyArg,
                  MappedArg,
                  HashArg,
                  KeyTraitsArg,
                  MappedTraitsArg,
                  Allocator>
      HashMapType;
  typedef typename HashMapType::iterator::KeysIterator iterator;
  typedef typename HashMapType::const_iterator::KeysIterator const_iterator;

  iterator begin() { return HashMapType::begin().Keys(); }

  iterator end() { return HashMapType::end().Keys(); }

  const_iterator begin() const { return HashMapType::begin().Keys(); }

  const_iterator end() const { return HashMapType::end().Keys(); }

 private:
  friend class HashMap;

  HashMapKeysProxy() = delete;
  HashMapKeysProxy(const HashMapKeysProxy&) = delete;
  HashMapKeysProxy& operator=(const HashMapKeysProxy&) = delete;
  ~HashMapKeysProxy() = delete;
};

template <typename KeyArg,
          typename MappedArg,
          typename HashArg,
          typename KeyTraitsArg,
          typename MappedTraitsArg,
          typename Allocator>
class HashMap<KeyArg,
              MappedArg,
              HashArg,
              KeyTraitsArg,
              MappedTraitsArg,
              Allocator>::HashMapValuesProxy : private HashMap<KeyArg,
                                                               MappedArg,
                                                               HashArg,
                                                               KeyTraitsArg,
                                                               MappedTraitsArg,
                                                               Allocator> {
  DISALLOW_NEW();

 public:
  typedef HashMap<KeyArg,
                  MappedArg,
                  HashArg,
                  KeyTraitsArg,
                  MappedTraitsArg,
                  Allocator>
      HashMapType;
  typedef typename HashMapType::iterator::ValuesIterator iterator;
  typedef typename HashMapType::const_iterator::ValuesIterator const_iterator;

  iterator begin() { return HashMapType::begin().Values(); }

  iterator end() { return HashMapType::end().Values(); }

  const_iterator begin() const { return HashMapType::begin().Values(); }

  const_iterator end() const { return HashMapType::end().Values(); }

 private:
  friend class HashMap;

  HashMapValuesProxy() = delete;
  HashMapValuesProxy(const HashMapValuesProxy&) = delete;
  HashMapValuesProxy& operator=(const HashMapValuesProxy&) = delete;
  ~HashMapValuesProxy() = delete;
};

template <typename KeyTraits, typename MappedTraits>
struct HashMapValueTraits : KeyValuePairHashTraits<KeyTraits, MappedTraits> {
  STATIC_ONLY(HashMapValueTraits);
  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(
      const typename KeyValuePairHashTraits<KeyTraits, MappedTraits>::TraitType&
          value) {
    return IsHashTraitsEmptyValue<KeyTraits>(value.key);
  }
};

template <typename ValueTraits, typename HashFunctions, typename Allocator>
struct HashMapTranslator {
  STATIC_ONLY(HashMapTranslator);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return HashFunctions::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return HashFunctions::Equal(a, b);
  }
  template <typename T, typename U, typename V>
  static void Translate(T& location, U&& key, V&& mapped) {
    location.key = std::forward<U>(key);
    ValueTraits::ValueTraits::Store(std::forward<V>(mapped), location.value);
  }
};

template <typename ValueTraits, typename Translator>
struct HashMapTranslatorAdapter {
  STATIC_ONLY(HashMapTranslatorAdapter);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return Translator::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return Translator::Equal(a, b);
  }
  template <typename T, typename U, typename V>
  static void Translate(T& location, U&& key, V&& mapped, unsigned hash_code) {
    Translator::Translate(location.key, std::forward<U>(key), hash_code);
    ValueTraits::ValueTraits::store(std::forward<V>(mapped), location.value);
  }
};

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
HashMap<T, U, V, W, X, Y>::HashMap(std::initializer_list<ValueType> elements) {
  if (elements.size()) {
    impl_.ReserveCapacityForSize(
        base::checked_cast<wtf_size_t>(elements.size()));
  }
  for (const ValueType& element : elements)
    insert(element.key, element.value);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
auto HashMap<T, U, V, W, X, Y>::operator=(
    std::initializer_list<ValueType> elements) -> HashMap& {
  *this = HashMap(std::move(elements));
  return *this;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline unsigned HashMap<T, U, V, W, X, Y>::size() const {
  return impl_.size();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline unsigned HashMap<T, U, V, W, X, Y>::Capacity() const {
  return impl_.Capacity();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline bool HashMap<T, U, V, W, X, Y>::empty() const {
  return impl_.empty();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::iterator
HashMap<T, U, V, W, X, Y>::begin() {
  return impl_.begin();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::iterator
HashMap<T, U, V, W, X, Y>::end() {
  return impl_.end();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::const_iterator
HashMap<T, U, V, W, X, Y>::begin() const {
  return impl_.begin();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::const_iterator
HashMap<T, U, V, W, X, Y>::end() const {
  return impl_.end();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::iterator
HashMap<T, U, V, W, X, Y>::find(KeyPeekInType key) {
  return impl_.find(key);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline typename HashMap<T, U, V, W, X, Y>::const_iterator
HashMap<T, U, V, W, X, Y>::find(KeyPeekInType key) const {
  return impl_.find(key);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline bool HashMap<T, U, V, W, X, Y>::Contains(KeyPeekInType key) const {
  return impl_.Contains(key);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename HashTranslator, typename TYPE>
inline typename HashMap<T, U, V, W, X, Y>::iterator
HashMap<T, U, V, W, X, Y>::Find(const TYPE& value) {
  return impl_
      .template Find<HashMapTranslatorAdapter<ValueTraits, HashTranslator>>(
          value);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename HashTranslator, typename TYPE>
inline typename HashMap<T, U, V, W, X, Y>::const_iterator
HashMap<T, U, V, W, X, Y>::Find(const TYPE& value) const {
  return impl_
      .template Find<HashMapTranslatorAdapter<ValueTraits, HashTranslator>>(
          value);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename HashTranslator, typename TYPE>
inline bool HashMap<T, U, V, W, X, Y>::Contains(const TYPE& value) const {
  return impl_
      .template Contains<HashMapTranslatorAdapter<ValueTraits, HashTranslator>>(
          value);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Allocator>
template <typename IncomingKeyType, typename IncomingMappedType>
typename HashMap<T, U, V, W, X, Allocator>::AddResult
HashMap<T, U, V, W, X, Allocator>::InlineAdd(IncomingKeyType&& key,
                                             IncomingMappedType&& mapped) {
  return impl_.template insert<
      HashMapTranslator<ValueTraits, HashFunctions, Allocator>>(
      std::forward<IncomingKeyType>(key),
      std::forward<IncomingMappedType>(mapped));
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename IncomingKeyType, typename IncomingMappedType>
typename HashMap<T, U, V, W, X, Y>::AddResult HashMap<T, U, V, W, X, Y>::Set(
    IncomingKeyType&& key,
    IncomingMappedType&& mapped) {
  AddResult result = InlineAdd(std::forward<IncomingKeyType>(key),
                               std::forward<IncomingMappedType>(mapped));
  if (!result.is_new_entry) {
    // The inlineAdd call above found an existing hash table entry; we need
    // to set the mapped value.
    //
    // It's safe to call std::forward again, because |mapped| isn't moved if
    // there's an existing entry.
    MappedTraits::Store(std::forward<IncomingMappedType>(mapped),
                        result.stored_value->value);
  }
  return result;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename IncomingKeyType, typename IncomingMappedType>
typename HashMap<T, U, V, W, X, Y>::AddResult HashMap<T, U, V, W, X, Y>::insert(
    IncomingKeyType&& key,
    IncomingMappedType&& mapped) {
  return InlineAdd(std::forward<IncomingKeyType>(key),
                   std::forward<IncomingMappedType>(mapped));
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
typename HashMap<T, U, V, W, X, Y>::MappedPeekType
HashMap<T, U, V, W, X, Y>::at(KeyPeekInType key) const {
  const ValueType* entry = impl_.Lookup(key);
  CHECK(entry) << "HashMap::at found no value for the given key. See "
                  "https://crbug.com/1058527.";
  return MappedTraits::Peek(entry->value);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline void HashMap<T, U, V, W, X, Y>::erase(iterator it) {
  impl_.erase(it.impl_);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline void HashMap<T, U, V, W, X, Y>::erase(KeyPeekInType key) {
  erase(find(key));
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline void HashMap<T, U, V, W, X, Y>::clear() {
  impl_.clear();
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
auto HashMap<T, U, V, W, X, Y>::Take(KeyPeekInType key) -> MappedType {
  iterator it = find(key);
  if (it == end())
    return MappedTraits::EmptyValue();
  MappedType result = std::move(it->value);
  erase(it);
  return result;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
template <typename IncomingKeyType>
inline bool HashMap<T, U, V, W, X, Y>::IsValidKey(const IncomingKeyType& key) {
  if (KeyTraits::IsDeletedValue(key))
    return false;

  if (HashFunctions::safe_to_compare_to_empty_or_deleted) {
    if (key == KeyTraits::EmptyValue())
      return false;
  } else {
    if (IsHashTraitsEmptyValue<KeyTraits>(key))
      return false;
  }

  return true;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
bool operator==(const HashMap<T, U, V, W, X, Y>& a,
                const HashMap<T, U, V, W, X, Y>& b) {
  if (a.size() != b.size())
    return false;

  typedef typename HashMap<T, U, V, W, X, Y>::const_iterator const_iterator;

  const_iterator a_end = a.end();
  const_iterator b_end = b.end();
  for (const_iterator it = a.begin(); it != a_end; ++it) {
    const_iterator b_pos = b.find(it->key);
    if (b_pos == b_end || it->value != b_pos->value)
      return false;
  }

  return true;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
inline bool operator!=(const HashMap<T, U, V, W, X, Y>& a,
                       const HashMap<T, U, V, W, X, Y>& b) {
  return !(a == b);
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y,
          typename Z>
inline void CopyKeysToVector(const HashMap<T, U, V, W, X, Y>& collection,
                             Z& vector) {
  typedef
      typename HashMap<T, U, V, W, X, Y>::const_iterator::KeysIterator iterator;

  {
    // Disallow GC during resize allocation; see crbugs 568173 and 823612.
    // The element copy doesn't need to be in this scope because garbage
    // collection can only remove elements from collection if its keys are
    // WeakMembers, in which case copying them doesn't perform a heap
    // allocation.
    typename Z::GCForbiddenScope scope;
    vector.resize(collection.size());
  }

  iterator it = collection.begin().Keys();
  iterator end = collection.end().Keys();
  for (unsigned i = 0; it != end; ++it, ++i)
    vector[i] = *it;
}

template <typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y,
          typename Z>
inline void CopyValuesToVector(const HashMap<T, U, V, W, X, Y>& collection,
                               Z& vector) {
  typedef typename HashMap<T, U, V, W, X, Y>::const_iterator::ValuesIterator
      iterator;

  // Disallow GC during resize allocation and copy operations (which may also
  // perform allocations and therefore cause elements of collection to be
  // removed); see crbugs 568173 and 823612.
  typename Z::GCForbiddenScope scope;

  vector.resize(collection.size());

  iterator it = collection.begin().Values();
  iterator end = collection.end().Values();
  for (unsigned i = 0; it != end; ++it, ++i)
    vector[i] = *it;
}

}  // namespace WTF

using WTF::HashMap;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_MAP_H_
