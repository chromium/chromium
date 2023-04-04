/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

ScrollAnimatorBase::ScrollAnimatorBase(ScrollableArea* scrollable_area)
    : scrollable_area_(scrollable_area) {}

ScrollAnimatorBase::~ScrollAnimatorBase() = default;

ScrollOffset ScrollAnimatorBase::ComputeDeltaToConsume(
    const ScrollOffset& delta) const {
  ScrollOffset new_pos =
      scrollable_area_->ClampScrollOffset(current_offset_ + delta);
  return new_pos - current_offset_;
}

ScrollResult ScrollAnimatorBase::UserScroll(
    ui::ScrollGranularity,
    const ScrollOffset& delta,
    ScrollableArea::ScrollCallback on_finish) {
  // Run the callback for non-animation user scroll.

  ScrollOffset consumed_delta = ComputeDeltaToConsume(delta);
  ScrollOffset new_pos = current_offset_ + consumed_delta;
  if (current_offset_ == new_pos) {
    if (on_finish) {
      std::move(on_finish).Run(
          ScrollableArea::ScrollCompletionMode::kZeroDelta);
    }
    return ScrollResult(false, false, delta.x(), delta.y());
  }

  SetCurrentOffset(new_pos);
  ScrollOffsetChanged(current_offset_, mojom::blink::ScrollType::kUser);

  if (on_finish) {
    std::move(on_finish).Run(ScrollableArea::ScrollCompletionMode::kFinished);
  }
  return ScrollResult(consumed_delta.x(), consumed_delta.y(),
                      delta.x() - consumed_delta.x(),
                      delta.y() - consumed_delta.y());
}

void ScrollAnimatorBase::ScrollToOffsetWithoutAnimation(
    const ScrollOffset& offset) {
  SetCurrentOffset(offset);
  ScrollOffsetChanged(current_offset_, mojom::blink::ScrollType::kUser);
}

void ScrollAnimatorBase::SetCurrentOffset(const ScrollOffset& offset) {
  current_offset_ = offset;
}

ScrollOffset ScrollAnimatorBase::CurrentOffset() const {
  return current_offset_;
}

void ScrollAnimatorBase::Trace(Visitor* visitor) const {
  visitor->Trace(scrollable_area_);
  ScrollAnimatorCompositorCoordinator::Trace(visitor);
}

}  // namespace blink
