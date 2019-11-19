// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_FRAGMENT_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_FRAGMENT_GEOMETRY_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"

namespace blink {

// This represents the initial (pre-layout) geometry of a fragment. E.g.
//  - The inline-size of the fragment.
//  - The block-size of the fragment (might be |kIndefiniteSize| if height is
//    'auto' for example).
//  - The border, scrollbar, and padding.
// This *doesn't* necessarily represent the final geometry of the fragment.
struct NGFragmentGeometry {
  LogicalSize border_box_size;
  NGBoxStrut border;
  NGBoxStrut scrollbar;
  NGBoxStrut padding;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_FRAGMENT_GEOMETRY_H_
