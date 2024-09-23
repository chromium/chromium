// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/constraint_space.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsConstraintSpace {
  LogicalSize available_size;
  union {
    BfcOffset bfc_offset;
    void* rare_data;
  };
  ExclusionSpace exclusion_space;
  unsigned bitfields[1];
};

ASSERT_SIZE(ConstraintSpace, SameSizeAsConstraintSpace);

}  // namespace

const ConstraintSpace& ConstraintSpace::CloneForBlockInInlineIfNeeded(
    std::optional<ConstraintSpace>& space) const {
  if (ShouldTextBoxTrimEnd()) {
    // A block-in-inline always has following lines, though it could be empty.
    // `ShouldTextBoxTrimEnd()` shouldn't trim the end if it's not the last
    // inflow child. See `CreateConstraintSpaceForChild()`.
    //
    // If all following lines are empty, which in turn makes it the last
    // *non-empty* inflow child, `RelayoutForTextBoxTrimEnd()` should run the
    // layout again with `ShouldForceTextBoxTrimEnd()` set.
    space = *this;
    if (ShouldForceTextBoxTrimEnd()) {
      space->SetShouldForceTextBoxTrimEnd(false);
    } else {
      space->SetShouldTextBoxTrimEnd(false);
    }
    return *space;
  } else {
    DCHECK(!ShouldForceTextBoxTrimEnd());
  }

  return *this;
}

String ConstraintSpace::ToString() const {
  return String::Format("Offset: %s,%s Size: %sx%s Clearance: %s",
                        BfcOffset().line_offset.ToString().Ascii().c_str(),
                        BfcOffset().block_offset.ToString().Ascii().c_str(),
                        AvailableSize().inline_size.ToString().Ascii().c_str(),
                        AvailableSize().block_size.ToString().Ascii().c_str(),
                        HasClearanceOffset()
                            ? ClearanceOffset().ToString().Ascii().c_str()
                            : "none");
}

}  // namespace blink
