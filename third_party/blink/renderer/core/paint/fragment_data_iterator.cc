// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"

namespace blink {

AccompaniedFragmentIterator::AccompaniedFragmentIterator(
    const LayoutObject& object)
    : FragmentDataIterator(object) {
  if (const auto* box = DynamicTo<LayoutBox>(&object)) {
    if (box->IsLayoutNGObject())
      ng_layout_box_ = box;
    return;
  }

  if (object.IsInLayoutNGInlineFormattingContext()) {
    cursor_.emplace();
    cursor_->MoveToIncludingCulledInline(object);
  }
}

const PhysicalBoxFragment* AccompaniedFragmentIterator::GetPhysicalBoxFragment()
    const {
  if (ng_layout_box_) {
    return ng_layout_box_->GetPhysicalFragment(idx_);
  }
  return nullptr;
}

bool AccompaniedFragmentIterator::Advance() {
  if (IsDone()) {
    return false;
  }

  if (cursor_) {
    wtf_size_t fragmentainer_index = cursor_->ContainerFragmentIndex();
    cursor_->MoveToNextForSameLayoutObject();
    // Are we're still in the same fragmentainer? If we are, we shouldn't
    // advance FragmentData, since we only create one of those per container
    // fragment.
    if (*cursor_ && fragmentainer_index == cursor_->ContainerFragmentIndex())
      return true;
  }

#if DCHECK_IS_ON()
  wtf_size_t previous_idx = idx_;
#endif

  FragmentDataIterator::Advance();

  if (IsDone()) {
#if DCHECK_IS_ON()
    // We're done, since there are no more FragmentData entries. Assert that
    // this agrees with the NG side of things.
    if (cursor_) {
      DCHECK(!*cursor_);
    } else if (ng_layout_box_) {
      DCHECK_EQ(ng_layout_box_->PhysicalFragmentCount(), previous_idx + 1);
    }
#endif
    ng_layout_box_ = nullptr;
    return false;
  }

#if DCHECK_IS_ON()
  // We have another FragmentData entry, so we're not done. Assert that this
  // agrees with the NG side of things.
  if (ng_layout_box_) {
    DCHECK_GT(ng_layout_box_->PhysicalFragmentCount(), idx_);
  } else if (cursor_) {
    DCHECK(*cursor_);
  }
#endif

  return true;
}

}  // namespace blink
