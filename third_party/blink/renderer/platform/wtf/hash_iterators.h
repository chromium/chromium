/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_ITERATORS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_ITERATORS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace WTF {

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableConstKeysIterator;
template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableConstValuesIterator;
template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableKeysIterator;
template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableValuesIterator;

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableConstIteratorAdapter<HashTableType,
                                     KeyValuePair<KeyType, MappedType>> {
  STACK_ALLOCATED();

 private:
  typedef KeyValuePair<KeyType, MappedType> ValueType;

 public:
  typedef HashTableConstKeysIterator<HashTableType, KeyType, MappedType>
      KeysIterator;
  typedef HashTableConstValuesIterator<HashTableType, KeyType, MappedType>
      ValuesIterator;

  HashTableConstIteratorAdapter() = default;
  HashTableConstIteratorAdapter(
      const typename HashTableType::const_iterator& impl)
      : impl_(impl) {}

  const ValueType* Get() const { return (const ValueType*)impl_.Get(); }
  const ValueType& operator*() const { return *Get(); }
  const ValueType* operator->() const { return Get(); }

  HashTableConstIteratorAdapter& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableConstIteratorAdapter& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  KeysIterator Keys() { return KeysIterator(*this); }
  ValuesIterator Values() { return ValuesIterator(*this); }

  typename HashTableType::const_iterator impl_;
};

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableIteratorAdapter<HashTableType,
                                KeyValuePair<KeyType, MappedType>> {
  STACK_ALLOCATED();

 private:
  typedef KeyValuePair<KeyType, MappedType> ValueType;

 public:
  typedef HashTableKeysIterator<HashTableType, KeyType, MappedType>
      KeysIterator;
  typedef HashTableValuesIterator<HashTableType, KeyType, MappedType>
      ValuesIterator;

  HashTableIteratorAdapter() = default;
  HashTableIteratorAdapter(const typename HashTableType::iterator& impl)
      : impl_(impl) {}

  ValueType* Get() const { return (ValueType*)impl_.Get(); }
  ValueType& operator*() const { return *Get(); }
  ValueType* operator->() const { return Get(); }

  HashTableIteratorAdapter& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableIteratorAdapter& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  operator HashTableConstIteratorAdapter<HashTableType, ValueType>() {
    typename HashTableType::const_iterator i = impl_;
    return i;
  }

  KeysIterator Keys() { return KeysIterator(*this); }
  ValuesIterator Values() { return ValuesIterator(*this); }

  typename HashTableType::iterator impl_;
};

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableConstKeysIterator {
  STACK_ALLOCATED();

 private:
  typedef HashTableConstIteratorAdapter<HashTableType,
                                        KeyValuePair<KeyType, MappedType>>
      ConstIterator;

 public:
  HashTableConstKeysIterator(const ConstIterator& impl) : impl_(impl) {}

  const KeyType* Get() const { return &(impl_.Get()->key); }
  const KeyType& operator*() const { return *Get(); }
  const KeyType* operator->() const { return Get(); }

  HashTableConstKeysIterator& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableConstKeysIterator& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  ConstIterator impl_;
};

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableConstValuesIterator {
  STACK_ALLOCATED();

 private:
  typedef HashTableConstIteratorAdapter<HashTableType,
                                        KeyValuePair<KeyType, MappedType>>
      ConstIterator;

 public:
  HashTableConstValuesIterator(const ConstIterator& impl) : impl_(impl) {}

  const MappedType* Get() const { return &(impl_.Get()->value); }
  const MappedType& operator*() const { return *Get(); }
  const MappedType* operator->() const { return Get(); }

  HashTableConstValuesIterator& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableConstValuesIterator& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  ConstIterator impl_;
};

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableKeysIterator {
  STACK_ALLOCATED();

 private:
  typedef HashTableIteratorAdapter<HashTableType,
                                   KeyValuePair<KeyType, MappedType>>
      Iterator;
  typedef HashTableConstIteratorAdapter<HashTableType,
                                        KeyValuePair<KeyType, MappedType>>
      ConstIterator;

 public:
  HashTableKeysIterator(const Iterator& impl) : impl_(impl) {}

  KeyType* Get() const { return &(impl_.Get()->key); }
  KeyType& operator*() const { return *Get(); }
  KeyType* operator->() const { return Get(); }

  HashTableKeysIterator& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableKeysIterator& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  operator HashTableConstKeysIterator<HashTableType, KeyType, MappedType>() {
    ConstIterator i = impl_;
    return i;
  }

  Iterator impl_;
};

template <typename HashTableType, typename KeyType, typename MappedType>
struct HashTableValuesIterator {
  STACK_ALLOCATED();

 private:
  typedef HashTableIteratorAdapter<HashTableType,
                                   KeyValuePair<KeyType, MappedType>>
      Iterator;
  typedef HashTableConstIteratorAdapter<HashTableType,
                                        KeyValuePair<KeyType, MappedType>>
      ConstIterator;

 public:
  HashTableValuesIterator(const Iterator& impl) : impl_(impl) {}

  MappedType* Get() const { return &(impl_.Get()->value); }
  MappedType& operator*() const { return *Get(); }
  MappedType* operator->() const { return Get(); }

  HashTableValuesIterator& operator++() {
    ++impl_;
    return *this;
  }
  // postfix ++ intentionally omitted

  HashTableValuesIterator& operator--() {
    --impl_;
    return *this;
  }
  // postfix -- intentionally omitted

  operator HashTableConstValuesIterator<HashTableType, KeyType, MappedType>() {
    ConstIterator i = impl_;
    return i;
  }

  Iterator impl_;
};

template <typename T, typename U, typename V>
inline bool operator==(const HashTableConstKeysIterator<T, U, V>& a,
                       const HashTableConstKeysIterator<T, U, V>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator!=(const HashTableConstKeysIterator<T, U, V>& a,
                       const HashTableConstKeysIterator<T, U, V>& b) {
  return a.impl_ != b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator==(const HashTableConstValuesIterator<T, U, V>& a,
                       const HashTableConstValuesIterator<T, U, V>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator!=(const HashTableConstValuesIterator<T, U, V>& a,
                       const HashTableConstValuesIterator<T, U, V>& b) {
  return a.impl_ != b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator==(const HashTableKeysIterator<T, U, V>& a,
                       const HashTableKeysIterator<T, U, V>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator!=(const HashTableKeysIterator<T, U, V>& a,
                       const HashTableKeysIterator<T, U, V>& b) {
  return a.impl_ != b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator==(const HashTableValuesIterator<T, U, V>& a,
                       const HashTableValuesIterator<T, U, V>& b) {
  return a.impl_ == b.impl_;
}

template <typename T, typename U, typename V>
inline bool operator!=(const HashTableValuesIterator<T, U, V>& a,
                       const HashTableValuesIterator<T, U, V>& b) {
  return a.impl_ != b.impl_;
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_ITERATORS_H_
