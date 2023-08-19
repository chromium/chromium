// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LEADING_FLOATS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LEADING_FLOATS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

//
// Represents already handled leading floats within an inline formatting
// context. See `PositionLeadingFloats`.
//
struct CORE_EXPORT NGLeadingFloats {
  STACK_ALLOCATED();

 public:
  wtf_size_t handled_index = 0;
  NGPositionedFloatVector floats;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LEADING_FLOATS_H_
