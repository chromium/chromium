// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;

using LayoutNGBlock = LayoutNGMixin<LayoutBlock>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_BLOCK_H_
