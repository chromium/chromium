// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class NGPhysicalBoxFragment;

// FragmentData iterator, accompanied by "corresponding" NG layout structures.
// For LayoutBox, this means NGPhysicalBoxFragment. For non-atomic inlines, it
// means NGInlineCursor.
class FragmentDataIterator {
  STACK_ALLOCATED();

 public:
  explicit FragmentDataIterator(const LayoutObject&);

  const NGInlineCursor* Cursor() { return cursor_ ? &(*cursor_) : nullptr; }
  const FragmentData* GetFragmentData() const { return fragment_data_; }
  const NGPhysicalBoxFragment* GetPhysicalBoxFragment() const;

  // Advance the iterator. For LayoutBox fragments this also means that we're
  // going to advance to the next fragmentainer, and thereby the next
  // FragmentData entry. For non-atomic inlines, though, there may be multiple
  // fragment items (because there are multiple lines inside the same
  // fragmentainer, for instance).
  bool Advance();

  bool IsDone() const { return !fragment_data_; }

 private:
  absl::optional<NGInlineCursor> cursor_;
  const LayoutBox* ng_layout_box_ = nullptr;
  const FragmentData* fragment_data_;
  wtf_size_t box_fragment_index_ = 0u;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_ITERATOR_H_
