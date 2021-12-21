// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_STATE_H_

namespace blink {

enum CompositingState {
  // The layer paints into its enclosing composited ancestor.
  kNotComposited = 0,

  kPaintsIntoOwnBacking = 1,

  // In this state, the Layer subtree paints into a backing that is shared by
  // several Layer subtrees.
  kPaintsIntoGroupedBacking = 2
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_STATE_H_
