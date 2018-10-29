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
#include "base/bits.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/address_sanitizer.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace base {
namespace trace_event {
class MemoryAllocatorDump;
}  // namespace trace_event
}  // namespace base

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
#if 0
// NaCl's system page size is 64 KiB. This causes a problem in Oilpan's heap
// layout because Oilpan allocates two guard pages for each Blink page (whose
// size is kBlinkPageSize = 2^17 = 128 KiB). So we don't use guard pages in
// NaCl.
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

// Returns a random value.
//
// The implementation gets its randomness from the locations of 2 independent
// sources of address space layout randomization: a function in a Chrome
// executable image, and a function in an external DLL/so. This implementation
// should be fast and small, and should have the benefit of requiring
// attackers to discover and use 2 independent weak infoleak bugs, or 1
// arbitrary infoleak bug (used twice).
uint32_t ComputeRandomMagic();

// HeapObjectHeader is a 64-bit (64-bit platforms) or 32-bit (32-bit platforms)
// object that has the following layout:
//
// | random magic value (32 bits) | <- present on 64-bit platforms only
// | gc_info_index (14 bits)      |
// | DOM mark bit (1 bit)         |
// | size (14 bits)               |
// | dead bit (1 bit)             |
// | freed bit (1 bit)            |
// | mark bit (1 bit)             |
//
// - For non-large objects, 14 bits are enough for |size| because the Blink page
//   size is 2^kBlinkPageSizeLog2 (kBlinkPageSizeLog2 = 17) bytes, and each
//   object is guaranteed to be aligned on a kAllocationGranularity-byte
//   boundary.
// - For large objects, |size| is 0. The actual size of a large object is
//   stored in |LargeObjectPage::PayloadSize()|.
// - 1 bit used to mark DOM trees for V8.
// - 14 bits are enough for |gc_info_index| because there are fewer than 2^14
//   types in Blink.
constexpr size_t kHeaderWrapperMarkBitMask = 1u << kBlinkPageSizeLog2;
constexpr size_t kHeaderGCInfoIndexShift = kBlinkPageSizeLog2 + 1;
constexpr size_t kHeaderGCInfoIndexMask = (static_cast<size_t>((1 << 14) - 1))
                                          << kHeaderGCInfoIndexShift;
constexpr size_t kHeaderSizeMask = (static_cast<size_t>((1 << 14) - 1)) << 3;
constexpr size_t kHeaderMarkBitMask = 1;
constexpr size_t kHeaderFreedBitMask = 2;
constexpr size_t kLargeObjectSizeInHeader = 0;
constexpr size_t kGcInfoIndexForFreeListHeader = 0;
constexpr size_t kNonLargeObjectPageSizeMax = 1 << kBlinkPageSizeLog2;

static_assert(
    kNonLargeObjectPageSizeMax >= kBlinkPageSize,
    "max size supported by HeapObjectHeader must at least be kBlinkPageSize");

class PLATFORM_EXPORT HeapObjectHeader {
  DISALLOW_NEW();

 public:
  enum HeaderLocation { kNormalPage, kLargePage };

  // If |gc_info_index| is 0, this header is interpreted as a free list header.
  NO_SANITIZE_ADDRESS
  inline HeapObjectHeader(size_t, size_t, HeaderLocation);

  NO_SANITIZE_ADDRESS bool IsFree() const {
    return encoded_ & kHeaderFreedBitMask;
  }

  size_t size() const;

  NO_SANITIZE_ADDRESS uint32_t GcInfoIndex() const {
    return (encoded_ & kHeaderGCInfoIndexMask) >> kHeaderGCInfoIndexShift;
  }

  NO_SANITIZE_ADDRESS void SetSize(size_t size) {
    DCHECK_LT(size, kNonLargeObjectPageSizeMax);
    CheckHeader();
    encoded_ = static_cast<uint32_t>(size) | (encoded_ & ~kHeaderSizeMask);
  }

  bool IsWrapperHeaderMarked() const;
  void MarkWrapperHeader();
  void UnmarkWrapperHeader();
  bool IsMarked() const;
  void Mark();
  void Unmark();
  bool TryMark();

  // The payload starts directly after the HeapObjectHeader, and the payload
  // size does not include the sizeof(HeapObjectHeader).
  Address Payload();
  size_t PayloadSize();
  Address PayloadEnd();

  void Finalize(Address, size_t);
  static HeapObjectHeader* FromPayload(const void*);

  // Some callers formerly called |FromPayload| only for its side-effect of
  // calling |CheckHeader| (which is now private). This function does that, but
  // its explanatory name makes the intention at the call sites easier to
  // understand, and is public.
  static void CheckFromPayload(const void*);

  // Returns true if magic number is valid.
  bool IsValid() const;
  // Returns true if magic number is valid or zapped.
  bool IsValidOrZapped() const;

  // The following values are used when zapping free list entries.
  // Regular zapping value.
  static const uint32_t kZappedMagic = 0xDEAD4321;
  // On debug and sanitizer builds the zap values differ, indicating when free
  // list entires are allowed to be reused.
  static const uint32_t kZappedMagicAllowed = 0x2a2a2a2a;
  static const uint32_t kZappedMagicForbidden = 0x2c2c2c2c;

 protected:
#if DCHECK_IS_ON() && defined(ARCH_CPU_64_BITS)
  // Zap |m_magic| with a new magic number that means there was once an object
  // allocated here, but it was freed because nobody marked it during GC.
  void ZapMagic();
#endif

 private:
  void CheckHeader() const;

#if defined(ARCH_CPU_64_BITS)
  // Returns a random magic value.
  static uint32_t GetMagic();
  uint32_t magic_;
#endif  // defined(ARCH_CPU_64_BITS)

  uint32_t encoded_;
};

class FreeListEntry final : public HeapObjectHeader {
 public:
  NO_SANITIZE_ADDRESS
  explicit FreeListEntry(size_t size)
      : HeapObjectHeader(size,
                         kGcInfoIndexForFreeListHeader,
                         HeapObjectHeader::kNormalPage),
        next_(nullptr) {
#if DCHECK_IS_ON() && defined(ARCH_CPU_64_BITS)
    DCHECK_GE(size, sizeof(HeapObjectHeader));
    ZapMagic();
#endif
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
  BasePage(PageMemory*, BaseArena*);
  virtual ~BasePage() = default;

  void Link(BasePage** previous_next) {
    next_ = *previous_next;
    *previous_next = this;
  }
  void Unlink(BasePage** previous_next) {
    *previous_next = next_;
    next_ = nullptr;
  }
  BasePage* Next() const { return next_; }

  // Virtual methods are slow. So performance-sensitive methods should be
  // defined as non-virtual methods on |NormalPage| and |LargeObjectPage|. The
  // following methods are not performance-sensitive.
  virtual size_t ObjectPayloadSizeForTesting() = 0;
  virtual void RemoveFromHeap() = 0;
  // Sweeps a page. Returns true when that page is empty and false otherwise.
  // Does not create free list entries for empty pages.
  virtual bool Sweep() = 0;
  virtual void MakeConsistentForMutator() = 0;

#if defined(ADDRESS_SANITIZER)
  virtual void PoisonUnmarkedObjects() = 0;
#endif

  class HeapSnapshotInfo {
    STACK_ALLOCATED();

   public:
    size_t free_count = 0;
    size_t free_size = 0;
  };

  virtual void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                            ThreadState::GCSnapshotInfo&,
                            HeapSnapshotInfo&) = 0;
#if DCHECK_IS_ON()
  virtual bool Contains(Address) = 0;
#endif
  virtual size_t size() = 0;
  virtual bool IsLargeObjectPage() { return false; }

  Address GetAddress() { return reinterpret_cast<Address>(this); }
  PageMemory* Storage() const { return storage_; }
  BaseArena* Arena() const { return arena_; }

  // Returns true if this page has been swept by the ongoing lazy sweep.
  bool HasBeenSwept() const { return swept_; }

  void MarkAsSwept() {
    DCHECK(!swept_);
    swept_ = true;
  }

  void MarkAsUnswept() {
    DCHECK(swept_);
    swept_ = false;
  }

  // Returns true if magic number is valid.
  bool IsValid() const;

  virtual void VerifyMarking() = 0;

 private:
  // Returns a random magic value.
  PLATFORM_EXPORT static uint32_t GetMagic();

  uint32_t const magic_;
  PageMemory* const storage_;
  BaseArena* const arena_;
  BasePage* next_;

  // Track the sweeping state of a page. Set to false at the start of a sweep,
  // true  upon completion of lazy sweeping.
  bool swept_;

  friend class BaseArena;
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
  bool Sweep() override;
  void MakeConsistentForMutator() override;
#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects() override;
#endif

  void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                    ThreadState::GCSnapshotInfo&,
                    HeapSnapshotInfo&) override;
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
    // Chain of available pages to use for compaction. Page compaction picks the
    // next one when the current one is exhausted.
    BasePage* available_pages_ = nullptr;
    // Chain of pages that have been compacted. Page compaction will add
    // compacted pages once the current one becomes exhausted.
    BasePage** compacted_pages_ = nullptr;
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
  HeapObjectHeader* FindHeaderFromAddress(Address);

  void VerifyMarking() override;

 private:
  ObjectStartBitmap object_start_bit_map_;
};

// Large allocations are allocated as separate objects and linked in a list.
//
// In order to use the same memory allocation routines for everything allocated
// in the heap, large objects are considered heap pages containing only one
// object.
class LargeObjectPage final : public BasePage {
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
  bool Sweep() override;
  void MakeConsistentForMutator() override;

  void TakeSnapshot(base::trace_event::MemoryAllocatorDump*,
                    ThreadState::GCSnapshotInfo&,
                    HeapSnapshotInfo&) override;

  bool IsLargeObjectPage() override { return true; }

  void VerifyMarking() override {}

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

class FreeList {
  DISALLOW_NEW();

 public:
  FreeList();

  void AddToFreeList(Address, size_t);
  void Clear();

  // Returns a bucket number for inserting a |FreeListEntry| of a given size.
  // All entries in the given bucket, n, have size >= 2^n.
  static int BucketIndexForSize(size_t);

  // Returns true if the freelist snapshot is captured.
  bool TakeSnapshot(const String& dump_base_name);

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  static void GetAllowedAndForbiddenCounts(Address, size_t, size_t&, size_t&);
  static void ZapFreedMemory(Address, size_t);
  static void CheckFreedMemoryIsZapped(Address, size_t);
#endif

 private:
  int biggest_free_list_index_;

  // All |FreeListEntry|s in the nth list have size >= 2^n.
  FreeListEntry* free_lists_[kBlinkPageSizeLog2];

  size_t FreeListSize() const;

  friend class NormalPageArena;
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

  void TakeSnapshot(const String& dump_base_name, ThreadState::GCSnapshotInfo&);
#if DCHECK_IS_ON()
  BasePage* FindPageFromAddress(Address);
#endif
  virtual void TakeFreelistSnapshot(const String& dump_base_name) {}
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
  void PoisonArena();
#endif
  Address LazySweep(size_t, size_t gc_info_index);
  void SweepUnsweptPage();
  // Returns true if we have swept all pages within the deadline. Returns false
  // otherwise.
  bool LazySweepWithDeadline(TimeTicks deadline);
  void CompleteSweep();

  ThreadState* GetThreadState() { return thread_state_; }
  int ArenaIndex() const { return index_; }

  Address AllocateLargeObject(size_t allocation_size, size_t gc_info_index);

  bool WillObjectBeLazilySwept(BasePage*, void*) const;

  virtual void VerifyObjectStartBitmap(){};
  virtual void VerifyMarking(){};

 protected:
  bool SweepingCompleted() const { return !first_unswept_page_; }

  BasePage* first_page_;
  BasePage* first_unswept_page_;

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
    free_list_.AddToFreeList(address, size);
  }
  void ClearFreeLists() override;
  void MakeIterable() override;

#if DCHECK_IS_ON()
  bool IsConsistentForGC() override;
  bool PagesToBeSweptContains(Address);
#endif
  void TakeFreelistSnapshot(const String& dump_base_name) override;

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

  bool IsLazySweeping() const { return is_lazy_sweeping_; }
  void SetIsLazySweeping(bool sweeping) { is_lazy_sweeping_ = sweeping; }

  size_t ArenaSize();
  size_t FreeListSize();

  void SweepAndCompact();

  void VerifyObjectStartBitmap() override;
  void VerifyMarking() override;

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

  Address OutOfLineAllocate(size_t allocation_size, size_t gc_info_index);
  Address AllocateFromFreeList(size_t, size_t gc_info_index);

  Address LazySweepPages(size_t, size_t gc_info_index) override;

  bool HasCurrentAllocationArea() const {
    return CurrentAllocationPoint() && RemainingAllocationSize();
  }
  void SetAllocationPoint(Address, size_t);

  void SetRemainingAllocationSize(size_t);
  void UpdateRemainingAllocationSize();

  FreeList free_list_;
  Address current_allocation_point_;
  size_t remaining_allocation_size_;
  size_t last_remaining_allocation_size_;

  // The size of promptly freed objects in the heap. This counter is set to
  // zero before sweeping when clearing the free list and after coalescing.
  // It will increase for promptly freed objects on already swept pages.
  size_t promptly_freed_size_;

  bool is_lazy_sweeping_;
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
  // Page must have a valid magic.
  DCHECK(page->IsValid());
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  return page;
}

NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::size() const {
  size_t result = encoded_ & kHeaderSizeMask;
  // Large objects should not refer to header->size() but use
  // LargeObjectPage::PayloadSize().
  DCHECK(result != kLargeObjectSizeInHeader);
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return result;
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsValid() const {
#if defined(ARCH_CPU_64_BITS)
  return GetMagic() == magic_;
#else
  return true;
#endif
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsValidOrZapped() const {
#if defined(ARCH_CPU_64_BITS)
  return IsValid() || kZappedMagic == magic_ || kZappedMagicAllowed == magic_ ||
         kZappedMagicForbidden == magic_;
#else
  return true;
#endif
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::CheckHeader() const {
#if defined(ARCH_CPU_64_BITS)
  CHECK(IsValid());
#endif
}

inline Address HeapObjectHeader::Payload() {
  return reinterpret_cast<Address>(this) + sizeof(HeapObjectHeader);
}

inline Address HeapObjectHeader::PayloadEnd() {
  return reinterpret_cast<Address>(this) + size();
}

NO_SANITIZE_ADDRESS inline size_t HeapObjectHeader::PayloadSize() {
  CheckHeader();
  size_t size = encoded_ & kHeaderSizeMask;
  if (UNLIKELY(size == kLargeObjectSizeInHeader)) {
    DCHECK(PageFromObject(this)->IsLargeObjectPage());
    return static_cast<LargeObjectPage*>(PageFromObject(this))->PayloadSize();
  }
  DCHECK(!PageFromObject(this)->IsLargeObjectPage());
  return size - sizeof(HeapObjectHeader);
}

inline HeapObjectHeader* HeapObjectHeader::FromPayload(const void* payload) {
  Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
  HeapObjectHeader* header =
      reinterpret_cast<HeapObjectHeader*>(addr - sizeof(HeapObjectHeader));
  header->CheckHeader();
  return header;
}

inline void HeapObjectHeader::CheckFromPayload(const void* payload) {
  (void)FromPayload(payload);
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsWrapperHeaderMarked()
    const {
  CheckHeader();
  return encoded_ & kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::MarkWrapperHeader() {
  CheckHeader();
  DCHECK(!IsWrapperHeaderMarked());
  encoded_ |= kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::UnmarkWrapperHeader() {
  CheckHeader();
  DCHECK(IsWrapperHeaderMarked());
  encoded_ &= ~kHeaderWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::IsMarked() const {
  CheckHeader();
  return encoded_ & kHeaderMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Mark() {
  CheckHeader();
  DCHECK(!IsMarked());
  encoded_ = encoded_ | kHeaderMarkBitMask;
}

NO_SANITIZE_ADDRESS inline void HeapObjectHeader::Unmark() {
  CheckHeader();
  DCHECK(IsMarked());
  encoded_ &= ~kHeaderMarkBitMask;
}

NO_SANITIZE_ADDRESS inline bool HeapObjectHeader::TryMark() {
  CheckHeader();
  if (encoded_ & kHeaderMarkBitMask)
    return false;
  encoded_ |= kHeaderMarkBitMask;
  return true;
}

NO_SANITIZE_ADDRESS inline bool BasePage::IsValid() const {
  return GetMagic() == magic_;
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

inline HeapObjectHeader::HeapObjectHeader(size_t size,
                                          size_t gc_info_index,
                                          HeaderLocation header_location) {
  // sizeof(HeapObjectHeader) must be equal to or smaller than
  // |kAllocationGranularity|, because |HeapObjectHeader| is used as a header
  // for a freed entry. Given that the smallest entry size is
  // |kAllocationGranurarity|, |HeapObjectHeader| must fit into the size.
  static_assert(
      sizeof(HeapObjectHeader) <= kAllocationGranularity,
      "size of HeapObjectHeader must be smaller than kAllocationGranularity");
#if defined(ARCH_CPU_64_BITS)
  static_assert(sizeof(HeapObjectHeader) == 8,
                "sizeof(HeapObjectHeader) must be 8 bytes");
  magic_ = GetMagic();
#endif

  DCHECK(gc_info_index < GCInfoTable::kMaxIndex);
  DCHECK_LT(size, kNonLargeObjectPageSizeMax);
  DCHECK(!(size & kAllocationMask));
  encoded_ = static_cast<uint32_t>(
      (gc_info_index << kHeaderGCInfoIndexShift) | size |
      (gc_info_index == kGcInfoIndexForFreeListHeader ? kHeaderFreedBitMask
                                                      : 0));

  if (header_location == kNormalPage) {
    DCHECK(!PageFromObject(this)->IsLargeObjectPage());
    static_cast<NormalPage*>(PageFromObject(this))
        ->object_start_bit_map()
        ->SetBit(reinterpret_cast<Address>(this));
  } else {
    DCHECK(PageFromObject(this)->IsLargeObjectPage());
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_PAGE_H_
