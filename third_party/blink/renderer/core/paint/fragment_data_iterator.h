// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_

#include <cstddef>
#include <optional>

#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class PhysicalBoxFragment;

template <typename Data, typename Head>
class FragmentDataIteratorBase {
  STACK_ALLOCATED();

 public:
  explicit FragmentDataIteratorBase(Head& head) : fragment_head_(head) {}
  explicit FragmentDataIteratorBase(std::nullptr_t) {}

  Data* GetFragmentData() const {
    return !IsDone() ? &fragment_head_.at(idx_) : nullptr;
  }

  bool Advance() {
    if (IsDone()) {
      return false;
    }
    idx_++;
    if (idx_ >= fragment_head_.size()) {
      idx_ = kNotFound;
      return false;
    }
    return true;
  }

  bool IsDone() const { return idx_ == kNotFound; }

 protected:
  Head& fragment_head_;
  wtf_size_t idx_ = 0u;
};

class FragmentDataIterator
    : public FragmentDataIteratorBase<const FragmentData,
                                      const FragmentDataList> {
 public:
  explicit FragmentDataIterator(const LayoutObject& object)
      : FragmentDataIteratorBase(object.FragmentList()) {}
};

// FragmentData iterator, accompanied by "corresponding" NG layout structures.
// For LayoutBox, this means PhysicalBoxFragment. For non-atomic inlines, it
// means InlineCursor. For non-atomic inlines, this also means that Advance()
// will stop for each line on which the LayoutObject is represented. There may
// be multiple lines per FragmentData (whereas there's just one FragmentData per
// fragmentainer), meaning that Advance() may stop several times at the same
// FragmentData while progressing through the lines.
class AccompaniedFragmentIterator
    : public FragmentDataIteratorBase<const FragmentData,
                                      const FragmentDataList> {
  STACK_ALLOCATED();

 public:
  explicit AccompaniedFragmentIterator(const LayoutObject&);

  const InlineCursor* Cursor() { return cursor_ ? &(*cursor_) : nullptr; }
  const PhysicalBoxFragment* GetPhysicalBoxFragment() const;

  // Advance the iterator. For LayoutBox fragments this also means that we're
  // going to advance to the next fragmentainer, and thereby the next
  // FragmentData entry. For non-atomic inlines, though, there may be multiple
  // fragment items (because there are multiple lines inside the same
  // fragmentainer, for instance).
  bool Advance();

 private:
  std::optional<InlineCursor> cursor_;
  const LayoutBox* layout_box_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
