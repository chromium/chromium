// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct LogicalSize;

class CORE_EXPORT NGFragment {
  STACK_ALLOCATED();

 public:
  NGFragment(WritingMode writing_mode,
             const NGPhysicalFragment& physical_fragment)
      : physical_fragment_(physical_fragment), writing_mode_(writing_mode) {}

  // Returns the border-box size.
  LayoutUnit InlineSize() const {
    return writing_mode_ == WritingMode::kHorizontalTb
               ? physical_fragment_.Size().width
               : physical_fragment_.Size().height;
  }
  LayoutUnit BlockSize() const {
    return writing_mode_ == WritingMode::kHorizontalTb
               ? physical_fragment_.Size().height
               : physical_fragment_.Size().width;
  }
  LogicalSize Size() const {
    return physical_fragment_.Size().ConvertToLogical(writing_mode_);
  }

 protected:
  const NGPhysicalFragment& physical_fragment_;
  const WritingMode writing_mode_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_H_
