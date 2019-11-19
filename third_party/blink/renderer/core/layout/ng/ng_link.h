// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

// Class representing the offset of a child fragment relative to the
// parent fragment. Fragments themselves have no position information
// allowing entire fragment subtrees to be reused and cached regardless
// of placement.
// This class is stored in a C-style regular array on
// NGPhysicalContainerFragment. It cannot have destructors. Fragment reference
// counting is done manually.
struct CORE_EXPORT NGLink {
  PhysicalOffset Offset() const { return offset; }
  const NGPhysicalFragment* get() const { return fragment; }

  operator bool() const { return fragment; }
  const NGPhysicalFragment& operator*() const { return *fragment; }
  const NGPhysicalFragment* operator->() const { return fragment; }

  // Returns a |NGLink| with newer generation if exists, or |this|. See
  // |NGPhysicalFragment::PostLayout()| for more details.
  const NGLink PostLayout() const {
    if (const NGPhysicalFragment* new_fragment = fragment->PostLayout())
      return {new_fragment, offset};
    return *this;
  }

  const NGPhysicalFragment* fragment;
  PhysicalOffset offset;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_
