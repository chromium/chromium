// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SUBTREE_PAINT_PROPERTY_UPDATE_REASON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SUBTREE_PAINT_PROPERTY_UPDATE_REASON_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class SubtreePaintPropertyUpdateReason : unsigned {
  kNone = 0,
  kContainerChainMayChange = 1 << 0,
  kFragmentsChanged = 1 << 1,
  kPreviouslySkipped = 1 << 2,
  kPrinting = 1 << 3,
  kTransformStyleChanged = 1 << 4
};
enum { kSubtreePaintPropertyUpdateReasonsBitfieldWidth = 5 };

PLATFORM_EXPORT String
SubtreePaintPropertyUpdateReasonsToString(unsigned bitmask);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SUBTREE_PAINT_PROPERTY_UPDATE_REASON_H_
