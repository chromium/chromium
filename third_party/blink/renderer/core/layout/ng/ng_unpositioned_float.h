// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnpositionedFloat_h
#define NGUnpositionedFloat_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGUnpositionedFloat final {
  DISALLOW_NEW();

 public:
  NGUnpositionedFloat(NGBlockNode node, const NGBlockBreakToken* token)
      : node(node), token(token) {}

  NGUnpositionedFloat(NGUnpositionedFloat&&) noexcept = default;
  NGUnpositionedFloat(const NGUnpositionedFloat&) noexcept = default;
  NGUnpositionedFloat& operator=(NGUnpositionedFloat&&) = default;
  NGUnpositionedFloat& operator=(const NGUnpositionedFloat&) = default;

  NGBlockNode node;
  scoped_refptr<const NGBlockBreakToken> token;

  // layout_result and margins are used as a cache when measuring the
  // inline_size of a float in an inline context.
  scoped_refptr<NGLayoutResult> layout_result;
  NGBoxStrut margins;

  bool IsLeft() const { return node.Style().Floating() == EFloat::kLeft; }
  bool IsRight() const { return node.Style().Floating() == EFloat::kRight; }
  EClear ClearType() const { return node.Style().Clear(); }
};

}  // namespace blink

#endif  // NGUnpositionedFloat_h
