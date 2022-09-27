// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"

namespace blink {

FragmentDataIterator::FragmentDataIterator(const LayoutObject& object) {
  fragment_data_ = &object.FirstFragment();
  if (const auto* box = DynamicTo<LayoutBox>(&object)) {
    if (box->IsLayoutNGObject())
      ng_layout_box_ = box;
    return;
  }

  if (object.IsInLayoutNGInlineFormattingContext()) {
    // TODO(mstensho): Avoid falling back to legacy code when there are
    // continuations, as that will look bad if we're block-fragmented.
    if (object.VirtualContinuation())
      return;
    cursor_.emplace();
    cursor_->MoveToIncludingCulledInline(object);
  }
}

const NGPhysicalBoxFragment* FragmentDataIterator::GetPhysicalBoxFragment()
    const {
  if (ng_layout_box_)
    return ng_layout_box_->GetPhysicalFragment(box_fragment_index_);
  return nullptr;
}

bool FragmentDataIterator::Advance() {
  if (!fragment_data_)
    return false;

  if (cursor_) {
    wtf_size_t fragmentainer_index = cursor_->ContainerFragmentIndex();
    cursor_->MoveToNextForSameLayoutObject();
    // Are we're still in the same fragmentainer? If we are, we shouldn't
    // advance FragmentData, since we only create one of those per container
    // fragment.
    if (*cursor_ && fragmentainer_index == cursor_->ContainerFragmentIndex())
      return true;
  }

  fragment_data_ = fragment_data_->NextFragment();
  if (!fragment_data_) {
#if DCHECK_IS_ON()
    // We're done, since there are no more FragmentData entries. Assert that
    // this agrees with the NG side of things.
    if (cursor_) {
      DCHECK(!*cursor_);
    } else if (ng_layout_box_) {
      DCHECK_EQ(ng_layout_box_->PhysicalFragmentCount(),
                box_fragment_index_ + 1);
    }
#endif
    return false;
  }

  if (ng_layout_box_)
    box_fragment_index_++;

#if DCHECK_IS_ON()
  // We have another FragmentData entry, so we're not done. Assert that this
  // agrees with the NG side of things.
  if (ng_layout_box_)
    DCHECK_GT(ng_layout_box_->PhysicalFragmentCount(), box_fragment_index_);
  else if (cursor_)
    DCHECK(*cursor_);
#endif

  return true;
}

}  // namespace blink
