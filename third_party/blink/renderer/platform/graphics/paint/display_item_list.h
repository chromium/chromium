// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class JSONArray;

// A container for a list of display items of various types.
class PLATFORM_EXPORT DisplayItemList {
 public:
  static constexpr wtf_size_t kDefaultCapacity = 16;

  // Using 0 as the default value to make 0 also fall back to kDefaultCapacity.
  // The initial capacity will be allocated when the first item is appended.
  explicit DisplayItemList(wtf_size_t initial_capacity = 0)
      : initial_capacity_(initial_capacity ? initial_capacity
                                           : kDefaultCapacity) {}
  ~DisplayItemList();

  DisplayItemList(const DisplayItemList&) = delete;
  DisplayItemList& operator=(const DisplayItemList&) = delete;
  DisplayItemList(DisplayItemList&&) = delete;
  DisplayItemList& operator=(DisplayItemList&&) = delete;

  template <class DerivedItemType, typename... Args>
  DerivedItemType& AllocateAndConstruct(Args&&... args) {
    static_assert(WTF::IsSubclass<DerivedItemType, DisplayItem>::value,
                  "Must use subclass of DisplayItem.");
    static_assert(sizeof(DerivedItemType) <= kMaxItemSize,
                  "DisplayItem subclass is larger than kMaxItemSize.");
    static_assert(kAlignment % alignof(DerivedItemType) == 0,
                  "Derived type requires stronger alignment.");
    DisplayItem& result = AllocateUninitializedItem();
    new (&result) DerivedItemType(std::forward<Args>(args)...);
    return static_cast<DerivedItemType&>(result);
  }

  DisplayItem& AppendByMoving(DisplayItem& item) {
    SECURITY_CHECK(!item.IsTombstone());
    DisplayItem& new_item = AllocateUninitializedItem();
    MoveItem(item, new_item);
    return new_item;
  }

  DisplayItem& ReplaceLastByMoving(DisplayItem& item) {
    SECURITY_CHECK(!item.IsTombstone());
    DisplayItem& last = back();
    last.~DisplayItem();
    MoveItem(item, last);
    return last;
  }

 private:
  // Declares itself as a forward iterator, but also supports a few more
  // things. The whole random access iterator interface is a bit much.
  template <typename BaseIterator, typename ItemType>
  class IteratorWrapper
      : public std::iterator<std::forward_iterator_tag, ItemType> {
    DISALLOW_NEW();

   public:
    IteratorWrapper() = default;
    explicit IteratorWrapper(const BaseIterator& it) : it_(it) {}

    bool operator==(const IteratorWrapper& other) const {
      return it_ == other.it_;
    }
    bool operator!=(const IteratorWrapper& other) const {
      return it_ != other.it_;
    }
    bool operator<(const IteratorWrapper& other) const {
      return it_ < other.it_;
    }
    ItemType& operator*() const { return reinterpret_cast<ItemType&>(*it_); }
    ItemType* operator->() const { return &operator*(); }
    IteratorWrapper operator+(std::ptrdiff_t n) const {
      return IteratorWrapper(it_ + n);
    }
    IteratorWrapper operator++(int) {
      IteratorWrapper tmp = *this;
      ++it_;
      return tmp;
    }
    std::ptrdiff_t operator-(const IteratorWrapper& other) const {
      return it_ - other.it_;
    }
    IteratorWrapper& operator++() {
      ++it_;
      return *this;
    }

   private:
    BaseIterator it_;
  };

  // kAlignment must be a multiple of alignof(derived display item) for each
  // derived display item; the ideal value is the least common multiple.
  // The validity of kAlignment and kMaxItemSize are checked in
  // AllocateAndConstruct().
  static constexpr wtf_size_t kAlignment = alignof(ScrollbarDisplayItem);
  static constexpr wtf_size_t kMaxItemSize = sizeof(ScrollbarDisplayItem);

  struct ItemSlot {
    alignas(kAlignment) uint8_t data[kMaxItemSize];
  };
  using ItemVector = Vector<ItemSlot>;

 public:
  using value_type = DisplayItem;
  using iterator = IteratorWrapper<ItemVector::iterator, DisplayItem>;
  using const_iterator =
      IteratorWrapper<ItemVector::const_iterator, const DisplayItem>;
  iterator begin() { return iterator(items_.begin()); }
  iterator end() { return iterator(items_.end()); }
  const_iterator begin() const { return const_iterator(items_.begin()); }
  const_iterator end() const { return const_iterator(items_.end()); }

  DisplayItem& front() { return *begin(); }
  const DisplayItem& front() const { return *begin(); }
  DisplayItem& back() {
    DCHECK(size());
    return (*this)[size() - 1];
  }
  const DisplayItem& back() const {
    DCHECK(size());
    return (*this)[size() - 1];
  }

  DisplayItem& operator[](wtf_size_t index) { return *(begin() + index); }
  const DisplayItem& operator[](wtf_size_t index) const {
    return *(begin() + index);
  }

  wtf_size_t size() const { return items_.size(); }
  bool IsEmpty() const { return !size(); }

  size_t MemoryUsageInBytes() const {
    return sizeof(*this) + items_.CapacityInBytes();
  }

  // Useful for iterating with a range-based for loop.
  template <typename Iterator>
  class Range {
    DISALLOW_NEW();

   public:
    Range(const Iterator& begin, const Iterator& end)
        : begin_(begin), end_(end) {}
    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    wtf_size_t size() const { return end_ - begin_; }

    // To meet the requirement of gmock ElementsAre().
    using value_type = DisplayItem;
    using const_iterator = DisplayItemList::const_iterator;

   private:
    Iterator begin_;
    Iterator end_;
  };

  // In most cases, we should use PaintChunkSubset::Iterator::DisplayItems()
  // instead of these.
  Range<iterator> ItemsInRange(wtf_size_t begin_index, wtf_size_t end_index) {
    return Range<iterator>(begin() + begin_index, begin() + end_index);
  }
  Range<const_iterator> ItemsInRange(wtf_size_t begin_index,
                                     wtf_size_t end_index) const {
    return Range<const_iterator>(begin() + begin_index, begin() + end_index);
  }

#if DCHECK_IS_ON()
  enum JsonOptions {
    kDefault = 0,
    kClientKnownToBeAlive = 1,
    // Only show a compact representation of the display item list. This flag
    // cannot be used with additional flags such as kShowPaintRecords.
    kCompact = 1 << 1,
    kShowPaintRecords = 1 << 2,
    kShowOnlyDisplayItemTypes = 1 << 3
  };
  typedef unsigned JsonFlags;

  static std::unique_ptr<JSONArray> DisplayItemsAsJSON(
      wtf_size_t first_item_index,
      const Range<const_iterator>& display_items,
      JsonFlags);
#endif  // DCHECK_IS_ON()

 private:
  DisplayItem& AllocateUninitializedItem() {
    if (items_.IsEmpty())
      items_.ReserveCapacity(initial_capacity_);
    items_.emplace_back();
    return reinterpret_cast<DisplayItem&>(items_.back());
  }

  void MoveItem(DisplayItem& item, DisplayItem& new_item) {
    memcpy(static_cast<void*>(&new_item), static_cast<void*>(&item),
           kMaxItemSize);

    // Created a tombstone/"dead display item" that can be safely destructed but
    // should never be used except for debugging and raster invalidation.
    new (&item) DisplayItem;
    DCHECK(item.IsTombstone());
    // We need |visual_rect_| and |outset_for_raster_effects_| of the old
    // display item for raster invalidation. Also, the fields that make up the
    // ID (|client_|, |type_| and |fragment_|) need to match. As their values
    // were either initialized to default values or were left uninitialized by
    // DisplayItem's default constructor, now copy their original values back
    // from |result|.
    item.client_ = new_item.client_;
    item.type_ = new_item.type_;
    item.fragment_ = new_item.fragment_;
    DCHECK_EQ(item.GetId(), new_item.GetId());
    item.visual_rect_ = new_item.visual_rect_;
    item.raster_effect_outset_ = new_item.raster_effect_outset_;
  }

  ItemVector items_;
  wtf_size_t initial_capacity_;
};

using DisplayItemIterator = DisplayItemList::const_iterator;
using DisplayItemRange = DisplayItemList::Range<DisplayItemIterator>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_LIST_H_
