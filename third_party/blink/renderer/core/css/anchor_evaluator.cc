// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_evaluator.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"

namespace blink {

AnchorScope::AnchorScope(Mode mode,
                         const ComputedStyleBuilder& builder,
                         AnchorEvaluator* anchor_evaluator)
    : AnchorScope(mode,
                  builder.InsetAreaOffsets(),
                  anchor_evaluator) {}

AnchorScope::AnchorScope(Mode mode,
                         std::optional<InsetAreaOffsets> inset_area_offsets,
                         AnchorEvaluator* anchor_evaluator)
    : anchor_evaluator_(anchor_evaluator) {
  if (anchor_evaluator) {
    original_mode_ = anchor_evaluator_->mode_;
    original_inset_area_offsets_ = anchor_evaluator_->inset_area_offsets_;
    anchor_evaluator_->mode_ = mode;
    anchor_evaluator_->inset_area_offsets_ = inset_area_offsets;
  }
}

AnchorScope::AnchorScope(Mode mode, AnchorEvaluator* anchor_evaluator)
    : anchor_evaluator_(anchor_evaluator) {
  if (anchor_evaluator) {
    original_mode_ = anchor_evaluator->mode_;
    anchor_evaluator->mode_ = mode;
    // Store the existing value so that we reset back to the same value
    original_inset_area_offsets_ = anchor_evaluator->inset_area_offsets_;
  }
}

}  // namespace blink
