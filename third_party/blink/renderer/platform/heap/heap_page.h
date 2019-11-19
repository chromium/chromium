/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_PAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_PAGE_H_

#include <stdint.h>
#include <atomic>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_statistics.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// TODO(palmer): Document the reason for 17.
constexpr size_t kBlinkPageSizeLog2 = 17;
constexpr size_t kBlinkPageSize = 1 << kBlinkPageSizeLog2;
constexpr size_t kBlinkPageOffsetMask = kBlinkPageSize - 1;
constexpr size_t kBlinkPageBaseMask = ~kBlinkPageOffsetMask;

// We allocate pages at random addresses but in groups of kBlinkPagesPerRegion
// at a given random address. We group pages to not spread out too much over the
// address space which would blow away the page tables and lead to bad
// performance.
constexpr size_t kBlinkPagesPerRegion = 10;

// TODO(nya): Replace this with something like #if ENABLE_NACL.
#if defined(ARCH_CPU_PPC64)
// NaCl's system page size is 64 KiB. This causes a problem in Oilpan's heap
// layout because Oilpan allocates two guard pages for each Blink page (whose
// size is kBlinkPageSize = 2^17 = 128 KiB). So we don't use guard pages in
// NaCl.
// The same issue holds for ppc64 systems, which use a 64k page size.
constexpr size_t kBlinkGuardPageSize = 0;
#else
constexpr size_t kBlinkGuardPageSize = base::kSystemPageSize;
#endif

// Double precision floats are more efficient when 8-byte aligned, so we 8-byte
// align all allocations (even on 32 bit systems).
static_assert(8 == sizeof(double), "We expect sizeof(double) to be 8");
constexpr size_t kAllocationGranularity = sizeof(double);
constexpr size_t kAllocationMask = kAllocationGranularity - 1;
constexpr size_t kMaxHeapObjectSizeLog2 = 27;
constexpr size_t kMaxHeapObjectSize = 1 << kMaxHeapObjectSizeLog2;
constexpr size_t kLargeObjectSizeThreshold = kBlinkPageSize / 2;

// A zap value used for freed memory that is allowed to be added to the free
// list in the next call to AddToFreeList.
constexpr uint8_t kReuseAllowedZapValue = 0x2a;
// A zap value used for freed memory that is forbidden to be added to the free
// list in the next call to AddToFreeList.
constexpr uint8_t kReuseForbiddenZapValue = 0x2c;

// In non-production builds, memory is zapped when it's freed. The zapped memory
// is zeroed out when the memory is reused in ThreadHeap::AllocateObject.
//
// In production builds, memory is not zapped (for performance). The memory is
// just zeroed out when it is added to the free list.
#if defined(MEMORY_SANITIZER)
// TODO(kojii): We actually need __msan_poison/unpoison here, but it'll be
// added later.
#define SET_MEMORY_INACCESSIBLE(address, size) \
  FreeList::ZapFreedMemory(address, size);
#define SET_MEMORY_ACCESSIBLE(address, size) memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size)     \
  ASAN_UNPOISON_MEMORY_REGION(address, size);        \
  FreeList::CheckFreedMemoryIsZapped(address, size); \
  ASAN_POISON_MEMORY_REGION(address, size)
#elif DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define SET_MEMORY_INACCESSIBLE(address, size) \
  FreeList::ZapFreedMemory(address, size);     \
  ASAN_POISON_MEMORY_REGION(address, size)
#define SET_MEMORY_ACCESSIBLE(address, size)  \
  ASAN_UNPOISON_MEMORY_REGION(address, size); \
  memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size)     \
  ASAN_UNPOISON_MEMORY_REGION(address, size);        \
  FreeList::CheckFreedMemoryIsZapped(address, size); \
  ASAN_POISON_MEMORY_REGION(address, size)
#else
#define SET_MEMORY_INACCESSIBLE(address, size) memset((address), 0, (size))
#define SET_MEMORY_ACCESSIBLE(address, size) \
  do {                                       \
  } while (false)
#define CHECK_MEMORY_INACCESSIBLE(address, size) \
  do {                                           \
  } while (false)
#endif

class NormalPageArena;
class PageMemory;
class BaseArena;

// HeapObjectHeader is a 32-bit object that has the following layout:
//
// | padding (32 bits)            | Only present on 64-bit platforms.
// | gc_info_index (14 bits)      |
// | unused (1 bit)               |
// | in construction (1 bit)      | true: bit not set; false bit set
//
// | size (14 bits)               | Actually 17 bits because sizes are aligned.
// | unused (1 bit)               |
// | mark bit (1 bit)             |
//
// Notes:
// - 14 bits for |gc_info_index} (type information) are enough as there are
//   fewer than 2^14 types allocated in Blink.
// - |size| for regular objects is encoded with 14 bits but can actually
//   represent sizes up to |kBlinkPageSize| (2^17) because allocations are
//   always 8 byte aligned (see kAllocationGranularity).
// - |size| for large objects is encoded as 0. The size of a large object is
//   stored in |LargeObjectPage::PayloadSize()|.
// - |mark bit| and |in construction| bits are located in separate variables and
//   therefore can be accessed concurrently. Since tsan works with word-size
//   objects they still should be accessed atomically.
constexpr uint16_t kHeaderMarkBitMask = 1;
constexpr uint16_t kHeaderSizeShift = 2;
constexpr uint16_t kHeaderSizeMask =
    static_cast<uint16_t>(((1 << 14) - 1) << kHeaderSizeShift);

constexpr uint16_t kHeaderIsInConstructionMask = 1;
constexpr uint16_t kHeaderGCInfoIndexShift = 2;
constexpr uint16_t kHeaderGCInfoSize = static_cast<uint16_t>(1 << 14);
constexpr uint16_t kHeaderGCInfoIndexMask =
    static_cast<uint16_t>((kHeaderGCInfoSize - 1) << kHeaderGCInfoIndexShift);

constexpr uint16_t kLargeObjectSizeInHeader = 0;
constexpr uint16_t kGcInfoIndexForFreeListHeader = 0;
constexpr size_t kNonLargeObjectPageSizeMax = 1 << kBlinkPageSizeLog2;

static_assert(kHeaderGCInfoSize == GCInfoTable::kMaxIndex,
              "GCInfoTable size and and header GCInfo index size must match");

static_assert(
    kNonLargeObjectPageSizeMax >= kBlinkPageSize,
    "max size supported by HeapObjectHeader must at least be kBlinkPageSize");

namespace internal {

// This is needed due to asan complaining deep from std::atomic<>::load/store
// stacktraces.
class AsanUnpoisonScope {
 public:
  AsanUnpoisonScope(const void* addr, size_t size)
      : addr_(addr), size_(size), was_poisoned_(false) {
    if (!ASAN_REGION_IS_POISONED(const_cast<void*>(addr_), size_))
      return;
    ASAN_UNPOISON_MEMORY_REGION(addr_, size_);
    was_poisoned_ = true;
  }
  ~AsanUnpoisonScope() {
    if (was_poisoned_)
      ASAN_POISON_MEMORY_REGION(addr_, size_);
  }

 private:
  const void* addr_;
  size_t size_;
  bool was_poisoned_;
};

NO_SANITIZE_ADDRESS constexpr uint16_t EncodeSize(size_t size) {
  // Essentially, gets optimized to >> 1.
  return static_cast<uint16_t>((size << kHeaderSizeShift) /
                               kAllocationGranularity);
}

NO_SANITIZE_ADDRESS constexpr size_t DecodeSize(uint16_t encoded) {
  // Essentially, gets optimized to << 1.
  return ((encoded & kHeaderSizeMask) >> kHeaderSizeShift) *
         kAllocationGranularity;
}

}  // namespace internal

class PLATFORM_EXPORT HeapObjectHeader {
  DISALLOW_NEW();

 public:
  enum HeaderLocation : uint8_t { kNormalPage, kLargePage };
  enum class AccessMode : uint8_t { kNonAtomic, kAtomic };

  static HeapObjectHeader* FromPayload(const void*);
  template <AccessMode = AccessMode::kNonAtomic>
  static HeapObjectHeader* FromInnerAddress(const void*);

  // Checks sanity of the header given a payload pointer.
  static void CheckFromPayload(const void*);

  // If |gc_info_index| is 0, this header is interpreted as a free list header.
  HeapObjectHeader(size_t, size_t, HeaderLocation);

  template <AccessMode mode = AccessMode::kNonAtomic>
  NO_SANITIZE_ADDRESS bool IsFree() const {
    return GcInfoIndex<mode>() == kGcInfoIndexForFreeListHeader;
  }

  template <AccessMode mode = AccessMode::kNonAtomic>
  NO_SANITIZE_ADDRESS uint32_t GcInfoIndex() const {
    const uint16_t encoded = LoadEncoded<mode, EncodedHalf::kHigh>();
    return (encoded & kHeaderGCInfoIndexMask) >> kHeaderGCInfoIndexShift;
  }

  template <AccessMode = AccessMode::kNonAtomic>
  size_t size() const;
  void SetSize(size_t size);

  bool IsLargeObject() const;

  template <AccessMode = AccessMode::kNonAtomic>
  bool IsMarked() const;
  template <AccessMode = AccessMode::kNonAtomic>
  void Mark();
  template <AccessMode = AccessMode::kNonAtomic>
  void Unmark();
  template <AccessMode = AccessMode::kNonAtomic>
  bool TryMark();

  template <AccessMode = AccessMode::kNonAtomic>
  bool IsInConstruction() const;
  template <AccessMode = AccessMode::kNonAtomic>
  void MarkFullyConstructed();

  // The payload starts directly after the HeapObjectHeader, and the payload
  // size does not include the sizeof(HeapObjectHeader).
  Address Payload() const;
  size_t PayloadSize() const;
  template <AccessMode = AccessMode::kNonAtomic>
  Address PayloadEnd() const;

  void Finalize(Address, size_t);

  // Returns true if object has finalizer.
  bool HasNonTrivialFinalizer() const;

  // Returns a human-readable name of this object.
  const char* Name() const;

 private:
  enum class EncodedHalf : uint8_t { kLow, kHigh };

  template <AccessMode, EncodedHalf>
  uint16_t LoadEncoded() const;
  template <AccessMode mode, EncodedHalf>
  void StoreEncoded(uint16_t bits, uint16_t mask);

#if defined(ARCH_CPU_64_BITS)
  uint32_t padding_ = 0;
#endif  // defined(ARCH_CPU_64_BITS)
  uint16_t encoded_high_;
  uint16_t encoded_low_;
};

class FreeListEntry final : public HeapObjectHeader {
 public:
  NO_SANITIZE_ADDRESS
  explicit FreeListEntry(size_t size)
      : HeapObjectHeader(size,
                         kGcInfoIndexForFreeListHeader,
                         HeapObjectHeader::kNormalPage),
        next_(nullptr) {
  }

  Address GetAddress() { return reinterpret_cast<Address>(this); }

  NO_SANITIZE_ADDRESS
  void Unlink(FreeListEntry** previous_next) {
    *previous_next = next_;
    next_ = nullptr;
  }

  NO_SANITIZE_ADDRESS
  void Link(FreeListEntry** previous_next) {
    next_ = *previous_next;
    *previous_next = this;
  }

  NO_SANITIZE_ADDRESS
  FreeListEntry* Next() const { return next_; }

  NO_SANITIZE_ADDRESS
  void Append(FreeListEntry* next) {
    DCHECK(!next_);
    next_ = next;
  }

 private:
  FreeListEntry* next_;

  friend class FreeList;
};

class FreeList {
  DISALLOW_NEW();

 public:
  // Returns a bucket number for inserting a |FreeListEntry| of a given size.
  // All entries in the given bucket, n, have size >= 2^n.
  static int BucketIndexForSize(size_t);

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  static void GetAllowedAndForbiddenCounts(Address, size_t, size_t&, size_t&);
  static void ZapFreedMemory(Address, size_t);
  static void CheckFreedMemoryIsZapped(Address, size_t);
#endif

  FreeList();

  FreeListEntry* Allocate(size_t);
  void Add(Address, size_t);
  void MoveFrom(FreeList*);
  void Clear();

  bool IsEmpty() const;
  size_t FreeListSize() const;

  void CollectStatistics(ThreadState::Statistics::FreeListStatistics*);

  template <typename Predicate>
  FreeListEntry* FindEntry(Predicate pred) {
    for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
      for (FreeListEntry* entry = free_list_heads_[i]; entry;
           entry = entry->Next()) {
        if (pred(entry)) {
          return entry;
        }
      }
    }
    return nullptr;
  }

 private:
  bool IsConsistent(size_t index) const {
    return (!free_list_heads_[index] && !free_list_tails_[index]) ||
           (free_list_heads_[index] && free_list_tails_[index] &&
            !free_list_tails_[index]->Next());
  }

  // All |FreeListEntry|s in the nth list have size >= 2^n.
  FreeListEntry* free_list_heads_[kBlinkPageSizeLog2];
  FreeListEntry* free_list_tails_[kBlinkPageSizeLog2];
  int biggest_free_list_index_;
};

// Blink heap pages are set up with a guard page before and after the payload.
inline size_t BlinkPagePayloadSize() {
  return kBlinkPageSize - 2 * kBlinkGuardPageSize;
}

// Blink heap pages are aligned to the Blink heap page size. Therefore, the
// start of a Blink page can be obtained by rounding down to the Blink page
// size.
inline Address RoundToBlinkPageStart(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) &
                                   kBlinkPageBaseMask);
}

inline Address RoundToBlinkPageEnd(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address - 1) &
                                   kBlinkPageBaseMask) +
         kBlinkPageSize;
}

// Masks an address down to the enclosing Blink page base address.
inline Address BlinkPageAddress(Address address) {
  return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) &
                                   kBlinkPageBaseMask);
}

inline bool VTableInitialized(void* object_pointer) {
  return !!(*reinterpret_cast<Address*>(object_pointer));
}

#if DCHECK_IS_ON()

// Sanity check for a page header address: the address of the page header should
// be 1 OS page size away from being Blink page size-aligned.
inline bool IsPageHeaderAddress(Address address) {
  return !((reinterpret_cast<uintptr_t>(address) & kBlinkPageOffsetMask) -
           kBlinkGuardPageSize);
}

#endif

// |FinalizeType| specifies when finalization should take place.
// In case of concurrent sweeper we defer finalization to be done
// on the main thread.
enum class FinalizeType : uint8_t { kInlined, kDeferred };

// |SweepResult| indicates if page turned out to be empty after sweeping.
enum class SweepResult : uint8_t { kPageEmpty, kPageNotEmpty };

// |PageType| indicates whether a page is used for normal objects or whether it
// holds a large object.
enum class PageType : uint8_t { kNormalPage, kLargeObjectPage };

// |BasePage| is a base class for |NormalPage| and |LargeObjectPage|.
//
// - |NormalPage| is a page whose size is |kBlinkPageSize|. A |NormalPage| can
//   contain multiple objects. An object whose size is smaller than
//   |kLargeObjectSizeThreshold| is stored in a |NormalPage|.
//
// - |LargeObjectPage| is a page that contains only one object. The object size
//   is arbitrary. An object whose size is larger than |kBlinkPageSize| is
//   stored as a single project in |LargeObjectPage|.
//
// Note: An object whose size is between |kLargeObjectSizeThreshold| and
// |kBlinkPageSize| can go to either of |NormalPage| or |LargeObjectPage|.
class BasePage {
  DISALLOW_NEW();

 public:
  BasePage(PageMemory*, BaseArena*, PageType);
  virtual ~BasePage() = default;

  // Virtual methods are slow. So performance-sensitive methods should be
  // defined as non-virtual methods on |NormalPage| and |LargeObjectPage|. The
  // following methods are not performance-sensitive.
  virtual size_t ObjectPayloadSizeForTesting() = 0;
  virtual void RemoveFromHeap() = 0;
  // Sweeps a page. Returns true when that page is empty and false otherwise.
  // Does not create free list entries for empty pages.
  virtual bool Sweep(FinalizeType) = 0;
  virtual void MakeConsistentForMutator() = 0;

  // Calls finalizers after sweeping is done.
  virtual void FinalizeSweep(SweepResult) = 0;

#if defined(ADDRESS_SANITIZER)
  virtual void PoisonUnmarkedObjects() = 0;
#endif

  virtual void CollectStatistics(
      ThreadState::Statistics::ArenaStatistics* arena_stats) = 0;

#if DCHECK_IS_ON()
  virtual bool Contains(Address) = 0;
#endif
  virtual size_t size() = 0;

  Address GetAddress() { return reinterpret_cast<Address>(this); }
  PageMemory* Storage() const { return storage_; }
  BaseArena* Arena() const { return arena_; }
  ThreadState* thread_state() const { return thread_state_; }

  // Returns true if this page has been swept by the ongoing sweep; false
  // otherwise.
  bool HasBeenSwept() const { return swept_; }

  void MarkAsSwept() {
    DCHECK(!swept_);
    swept_ = true;
  }

  void MarkAsUnswept() {
    DCHECK(swept_);
    swept_ = false;
  }

  // Returns true  if this page is a large object page; false otherwise.
  bool IsLargeObjectPage() const {
    return page_type_ == PageType::kLargeObjectPage;
  }

  virtual void VerifyMarking() = 0;

 private:
  PageMemory* const storage_;
  BaseArena* const arena_;
  ThreadState* const thread_state_;

  // Track the sweeping state of a page. Set to false at the start of a sweep,
  // true upon completion of sweeping that page.
  bool swept_ = true;

  PageType page_type_;

  friend class BaseArena;
};

class PageStack : Vector<BasePage*> {
  using Base = Vector<BasePage*>;

 public:
  PageStack() = default;

  void Push(BasePage* page) { push_back(page); }

  BasePage* Pop() {
    if (IsEmpty())
      return nullptr;
    BasePage* top = back();
    pop_back();
    return top;
  }

  BasePage* Top() const {
    if (IsEmpty())
      return nullptr;
    return back();
  }

  using Base::begin;
  using Base::clear;
  using Base::end;
  using Base::IsEmpty;
  using Base::size;
};

class PageStackThreadSafe : public PageStack {
 public:
  void PushLocked(BasePage* page) {
    WTF::MutexLocker locker(mutex_);
    Push(page);
  }

  BasePage* PopLocked() {
    WTF::MutexLocker locker(mutex_);
    return Pop();
  }

  bool IsEmptyLocked() const {
    WTF::MutexLocker locker(mutex_);
    return IsEmpty();
  }

  // Explicit unsafe move assignment.
  void MoveFrom(PageStack&& other) { PageStack::operator=(std::move(other)); }

 private:
  mutable WTF::Mutex mutex_;
};

// A bitmap for recording object starts. Objects have to be allocated at
// minimum granularity of kGranularity.
//
// Depends on internals such as:
// - kBlinkPageSize
// - kAllocationGranularity
class PLATFORM_EXPORT ObjectStartBitmap {
  DISALLOW_NEW();

 public:
  // Granularity of addresses added to the bitmap.
  static constexpr size_t Granularity() { return kAllocationGranularity; }

  // Maximum number of entries in the bitmap.
  static constexpr size_t MaxEntries() {
    return kReservedForBitmap * kCellSize;
  }

  explicit ObjectStartBitmap(Address offset);

  // Finds an object header based on a
  // address_maybe_pointing_to_the_middle_of_object. Will search for an object
  // start in decreasing address order.
  Address FindHeader(Address address_maybe_pointing_to_the_middle_of_object);

  inline void SetBit(Address);
  inline void ClearBit(Address);
  inline bool CheckBit(Address) const;

  // Iterates all object starts recorded in the bitmap.
  //
  // The callback is of type
  //   void(Address)
  // and is passed the object start address as parameter.
  template <typename Callback>
  inline void Iterate(Callback) const;

  // Clear the object start bitmap.
  void Clear();

 private:
  static const size_t kCellSize = sizeof(uint8_t) * 8;
  static const size_t kCellMask = sizeof(uint8_t) * 8 - 1;
  static const size_t kBitmapSize =
      (kBlinkPageSize + ((kCellSize * kAllocationGranularity) - 1)) /
      (kCellSize * kAllocationGranularity);
  static const size_t kReservedForBitmap =
      ((kBitmapSize + kAllocationMask) & ~kAllocationMask);

  inline void ObjectStartIndexAndBit(Address, size_t*, size_t*) const;

  const Address offset_;
  // The bitmap contains a bit for every kGranularity aligned address on a
  // a NormalPage, i.e., for a page of size kBlinkPageSize.
  uint8_t object_start_bit_map_[kReservedForBitmap];
};

class PLATFORM_EXPORT NormalPage final : public BasePage {
 public:
  NormalPage(PageMemory*, BaseArena*);
  ~NormalPage() override;

  Address Payload() { return GetAddress() + PageHeaderSize(); }
  size_t PayloadSize() {
    return (BlinkPagePayloadSize() - PageHeaderSize()) & ~kAllocationMask;
  }
  Address PayloadEnd() { return Payload() + PayloadSize(); }
  bool ContainedInObjectPayload(Address address) {
    return Payload() <= address && address < PayloadEnd();
  }

  size_t ObjectPayloadSizeForTesting() override;
  void RemoveFromHeap() override;
  bool Sweep(FinalizeType) override;
  void MakeConsistentForMutator() override;
  void FinalizeSweep(SweepResult) override;
#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects() override;
#endif

  void CollectStatistics(
      ThreadState::Statistics::ArenaStatistics* arena_stats) override;

#if DCHECK_IS_ON()
  // Returns true for the whole |kBlinkPageSize| page that the page is on, even
  // for the header, and the unmapped guard page at the start. That ensures the
  // result can be used to populate the negative page cache.
  bool Contains(Address) override;
#endif
  size_t size() override { return kBlinkPageSize; }
  static size_t PageHeaderSize() {
    // Compute the amount of padding we have to add to a header to make the size
    // of the header plus the padding a multiple of 8 bytes.
    size_t padding_size =
        (sizeof(NormalPage) + kAllocationGranularity -
         (sizeof(HeapObjectHeader) % kAllocationGranularity)) %
        kAllocationGranularity;
    return sizeof(NormalPage) + padding_size;
  }

  inline NormalPageArena* ArenaForNormalPage() const;

  // Context object holding the state of the arena page compaction pass, passed
  // in when compacting individual pages.
  class CompactionContext {
    STACK_ALLOCATED();

   public:
    // Page compacting into.
    NormalPage* current_page_ = nullptr;
    // Offset into |current_page_| to the next free address.
    size_t allocation_point_ = 0;
    // Vector of available pages to use for compaction. Page compaction picks
    // the next one when the current one is exhausted.
    PageStack available_pages_;
    // Vector of pages that have been compacted. Page compaction will add
    // compacted pages once the current one becomes exhausted.
    PageStack* compacted_pages_;
  };

  void SweepAndCompact(CompactionContext&);

  // Object start bitmap of this page.
  ObjectStartBitmap* object_start_bit_map() { return &object_start_bit_map_; }

  // Verifies that the object start bitmap only contains a bit iff the object
  // is also reachable through iteration on the page.
  void VerifyObjectStartBitmapIsConsistentWithPayload();

  // Uses the object_start_bit_map_ to find an object for a given address. The
  // returned header is either nullptr, indicating that no object could be
  // found, or it is pointing to valid object or free list entry.
  HeapObjectHeader* ConservativelyFindHeaderFromAddress(Address);

  // Uses the object_start_bit_map_ to find an object for a given address. It is
  // assumed that the address points into a valid heap object. Use the
  // conservative version if that assumption does not hold.
  template <
      HeapObjectHeader::AccessMode = HeapObjectHeader::AccessMode::kNonAtomic>
  HeapObjectHeader* FindHeaderFromAddress(Address);

  void VerifyMarking() override;

 private:
  struct ToBeFinalizedObject {
    HeapObjectHeader* header;
    void Finalize();
  };
  struct FutureFreelistEntry {
    Address start;
    size_t size;
  };

  void MergeFreeLists();
  void AddToFreeList(Address start,
                     size_t size,
                     FinalizeType finalize_type,
                     bool found_finalizer);

  ObjectStartBitmap object_start_bit_map_;
  Vector<ToBeFinalizedObject> to_be_finalized_objects_;
  FreeList cached_freelist_;
  Vector<FutureFreelistEntry> unfinalized_freelist_;
};

// Large allocations are allocated as separate objects and linked in a list.
//
// In order to use the same memory allocation routines for everything allocated
// in the heap, large objects are considered heap pages containing only one
// object.
class PLATFORM_EXPORT LargeObjectPage final : public BasePage {
 public:
  static size_t PageHeaderSize() {
    // Compute the amount of padding we have to add to a header to make the size
    // of the header plus the padding a multiple of 8 bytes.
    size_t padding_size =
        (sizeof(LargeObjectPage) + kAllocationGranularity -
         (sizeof(HeapObjectHeader) % kAllocationGranularity)) %
        kAllocationGranularity;
    return sizeof(LargeObjectPage) + padding_size;
  }

  LargeObjectPage(PageMemory*, BaseArena*, size_t);

  // LargeObjectPage has the following memory layout:
  //   this          -> +------------------+
  //                    | Header           | PageHeaderSize()
  //   ObjectHeader() -> +------------------+
  //                    | HeapObjectHeader | sizeof(HeapObjectHeader)
  //   Payload()     -> +------------------+
  //                    | Object payload   | PayloadSize()
  //                    |                  |
  //   PayloadEnd()  -> +------------------+
  //
  //   ObjectSize(): PayloadSize() + sizeof(HeapObjectHeader)
  //   size():       ObjectSize() + PageHeaderSize()

  HeapObjectHeader* ObjectHeader() {
    Address header_address = GetAddress() + PageHeaderSize();
    return reinterpret_cast<HeapObjectHeader*>(header_address);
  }

  // Returns the size of the page that is allocatable for objects. This differs
  // from PayloadSize() as it also includes the HeapObjectHeader.
  size_t ObjectSize() const { return object_size_; }

  // Returns the size of the page including the header.
  size_t size() override { return PageHeaderSize() + object_size_; }

  // Returns the payload start of the underlying object.
  Address Payload() { return ObjectHeader()->Payload(); }

  // Returns the payload size of the underlying object.
  size_t PayloadSize() { return object_size_ - sizeof(HeapObjectHeader); }

  // Points to the payload end of the underlying object.
  Address PayloadEnd() { return Payload() + PayloadSize(); }

  bool ContainedInObjectPayload(Address address) {
    return Payload() <= address && address < PayloadEnd();
  }

  size_t ObjectPayloadSizeForTesting() override;
  void RemoveFromHeap() override;
  bool Sweep(FinalizeType) override;
  void MakeConsistentForMutator() override;
  void FinalizeSweep(SweepResult) override;

  void CollectStatistics(
      ThreadState::Statistics::ArenaStatistics* arena_stats) override;

  void VerifyMarking() override;

#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects() override;
#endif

#if DCHECK_IS_ON()
  // Returns true for any address that is on one of the pages that this large
  // object uses. That ensures that we can use a negative result to populate the
  // negative page cache.
  bool Contains(Address) override;
#endif

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  void SetIsVectorBackingPage() { is_vector_backing_page_ = true; }
  bool IsVectorBackingPage() const { return is_vector_backing_page_; }
#endif

 private:
  // The size of the underlying object including HeapObjectHeader.
  size_t object_size_;
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  bool is_vector_backing_page_;
#endif
};

// Each thread has a number of thread arenas (e.g., Generic arenas, typed arenas
// for |Node|, arenas for collection backings, etc.) and |BaseArena| represents
// each thread arena.
//
// |BaseArena| is a parent class of |NormalPageArena| and |LargeObjectArena|.
// |NormalPageArena| represents a part of a heap that contains |NormalPage|s,
// and |LargeObjectArena| represents a part of a heap that contains
// |LargeObjectPage|s.
class PLATFORM_EXPORT BaseArena {
  USING_FAST_MALLOC(BaseArena);

 public:
  BaseArena(ThreadState*, int);
  virtual ~BaseArena();
  void RemoveAllPages();

  void CollectStatistics(std::string, ThreadState::Statistics*);
  virtual void CollectFreeListStatistics(
      ThreadState::Statistics::FreeListStatistics*) {}

#if DCHECK_IS_ON()
  BasePage* FindPageFromAddress(Address);
#endif
  virtual void ClearFreeLists() {}
  virtual void MakeIterable() {}
  virtual void MakeConsistentForGC();
  void MakeConsistentForMutator();
#if DCHECK_IS_ON()
  virtual bool IsConsistentForGC() = 0;
#endif
  size_t ObjectPayloadSizeForTesting();
  void PrepareForSweep();
#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects();
#endif
  Address LazySweep(size_t, size_t gc_info_index);
  bool SweepUnsweptPage(BasePage*);
  bool SweepUnsweptPageOnConcurrentThread(BasePage*);
  // Returns true if we have swept all pages within the deadline. Returns false
  // otherwise.
  bool LazySweepWithDeadline(base::TimeTicks deadline);
  bool ConcurrentSweepWithDeadline(base::TimeTicks deadline);
  void CompleteSweep();
  void InvokeFinalizersOnSweptPages();

  ThreadState* GetThreadState() { return thread_state_; }
  int ArenaIndex() const { return index_; }

  Address AllocateLargeObject(size_t allocation_size, size_t gc_info_index);

  // Resets the allocation point if it exists for an arena.
  virtual void ResetAllocationPoint() {}

  void VerifyMarking();
  virtual void VerifyObjectStartBitmap() {}

 protected:
  bool SweepingCompleted() const { return unswept_pages_.IsEmptyLocked(); }
  bool SweepingAndFinalizationCompleted() const {
    return unswept_pages_.IsEmptyLocked() &&
           swept_unfinalized_pages_.IsEmptyLocked() &&
           swept_unfinalized_empty_pages_.IsEmptyLocked();
  }

  // Pages for allocation.
  PageStackThreadSafe swept_pages_;
  // Pages that are being swept.
  PageStackThreadSafe unswept_pages_;
  // Pages that have been swept but contain unfinalized objects.
  PageStackThreadSafe swept_unfinalized_pages_;
  // Pages that have been swept and need to be removed from the heap.
  PageStackThreadSafe swept_unfinalized_empty_pages_;

 private:
  virtual Address LazySweepPages(size_t, size_t gc_info_index) = 0;

  ThreadState* thread_state_;

  // Index into the page pools. This is used to ensure that the pages of the
  // same type go into the correct page pool and thus avoid type confusion.
  //
  // TODO(palmer): Should this be size_t?
  int index_;
};

class PLATFORM_EXPORT NormalPageArena final : public BaseArena {
 public:
  NormalPageArena(ThreadState*, int index);
  void AddToFreeList(Address address, size_t size) {
#if DCHECK_IS_ON()
    DCHECK(FindPageFromAddress(address));
    // TODO(palmer): Do we need to handle about integer overflow here (and in
    // similar expressions elsewhere)?
    DCHECK(FindPageFromAddress(address + size - 1));
#endif
    free_list_.Add(address, size);
  }
  void AddToFreeList(FreeList* other) { free_list_.MoveFrom(other); }
  void ClearFreeLists() override;
  void CollectFreeListStatistics(
      ThreadState::Statistics::FreeListStatistics*) override;
  void MakeIterable() override;

#if DCHECK_IS_ON()
  bool IsConsistentForGC() override;
  bool PagesToBeSweptContains(Address);
#endif

  Address AllocateObject(size_t allocation_size, size_t gc_info_index);

  void FreePage(NormalPage*);

  void PromptlyFreeObject(HeapObjectHeader*);
  void PromptlyFreeObjectInFreeList(HeapObjectHeader*, size_t);
  bool ExpandObject(HeapObjectHeader*, size_t);
  bool ShrinkObject(HeapObjectHeader*, size_t);
  size_t promptly_freed_size() const { return promptly_freed_size_; }

  bool IsObjectAllocatedAtAllocationPoint(HeapObjectHeader* header) {
    return header->PayloadEnd() == current_allocation_point_;
  }

  size_t ArenaSize();
  size_t FreeListSize();

  void SweepAndCompact();

  void ResetAllocationPoint() override { SetAllocationPoint(nullptr, 0); }

  void VerifyObjectStartBitmap() override;

  Address CurrentAllocationPoint() const { return current_allocation_point_; }

  bool IsInCurrentAllocationPointRegion(Address address) const {
    return HasCurrentAllocationArea() &&
           (CurrentAllocationPoint() <= address) &&
           (address < (CurrentAllocationPoint() + RemainingAllocationSize()));
  }

  size_t RemainingAllocationSize() const { return remaining_allocation_size_; }

  void MakeConsistentForGC() override;

 private:
  void AllocatePage();

  // OutOfLineAllocate represent the slow-path allocation. The suffixed version
  // contains just allocation code while the other version also invokes a
  // safepoint where allocated bytes are reported to observers.
  Address OutOfLineAllocate(size_t allocation_size, size_t gc_info_index);
  Address OutOfLineAllocateImpl(size_t allocation_size, size_t gc_info_index);

  Address AllocateFromFreeList(size_t, size_t gc_info_index);

  Address LazySweepPages(size_t, size_t gc_info_index) override;

  bool HasCurrentAllocationArea() const {
    return CurrentAllocationPoint() && RemainingAllocationSize();
  }
  void SetAllocationPoint(Address, size_t);

  // Only use when adjusting the area from allocation and free and not when
  // returning it to free list.
  void SetRemainingAllocationSize(size_t);

  FreeList free_list_;
  Address current_allocation_point_;
  size_t remaining_allocation_size_;
  size_t last_remaining_allocation_size_;

  // The size of promptly freed objects in the heap. This counter is set to
  // zero before sweeping when clearing the free list and after coalescing.
  // It will increase for promptly freed objects on already swept pages.
  size_t promptly_freed_size_;
};

class LargeObjectArena final : public BaseArena {
 public:
  LargeObjectArena(ThreadState*, int index);
  Address AllocateLargeObjectPage(size_t, size_t gc_info_index);
  void FreeLargeObjectPage(LargeObjectPage*);

#if DCHECK_IS_ON()
  bool IsConsistentForGC() override { return true; }
#endif

 private:
  Address DoAllocateLargeObjectPage(size_t, size_t gc_info_index);
  Address LazySweepPages(size_t, size_t gc_info_index) override;
};

// Mask an address down to the enclosing Oilpan heap base page. All Oilpan heap
// pages are aligned at |kBlinkPageBase| plus the size of a guard page. This
// will work only for 1) a pointer pointing to a non-large object and 2) a
// pointer pointing to the beginning of a large object.
//
// FIXME: Remove PLATFORM_EXPORT once we get a proper public interface to our
// typed arenas. This is only exported to enable tests in HeapTest.cpp.
PLATFORM_EXPORT ALWAYS_INLINE BasePage* PageFromObject(const void* object) {
  Address address = reinterpret_cast<Address>(const_cast<void*>(object));
  BasePage* page = reinterpret_cast<BasePage*>(BlinkPageAddress(address) +
                                               kBlinkGuardPageSize);
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  return page;
}

inline HeapObjectHeader* HeapObjectHeader::FromPayload(const void* payload) {
  Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
  HeapObjectHeader* header =
      reinterpret_cast<HeapObjectHeader*>(addr - sizeof(HeapObjectHeader));
  return header;
}

template <HeapObjectHeader::AccessMode mode>
inline HeapObjectHeader* HeapObjectHeader::FromInnerAddress(
    const void* address) {
  BasePage* const page = PageFromObject(address);
  return page->IsLargeObjectPage()
             ? static_cast<LargeObjectPage*>(page)->ObjectHeader()
             : static_cast<NormalPage*>(page)->FindHeaderFromAddress<mode>(
                   reinterpret_cast<Address>(const_cast<void*>(address)));
}

inline void HeapObjectHeader::CheckFromPayload(const void* payload) {
  (void)FromPayload(payload);
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::size() const {
  uint16_t encoded_low_value;
  if (mode == AccessMode::kNonAtomic) {
    encoded_low_value = encoded_low_;
  } else {
    // mode == AccessMode::kAtomic
    // Relaxed load as size is immutable after construction while either
    // marking or sweeping is running
    internal::AsanUnpoisonScope unpoison_scope(
        static_cast<const void*>(&encoded_low_), sizeof(encoded_low_));
    encoded_low_value =
        reinterpret_cast<const std::atomic<uint16_t>&>(encoded_low_)
            .load(std::memory_order_relaxed);
  }
  const size_t result = internal::DecodeSize(encoded_low_value);
  // Large objects should not refer to header->size() but use
  // LargeObjectPage::PayloadSize().
  DCHECK(result != kLargeObjectSizeInHeader);
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return result;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::SetSize(size_t size) {
  DCHECK(!PageFromObject(Payload())->thread_state()->IsIncrementalMarking());
  DCHECK_LT(size, kNonLargeObjectPageSizeMax);
  encoded_low_ = static_cast<uint16_t>(internal::EncodeSize(size) |
                                       (encoded_low_ & ~kHeaderSizeMask));
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsLargeObject() const {
  return internal::DecodeSize(encoded_low_) == kLargeObjectSizeInHeader;
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsInConstruction() const {
  return (LoadEncoded<mode, EncodedHalf::kHigh>() &
          kHeaderIsInConstructionMask) == 0;
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline void HeapObjectHeader::MarkFullyConstructed() {
  DCHECK(IsInConstruction());
  StoreEncoded<mode, EncodedHalf::kHigh>(kHeaderIsInConstructionMask,
                                         kHeaderIsInConstructionMask);
}

inline Address HeapObjectHeader::Payload() const {
  return reinterpret_cast<Address>(const_cast<HeapObjectHeader*>(this)) +
         sizeof(HeapObjectHeader);
}

template <HeapObjectHeader::AccessMode mode>
inline Address HeapObjectHeader::PayloadEnd() const {
  return reinterpret_cast<Address>(const_cast<HeapObjectHeader*>(this)) +
         size<mode>();
}

NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::PayloadSize() const {
  const size_t size = internal::DecodeSize(encoded_low_);
  if (UNLIKELY(size == kLargeObjectSizeInHeader)) {
    DCHECK(PageFromObject(this)->IsLargeObjectPage());
    return static_cast<LargeObjectPage*>(PageFromObject(this))->PayloadSize();
  }
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return size - sizeof(HeapObjectHeader);
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsMarked() const {
  const uint16_t encoded = LoadEncoded<mode, EncodedHalf::kLow>();
  return encoded & kHeaderMarkBitMask;
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Mark() {
  DCHECK(!IsMarked<mode>());
  StoreEncoded<mode, EncodedHalf::kLow>(kHeaderMarkBitMask, kHeaderMarkBitMask);
}

template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Unmark() {
  DCHECK(IsMarked<mode>());
  StoreEncoded<mode, EncodedHalf::kLow>(0u, kHeaderMarkBitMask);
}

// The function relies on size bits being unmodified when the function is
// called, i.e. SetSize() and TryMark() can't be called concurrently.
template <HeapObjectHeader::AccessMode mode>
NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::TryMark() {
  if (mode == AccessMode::kNonAtomic) {
    if (encoded_low_ & kHeaderMarkBitMask)
      return false;
    encoded_low_ |= kHeaderMarkBitMask;
    return true;
  }
  internal::AsanUnpoisonScope unpoison_scope(
      static_cast<const void*>(&encoded_low_), sizeof(encoded_low_));
  auto* atomic_encoded =
      reinterpret_cast<std::atomic<uint16_t>*>(&encoded_low_);
  uint16_t old_value = atomic_encoded->load(std::memory_order_relaxed);
  if (old_value & kHeaderMarkBitMask)
    return false;
  const uint16_t new_value = old_value | kHeaderMarkBitMask;
  return atomic_encoded->compare_exchange_strong(old_value, new_value,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed);
}

inline Address NormalPageArena::AllocateObject(size_t allocation_size,
                                               size_t gc_info_index) {
  if (LIKELY(allocation_size <= remaining_allocation_size_)) {
    Address header_address = current_allocation_point_;
    current_allocation_point_ += allocation_size;
    remaining_allocation_size_ -= allocation_size;
    DCHECK_GT(gc_info_index, 0u);
    new (NotNull, header_address) HeapObjectHeader(
        allocation_size, gc_info_index, HeapObjectHeader::kNormalPage);
    Address result = header_address + sizeof(HeapObjectHeader);
    DCHECK(!(reinterpret_cast<uintptr_t>(result) & kAllocationMask));

    SET_MEMORY_ACCESSIBLE(result, allocation_size - sizeof(HeapObjectHeader));
#if DCHECK_IS_ON()
    DCHECK(FindPageFromAddress(header_address + allocation_size - 1));
#endif
    return result;
  }
  return OutOfLineAllocate(allocation_size, gc_info_index);
}

inline NormalPageArena* NormalPage::ArenaForNormalPage() const {
  return static_cast<NormalPageArena*>(Arena());
}

inline void ObjectStartBitmap::SetBit(Address header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  object_start_bit_map_[cell_index] |= (1 << object_bit);
}

inline void ObjectStartBitmap::ClearBit(Address header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  object_start_bit_map_[cell_index] &= ~(1 << object_bit);
}

inline bool ObjectStartBitmap::CheckBit(Address header_address) const {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  return object_start_bit_map_[cell_index] & (1 << object_bit);
}

inline void ObjectStartBitmap::ObjectStartIndexAndBit(Address header_address,
                                                      size_t* cell_index,
                                                      size_t* bit) const {
  const size_t object_offset = header_address - offset_;
  DCHECK(!(object_offset & kAllocationMask));
  const size_t object_start_number = object_offset / kAllocationGranularity;
  *cell_index = object_start_number / kCellSize;
#if DCHECK_IS_ON()
  const size_t bitmap_size = kBitmapSize;
  DCHECK_LT(*cell_index, bitmap_size);
#endif
  *bit = object_start_number & kCellMask;
}

template <typename Callback>
inline void ObjectStartBitmap::Iterate(Callback callback) const {
  for (size_t cell_index = 0; cell_index < kReservedForBitmap; cell_index++) {
    if (!object_start_bit_map_[cell_index])
      continue;

    uint8_t value = object_start_bit_map_[cell_index];
    while (value) {
      const int trailing_zeroes = base::bits::CountTrailingZeroBits(value);
      const size_t object_start_number =
          (cell_index * kCellSize) + trailing_zeroes;
      const Address object_address =
          offset_ + (kAllocationGranularity * object_start_number);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= ~(1 << (object_start_number & kCellMask));
    }
  }
}

NO_SANITIZE_ADDRESS inline HeapObjectHeader::HeapObjectHeader(
    size_t size,
    size_t gc_info_index,
    HeaderLocation header_location) {
  // sizeof(HeapObjectHeader) must be equal to or smaller than
  // |kAllocationGranularity|, because |HeapObjectHeader| is used as a header
  // for a freed entry. Given that the smallest entry size is
  // |kAllocationGranurarity|, |HeapObjectHeader| must fit into the size.
  static_assert(
      sizeof(HeapObjectHeader) <= kAllocationGranularity,
      "size of HeapObjectHeader must be smaller than kAllocationGranularity");

  DCHECK_LT(gc_info_index, GCInfoTable::kMaxIndex);
  DCHECK_LT(size, kNonLargeObjectPageSizeMax);
  DCHECK_EQ(0u, size & kAllocationMask);
  encoded_high_ =
      static_cast<uint16_t>(gc_info_index << kHeaderGCInfoIndexShift);
  encoded_low_ = internal::EncodeSize(size);
  if (header_location == kNormalPage) {
    DCHECK(!PageFromObject(this)->IsLargeObjectPage());
    static_cast<NormalPage*>(PageFromObject(this))
        ->object_start_bit_map()
        ->SetBit(reinterpret_cast<Address>(this));
  } else {
    DCHECK(PageFromObject(this)->IsLargeObjectPage());
  }
  DCHECK(IsInConstruction());
}

template <HeapObjectHeader::AccessMode mode, HeapObjectHeader::EncodedHalf part>
NO_SANITIZE_ADDRESS inline uint16_t HeapObjectHeader::LoadEncoded() const {
  const uint16_t& half =
      (part == EncodedHalf::kLow ? encoded_low_ : encoded_high_);
  internal::AsanUnpoisonScope unpoison_scope(static_cast<const void*>(&half),
                                             sizeof(half));
  if (mode == AccessMode::kNonAtomic)
    return half;
  return reinterpret_cast<const std::atomic<uint16_t>&>(half).load(
      std::memory_order_acquire);
}

// Sets bits selected by the mask to the given value. Please note that atomicity
// of the whole operation is not guaranteed.
template <HeapObjectHeader::AccessMode mode, HeapObjectHeader::EncodedHalf part>
NO_SANITIZE_ADDRESS inline void HeapObjectHeader::StoreEncoded(uint16_t bits,
                                                               uint16_t mask) {
  DCHECK_EQ(static_cast<uint16_t>(0u), bits & ~mask);
  uint16_t* half = (part == EncodedHalf::kLow ? &encoded_low_ : &encoded_high_);
  internal::AsanUnpoisonScope unpoison_scope(static_cast<void*>(half),
                                             sizeof(&half));
  if (mode == AccessMode::kNonAtomic) {
    *half = (*half & ~mask) | bits;
    return;
  }
  // We don't perform CAS loop here assuming that the data is constant and no
  // one except for us can change this half concurrently.
  auto* atomic_encoded = reinterpret_cast<std::atomic<uint16_t>*>(half);
  uint16_t value = atomic_encoded->load(std::memory_order_relaxed);
  value = (value & ~mask) | bits;
  atomic_encoded->store(value, std::memory_order_release);
}

template <HeapObjectHeader::AccessMode mode>
HeapObjectHeader* NormalPage::FindHeaderFromAddress(Address address) {
  DCHECK(ContainedInObjectPayload(address));
  DCHECK(!ArenaForNormalPage()->IsInCurrentAllocationPointRegion(address));
  HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(
      object_start_bit_map()->FindHeader(address));
  DCHECK_LT(0u, header->GcInfoIndex());
  DCHECK_GT(header->PayloadEnd<mode>(), address);
  return header;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_PAGE_H_
