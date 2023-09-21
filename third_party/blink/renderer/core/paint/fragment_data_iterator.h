// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class NGPhysicalBoxFragment;

template <typename Iterator, typename Data>
class FragmentDataIteratorBase {
  STACK_ALLOCATED();

 public:
  explicit FragmentDataIteratorBase(Data* data) : fragment_data_(data) {}

  Data* GetFragmentData() const { return fragment_data_; }
  bool Advance() {
    if (fragment_data_) {
      fragment_data_ = fragment_data_->NextFragment();
    }
    return !!fragment_data_;
  }
  bool IsDone() const { return !fragment_data_; }

  Iterator& begin() { return *static_cast<Iterator*>(this); }
  Iterator end() { return Iterator(nullptr); }
  bool operator!=(const Iterator& other) {
    return fragment_data_ != other.fragment_data_;
  }
  Data& operator*() const { return *fragment_data_; }
  Iterator& operator++() {
    Advance();
    return *static_cast<Iterator*>(this);
  }

 protected:
  Data* fragment_data_;
};

class FragmentDataIterator
    : public FragmentDataIteratorBase<FragmentDataIterator,
                                      const FragmentData> {
 public:
  explicit FragmentDataIterator(const LayoutObject& object)
      : FragmentDataIteratorBase(&object.FirstFragment()) {}
  explicit FragmentDataIterator(nullptr_t)
      : FragmentDataIteratorBase(nullptr) {}
};

class MutableFragmentDataIterator
    : public FragmentDataIteratorBase<MutableFragmentDataIterator,
                                      FragmentData> {
 public:
  explicit MutableFragmentDataIterator(const LayoutObject& object)
      : FragmentDataIteratorBase(
            &object.GetMutableForPainting().FirstFragment()) {}
  explicit MutableFragmentDataIterator(nullptr_t)
      : FragmentDataIteratorBase(nullptr) {}
};

// FragmentData iterator, accompanied by "corresponding" NG layout structures.
// For LayoutBox, this means NGPhysicalBoxFragment. For non-atomic inlines, it
// means NGInlineCursor. For non-atomic inlines, this also means that Advance()
// will stop for each line on which the LayoutObject is represented. There may
// be multiple lines per FragmentData (whereas there's just one FragmentData per
// fragmentainer), meaning that Advance() may stop several times at the same
// FragmentData while progressing through the lines.
class AccompaniedFragmentIterator : public FragmentDataIterator {
  STACK_ALLOCATED();

 public:
  explicit AccompaniedFragmentIterator(const LayoutObject&);

  const NGInlineCursor* Cursor() { return cursor_ ? &(*cursor_) : nullptr; }
  const NGPhysicalBoxFragment* GetPhysicalBoxFragment() const;

  // Advance the iterator. For LayoutBox fragments this also means that we're
  // going to advance to the next fragmentainer, and thereby the next
  // FragmentData entry. For non-atomic inlines, though, there may be multiple
  // fragment items (because there are multiple lines inside the same
  // fragmentainer, for instance).
  bool Advance();

 private:
  absl::optional<NGInlineCursor> cursor_;
  const LayoutBox* ng_layout_box_ = nullptr;
  wtf_size_t box_fragment_index_ = 0u;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
