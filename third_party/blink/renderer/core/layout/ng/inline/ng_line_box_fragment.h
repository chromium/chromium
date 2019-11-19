// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CORE_EXPORT NGLineBoxFragment final : public NGFragment {
 public:
  NGLineBoxFragment(WritingMode writing_mode,
                    const NGPhysicalLineBoxFragment& physical_fragment)
      : NGFragment(writing_mode, physical_fragment) {}
};

template <>
struct DowncastTraits<NGLineBoxFragment> {
  static bool AllowFrom(const NGFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentLineBox;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_H_
