// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_

#include <cstring>  // memcpy, memset
#include <type_traits>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class JSONArray;

// A container for a list of display items of various types.
class PLATFORM_EXPORT DisplayItemList {
  DISALLOW_NEW();

 public:
  DisplayItemList() = default;
  ~DisplayItemList() { clear(); }

  DisplayItemList(const DisplayItemList&) = delete;
  DisplayItemList& operator=(const DisplayItemList&) = delete;
  DisplayItemList(DisplayItemList&&) = delete;
  DisplayItemList& operator=(DisplayItemList&&) = delete;

  void ReserveCapacity(wtf_size_t initial_capacity) {
    items_.reserve(initial_capacity);
  }

  void clear();

  // This private section is before the public APIs because some inline public
  // methods depend on the private definitions.
 private:
  // kAlignment must be a multiple of alignof(derived display item) for each
  // derived display item; the ideal value is the least common multiple.
  // The validity of kAlignment and kMaxItemSize are checked in
  // AllocateAndConstruct().
  static constexpr wtf_size_t kAlignment = alignof(ScrollbarDisplayItem);
  static constexpr wtf_size_t kMaxItemSize = sizeof(ScrollbarDisplayItem);

  struct ItemSlot {
    alignas(kAlignment) uint8_t data[kMaxItemSize];
    DISALLOW_NEW();

    DisplayItem& Item() { return *reinterpret_cast<DisplayItem*>(data); }
    const DisplayItem& Item() const {
      return *reinterpret_cast<const DisplayItem*>(data);
    }
  };
  using ItemVector = Vector<ItemSlot>;

  base::span<ItemSlot> ItemSpan() { return base::span(items_); }
  base::span<const ItemSlot> ItemSpan() const { return base::span(items_); }

 public:
  // Useful for iterating with a range-based for loop.
  template <typename SlotType>
  class Range {
    STACK_ALLOCATED();

   public:
    using SpanType = base::span<SlotType>;
    using ItemType = std::conditional_t<std::is_const_v<SlotType>,
                                        const DisplayItem,
                                        DisplayItem>;
    using value_type = ItemType;

    class iterator {
     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = ItemType;
      using difference_type = std::ptrdiff_t;
      using pointer = ItemType*;
      using reference = ItemType&;

      iterator() = default;
      explicit iterator(typename SpanType::iterator it) : it_(it) {}

      ItemType& operator*() const { return it_->Item(); }
      ItemType* operator->() const { return &operator*(); }
      iterator& operator++() {
        ++it_;
        return *this;
      }
      iterator operator++(int) {
        iterator tmp = *this;
        ++it_;
        return tmp;
      }
      std::ptrdiff_t operator-(const iterator& other) const {
        return it_ - other.it_;
      }
      auto operator<=>(const iterator&) const = default;

     private:
      typename SpanType::iterator it_;
    };

    using const_iterator = Range<const SlotType>::iterator;

    Range() = default;
    explicit Range(SpanType span) : span_(span) {}

    iterator begin() const { return iterator(span_.begin()); }
    iterator end() const { return iterator(span_.end()); }
    wtf_size_t size() const {
      return base::checked_cast<wtf_size_t>(span_.size());
    }
    bool IsEmpty() const { return span_.empty(); }

   private:
    SpanType span_;
  };

  using value_type = DisplayItem;
  using iterator = Range<ItemSlot>::iterator;
  using const_iterator = Range<const ItemSlot>::iterator;
  using DisplayItemRange = Range<const ItemSlot>;

  iterator begin() { return iterator(ItemSpan().begin()); }
  iterator end() { return iterator(ItemSpan().end()); }
  const_iterator begin() const { return const_iterator(ItemSpan().begin()); }
  const_iterator end() const { return const_iterator(ItemSpan().end()); }

  DisplayItem& front() { return items_.front().Item(); }
  const DisplayItem& front() const { return items_.front().Item(); }
  DisplayItem& back() { return items_.back().Item(); }
  const DisplayItem& back() const { return items_.back().Item(); }

  DisplayItem& operator[](wtf_size_t index) { return items_[index].Item(); }
  const DisplayItem& operator[](wtf_size_t index) const {
    return items_[index].Item();
  }

  wtf_size_t size() const { return items_.size(); }
  bool IsEmpty() const { return !size(); }

  size_t MemoryUsageInBytes() const {
    return sizeof(*this) + items_.CapacityInBytes();
  }

  // In most cases, we should use PaintChunkSubset::Iterator::DisplayItems()
  // instead of these.
  Range<ItemSlot> ItemsInRange(wtf_size_t begin_index, wtf_size_t end_index) {
    CHECK_LE(begin_index, end_index);
    return Range<ItemSlot>(
        ItemSpan().subspan(begin_index, end_index - begin_index));
  }
  DisplayItemRange ItemsInRange(wtf_size_t begin_index,
                                wtf_size_t end_index) const {
    CHECK_LE(begin_index, end_index);
    return DisplayItemRange(
        ItemSpan().subspan(begin_index, end_index - begin_index));
  }

 public:
  template <class DerivedItemType, typename... Args>
  DerivedItemType& AllocateAndConstruct(Args&&... args) {
    static_assert(IsSubclass<DerivedItemType, DisplayItem>::value,
                  "Must use subclass of DisplayItem.");
    static_assert(sizeof(DerivedItemType) <= kMaxItemSize,
                  "DisplayItem subclass is larger than kMaxItemSize.");
    static_assert(kAlignment % alignof(DerivedItemType) == 0,
                  "Derived type requires stronger alignment.");
    ItemSlot* result = AllocateItemSlot();
    new (result) DerivedItemType(std::forward<Args>(args)...);
    return *reinterpret_cast<DerivedItemType*>(result);
  }

  DisplayItem& AppendByMoving(DisplayItem& item) {
    DCHECK(!item.IsTombstone());
    return MoveItem(item, AllocateItemSlot());
  }

  DisplayItem& ReplaceLastByMoving(DisplayItem& item) {
    DCHECK(!item.IsTombstone());
    DisplayItem& last = back();
    last.Destruct();
    return MoveItem(item, reinterpret_cast<ItemSlot*>(&last));
  }

  void AppendSubsequenceByMoving(DisplayItemList& from,
                                 wtf_size_t begin_index,
                                 wtf_size_t end_index) {
    DCHECK_GE(end_index, begin_index);
    if (end_index == begin_index)
      return;
    DCHECK_LT(begin_index, from.size());
    DCHECK_LE(end_index, from.size());

    wtf_size_t count = end_index - begin_index;
    ItemSlot* new_start_slot = AllocateItemSlots(count);
    size_t bytes_to_move =
        UNSAFE_TODO(reinterpret_cast<uint8_t*>(new_start_slot + count) -
                    reinterpret_cast<uint8_t*>(new_start_slot));
    UNSAFE_TODO(memcpy(static_cast<void*>(new_start_slot),
                       static_cast<void*>(&from[begin_index]), bytes_to_move));
    // This creates tombstones in the original items. Unlike AppendByMoving()
    // for individual items, we won't use the moved items in a subsequence
    // for raster invalidation, so we don't need to keep the other fields of
    // the display items. DisplayItemTest.AllZeroIsTombstone ensures that the
    // cleared items are tombstones.
    UNSAFE_TODO(
        memset(static_cast<void*>(&from[begin_index]), 0, bytes_to_move));
  }

#if DCHECK_IS_ON()
  enum JsonOption {
    kDefault,
    // Only show a compact representation of the display item list. This flag
    // cannot be used with kShowPaintRecords.
    kCompact,
    kShowPaintRecords,
  };

  static std::unique_ptr<JSONArray> DisplayItemsAsJSON(
      const PaintArtifact&,
      wtf_size_t first_item_index,
      const DisplayItemRange& display_items,
      JsonOption);
#else  // DCHECK_IS_ON()
  enum JsonOption { kDefault };
#endif

 private:
  static_assert(std::is_trivially_copyable<value_type>::value,
                "DisplayItemList uses `memcpy` in several member functions; "
                "the `value_type` used by it must be trivially copyable");

  ItemSlot* AllocateItemSlot() { return &items_.emplace_back(); }

  ItemSlot* AllocateItemSlots(wtf_size_t count) {
    items_.Grow(size() + count);
    return UNSAFE_TODO(&items_.back() - (count - 1));
  }

  DisplayItem& MoveItem(DisplayItem& item, ItemSlot* new_item_slot) {
    UNSAFE_TODO(memcpy(static_cast<void*>(new_item_slot),
                       static_cast<void*>(&item), kMaxItemSize));

    // Created a tombstone/"dead display item" that can be safely destructed but
    // should never be used except for debugging and raster invalidation.
    item.CreateTombstone();
    DCHECK(item.IsTombstone());
    // Original values for other fields are kept for debugging and raster
    // invalidation.
    DisplayItem& new_item = *reinterpret_cast<DisplayItem*>(new_item_slot);
    DCHECK_EQ(item.VisualRect(), new_item.VisualRect());
    DCHECK_EQ(item.GetRasterEffectOutset(), new_item.GetRasterEffectOutset());
    return new_item;
  }

  ItemVector items_;
};

using DisplayItemIterator = DisplayItemList::const_iterator;
using DisplayItemRange = DisplayItemList::DisplayItemRange;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
