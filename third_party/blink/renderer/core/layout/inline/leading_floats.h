// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LEADING_FLOATS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LEADING_FLOATS_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/floats_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct LeadingFloat {
  DISALLOW_NEW();

 public:
  LeadingFloat(const PositionedFloat& positioned_float,
               const InlineBreakToken* parallel_flow_break_token)
      : positioned_float(positioned_float),
        parallel_flow_break_token(parallel_flow_break_token) {}

  PositionedFloat positioned_float;
  Member<const InlineBreakToken> parallel_flow_break_token;

  void Trace(Visitor* visitor) const {
    visitor->Trace(positioned_float);
    visitor->Trace(parallel_flow_break_token);
  }
};

// Represents already handled leading floats within an inline formatting
// context. See `PositionLeadingFloats`.
class CORE_EXPORT LeadingFloats {
  STACK_ALLOCATED();

 public:
  ~LeadingFloats() { floats_.clear(); }

  void Add(const PositionedFloat& positioned_float,
           const InlineBreakToken* parallel_flow_break_token) {
    floats_.emplace_back(positioned_float, parallel_flow_break_token);
  }
  void SetHandledIndex(wtf_size_t idx) { handled_index_ = idx; }

  bool Empty() const { return floats_.empty(); }
  wtf_size_t Count() const { return floats_.size(); }
  const LeadingFloat& At(wtf_size_t idx) const { return floats_[idx]; }
  wtf_size_t HandledIndex() const { return handled_index_; }

 private:
  HeapVector<LeadingFloat> floats_;
  wtf_size_t handled_index_ = 0;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::LeadingFloat)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_LEADING_FLOATS_H_
