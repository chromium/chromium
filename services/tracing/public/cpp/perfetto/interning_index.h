// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>

#include "base/component_export.h"

namespace tracing {

// Value 0 is an invalid ID.
using InterningID = uint32_t;

struct COMPONENT_EXPORT(TRACING_CPP) InterningIndexEntry {
  InterningID id;

  // Whether the entry was emitted since the last reset of emitted state. If
  // |false|, the sink should (re)emit the entry in the current TracePacket.
  //
  // We don't remove entries on reset of emitted state, so that we can continue
  // to use their original IDs and avoid unnecessarily incrementing the ID
  // counter.
  bool was_emitted;
};

// Index is a template that finds the index (stored in |value|) of a type
// |ValueType| inside a |Tuple|. This is done at compile time since this is
// needed inside a std::get<>().
template <typename ValueType, typename Tuple>
struct Index;

template <typename T, typename... Types>
struct Index<T, std::tuple<T, Types...>> {
  static const size_t value = 0;
};

template <typename T, typename U, typename... Types>
struct Index<T, std::tuple<U, Types...>> {
  static const size_t value = 1 + Index<T, std::tuple<Types...>>::value;
};

// A TypeList holds a list of types, while SizeList holds a list of size_t
// integers.
//
// We can't just use a parameter pack to store these two informations because we
// can only have one parameter pack in a template (it absorbs all the types) and
// must be last. However we want users to be able to specify per type size
// limits to decrease memory usage. One of these lists aren't strictly needed
// (you could just have users enter the numbers (or types) as a parameter pack
// directly), but we use both so that when declaring a InterningIndex to
// declaration is readable and avoids surprising behaviour.
template <typename...>
struct TypeList {};
template <size_t...>
struct SizeList {};

// Interning index that associates interned values with interning IDs. It can
// track entries of different types within the same ID space, e.g. so that both
// copied strings and pointers to static strings can co-exist in the same index.
//
// The index will cache for a given ValueType and Size pair up to |Size| values
// after which it will start replacing them in a FIFO basis. All |Size|s must be
// a power of 2 for performance reasons (this is a compile time error if not).
//
// This definition just declares the type and template. Do not use this this
// directly, instead use the partial specialization below.
template <typename ValueTypes, typename Sizes>
class COMPONENT_EXPORT(TRACING_CPP) InterningIndex;
// Partially specalized so we can have two parameter packs which get nested
// inside their respective lists.
template <typename... ValueTypes, size_t... Sizes>
class InterningIndex<TypeList<ValueTypes...>, SizeList<Sizes...>> {
 public:
  // IndexCache is just a pair of std::arrays which are kept in sync with each
  // other. This has an advantage over a std::array<pairs> since we can load a
  // bunch of keys to search in one cache line without loading values.
  template <size_t N, typename ValueType>
  class IndexCache {
   public:
    IndexCache() {
      // Assert that N is a power of two. This allows the "% N" in Insert() to
      // compile to a bunch of bit shifts which improves performance over an
      // arbitrary modulo division.
      static_assert(
          N && ((N & (N - 1)) == 0),
          "InterningIndex requires that the cache size be a power of 2.");
    }

    void Clear() {
      for (auto& val : keys_) {
        val = ValueType{};
      }
      valid_index_ = 0;
      current_index_ = 0;
    }

    typename std::array<InterningIndexEntry, N>::iterator Find(
        const ValueType& value) {
      // First note that only |valid_index_| worth of entries have been assigned
      // InterningIDs.
      //
      // So we search from the beginning up to |valid_index_|.
      //
      // If valid_index_ <= keys_.size and the element doesn't exist find will
      // return it == keys_.begin() + valid_index_. This works because
      // IndexCache::end() returns values_.begin() + valid_index_. So this will
      // appear to be the IndexCache::end(), which again makes Find() act like
      // std::find should. If valid_index_ == keys_.size() then this will be
      // values_.end() as well.
      //
      // By doing this here and not checking the conditional we can only check
      // it once and saves us a pretty impactful branch (this find is the
      // majority of the time in LookUpOrInsert when profiled.
      auto it = std::find(keys_.begin(), keys_.begin() + valid_index_, value);
      return values_.begin() + (it - keys_.begin());
    }

    typename std::array<InterningIndexEntry, N>::iterator Insert(
        const ValueType& key,
        InterningIndexEntry&& value) {
      size_t new_position = current_index_++ % N;
      keys_[new_position] = key;
      values_[new_position] = std::move(value);
      valid_index_ = std::min(valid_index_ + 1, values_.size());
      return values_.begin() + new_position;
    }

    typename std::array<InterningIndexEntry, N>::iterator begin() {
      return values_.begin();
    }
    typename std::array<InterningIndexEntry, N>::iterator end() {
      return values_.begin() + valid_index_;
    }

   private:
    size_t valid_index_ = 0;
    size_t current_index_ = 0;
    std::array<ValueType, N> keys_{{}};
    std::array<InterningIndexEntry, N> values_{{}};
  };

  // Construct a new index with caches for each of the ValueTypes. The cache
  // size for the n-th ValueType will be limited to n-th element in the Sizes
  // list.
  //
  // For example, to construct an index containing at most 1024 char* pointers
  // and 128 std::string objects:
  //     InterningIndex<TypeList<char*, std::string>, SizeList<1024, 128>>
  //     index;
  InterningIndex() {}

  // GetCache<ValueType>() Returns the cache for |ValueType| as a reference.
  //
  // A c++14 helper function.
  // This is purely syntactic sugar function that is made possible by c++14.
  // Every call to GetCache could be replaced by inlining the std::get below,
  // but this hinders readability due to the template meta programing.
  //
  // auto return type means the compiler will determine the exact type returned,
  // using the trailing decltype(auto) return type (which is a c++14 feature)
  // allows us to never specify the type while the decltype(auto) rules
  // preserves references.
  //
  // The return type here is an std::array<ValueType, Size>. However, figuring
  // out the correct Size for the given ValueType is hard: because each
  // ValueType has a unique Size given at compile time, but when we look up a
  // Cache we only know which ValueType we want to store. So instead, we let the
  // compiler figure out the return type (including the value of Size) based on
  // the call to std;:get<>.
  template <typename ValueType>
  auto GetCache() -> decltype(auto) {
    return std::get<Index<ValueType, std::tuple<ValueTypes...>>::value>(
        entry_caches_);
  }

  // Returns the entry for the given interned |value|, adding it to the index if
  // it didn't exist previously or was evicted from the index. Entries may be
  // evicted if they are accessed infrequently and the index for the respective
  // ValueType is at full capacity.
  //
  // If the returned entry's |was_emitted| flag is false, the caller should
  // (re)emit the entry in the current TracePacket's InternedData message.
  template <typename ValueType>
  InterningIndexEntry LookupOrAdd(const ValueType& value) {
    auto& cache = GetCache<ValueType>();
    auto it = cache.Find(value);
    if (it == cache.end()) {
      it = cache.Insert(value, InterningIndexEntry{next_id_++, false});
    }
    DCHECK(it->id != 0);
    bool was_emitted = it->was_emitted;
    // The caller will (re)emit the entry, so mark it as emitted.
    it->was_emitted = true;
    return InterningIndexEntry{it->id, was_emitted};
  }

  // Marks all entries as "not emitted", so that they will be reemitted when
  // next accessed.
  void ResetEmittedState() { ResetStateHelper<ValueTypes...>(/*clear=*/false); }

  // Removes all entries from the index and restarts from the first valid ID.
  void Clear() {
    ResetStateHelper<ValueTypes...>(/*clear=*/true);
    next_id_ = 1u;
  }

 private:
  // Recursive helper template methods to reset the emitted state for all
  // entries or remove all entries from each of the caches.
  template <typename ValueType>
  void ResetStateHelper(bool clear) {
    return ResetStateForValueType<ValueType>(clear);
  }

  template <typename ValueType1,
            typename ValueType2,
            typename... RemainingValueTypes>
  void ResetStateHelper(bool clear) {
    ResetStateForValueType<ValueType1>(clear);
    ResetStateHelper<ValueType2, RemainingValueTypes...>(clear);
  }

  template <typename ValueType>
  void ResetStateForValueType(bool clear) {
    auto& cache = GetCache<ValueType>();
    if (clear) {
      cache.Clear();
    } else {
      for (auto& entry : cache) {
        entry.was_emitted = false;
      }
    }
  }

  std::tuple<IndexCache<Sizes, ValueTypes>...> entry_caches_;
  InterningID next_id_ = 1u;  // ID 0 indicates an unset field value.
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_INTERNING_INDEX_H_
