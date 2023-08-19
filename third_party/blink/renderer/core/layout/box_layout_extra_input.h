// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"

namespace blink {

class LayoutReplaced;

// Extra input data for laying out a LayoutReplaced. The object is associated
// with a target LayoutReplaced by SetBoxLayoutExtraInput().
struct BoxLayoutExtraInput {
  // BoxLayoutExtraInput is always allocated on the stack as it is scoped to
  // layout, but DISALLOW_NEW is used here since LayoutReplaced has a raw
  // pointer to it.
  DISALLOW_NEW();

  // The border-box size computed by NGReplacedLayoutAlgorithm.
  PhysicalSize size;

  // Border and padding values.
  NGPhysicalBoxStrut border_padding;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
