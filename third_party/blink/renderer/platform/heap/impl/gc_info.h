// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_GC_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_GC_INFO_H_

#include <atomic>
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/impl/finalizer_traits.h"
#include "third_party/blink/renderer/platform/heap/impl/name_traits.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

template <typename T>
struct TraceTrait;

using GCInfoIndex = uint32_t;

// GCInfo contains metadata for objects that are instantiated from classes that
// inherit from GarbageCollected.
struct PLATFORM_EXPORT GCInfo final {
  static inline const GCInfo& From(GCInfoIndex);

  const TraceCallback trace;
  const internal::FinalizationCallback finalize;
  const NameCallback name;
  const bool has_v_table;
};

class PLATFORM_EXPORT GCInfoTable final {
 public:
  // At maximum |kMaxIndex - 1| indices are supported.
  //
  // We assume that 14 bits is enough to represent all possible types: during
  // telemetry runs, we see about 1,000 different types; looking at the output
  // of the Oilpan GC Clang plugin, there appear to be at most about 6,000
  // types. Thus 14 bits should be more than twice as many bits as we will ever
  // need.
  static constexpr GCInfoIndex kMaxIndex = 1 << 14;

  // Minimum index returned. Values smaller |kMinIndex| may be used as
  // sentinels.
  static constexpr GCInfoIndex kMinIndex = 1;

  // Sets up a singleton table that can be acquired using Get().
  static void CreateGlobalTable();

  static GCInfoTable* GetMutable() { return global_table_; }
  static const GCInfoTable& Get() { return *global_table_; }

  const GCInfo& GCInfoFromIndex(GCInfoIndex index) const {
    DCHECK_GE(index, kMinIndex);
    DCHECK_LT(index, kMaxIndex);
    DCHECK(table_);
    const GCInfo* info = table_[index];
    DCHECK(info);
    return *info;
  }

  GCInfoIndex EnsureGCInfoIndex(const GCInfo*, std::atomic<GCInfoIndex>*);

  // Returns the number of recorded GCInfo objects, including |kMinIndex|.
  GCInfoIndex NumberOfGCInfos() const { return current_index_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(GCInfoTest, InitialEmpty);
  FRIEND_TEST_ALL_PREFIXES(GCInfoTest, ResizeToMaxIndex);
  FRIEND_TEST_ALL_PREFIXES(GCInfoDeathTest, MoreThanMaxIndexInfos);

  // Singleton for each process. Retrieved through Get().
  static GCInfoTable* global_table_;

  // Use GCInfoTable::Get() for retrieving the global table outside of testing
  // code.
  GCInfoTable();

  void Resize();

  // Holds the per-class GCInfo descriptors; each HeapObjectHeader keeps an
  // index into this table.
  const GCInfo** table_ = nullptr;

  // Current index used when requiring a new GCInfo object.
  GCInfoIndex current_index_ = kMinIndex;

  // The limit (exclusive) of the currently allocated table.
  GCInfoIndex limit_ = 0;

  Mutex table_mutex_;
};

// static
const GCInfo& GCInfo::From(GCInfoIndex index) {
  return GCInfoTable::Get().GCInfoFromIndex(index);
}

template <typename T>
struct GCInfoTrait {
  STATIC_ONLY(GCInfoTrait);

  static GCInfoIndex Index() {
    static_assert(sizeof(T), "T must be fully defined");
    static const GCInfo kGcInfo = {
        TraceTrait<T>::Trace, internal::FinalizerTrait<T>::kCallback,
        NameTrait<T>::GetName, std::is_polymorphic<T>::value};
    // This is more complicated than using threadsafe initialization, but this
    // is instantiated many times (once for every GC type).
    static std::atomic<GCInfoIndex> gc_info_index{0};
    GCInfoIndex index = gc_info_index.load(std::memory_order_acquire);
    if (!index) {
      index = GCInfoTable::GetMutable()->EnsureGCInfoIndex(&kGcInfo,
                                                           &gc_info_index);
    }
    DCHECK_GE(index, GCInfoTable::kMinIndex);
    DCHECK_LT(index, GCInfoTable::kMaxIndex);
    return index;
  }
};

template <typename U>
class GCInfoTrait<const U> : public GCInfoTrait<U> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_GC_INFO_H_
