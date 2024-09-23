/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/track/text_track_container.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

class VttCueBoxResizeDelegate final : public ResizeObserver::Delegate {
 public:
  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(entries.size(), 1u);
    VttCueLayoutAlgorithm(*To<VTTCueBox>(entries[0]->target())).Layout();
  }
};

}  // anonymous namespace

VTTCueBox::VTTCueBox(Document& document)
    : HTMLDivElement(document),
      snap_to_lines_position_(std::numeric_limits<float>::quiet_NaN()) {
  SetShadowPseudoId(AtomicString("-webkit-media-text-track-display"));
}

void VTTCueBox::Trace(Visitor* visitor) const {
  visitor->Trace(box_size_observer_);
  HTMLDivElement::Trace(visitor);
}

void VTTCueBox::ApplyCSSProperties(
    const VTTDisplayParameters& display_parameters) {
  // http://dev.w3.org/html5/webvtt/#applying-css-properties-to-webvtt-node-objects

  // Initialize the (root) list of WebVTT Node Objects with the following CSS
  // settings:

  // the 'position' property must be set to 'absolute'
  SetInlineStyleProperty(CSSPropertyID::kPosition, CSSValueID::kAbsolute);

  //  the 'unicode-bidi' property must be set to 'plaintext'
  SetInlineStyleProperty(CSSPropertyID::kUnicodeBidi, CSSValueID::kPlaintext);

  // the 'direction' property must be set to direction
  SetInlineStyleProperty(CSSPropertyID::kDirection,
                         display_parameters.direction);

  // the 'writing-mode' property must be set to writing-mode
  SetInlineStyleProperty(CSSPropertyID::kWritingMode,
                         display_parameters.writing_mode);

  const gfx::PointF& position = display_parameters.position;
  const bool is_horizontal =
      display_parameters.writing_mode == CSSValueID::kHorizontalTb;
  original_percent_position_ = is_horizontal ? position.y() : position.x();

  // the 'top' property must be set to top,
  SetInlineStyleProperty(CSSPropertyID::kTop, position.y(),
                         CSSPrimitiveValue::UnitType::kPercentage);

  // the 'left' property must be set to left
  SetInlineStyleProperty(CSSPropertyID::kLeft, position.x(),
                         CSSPrimitiveValue::UnitType::kPercentage);

  // the 'width' property must be set to width, and the 'height' property  must
  // be set to height
  if (is_horizontal) {
    SetInlineStyleProperty(CSSPropertyID::kWidth, display_parameters.size,
                           CSSPrimitiveValue::UnitType::kPercentage);
    SetInlineStyleProperty(CSSPropertyID::kHeight, CSSValueID::kAuto);
  } else {
    SetInlineStyleProperty(CSSPropertyID::kWidth, CSSValueID::kAuto);
    SetInlineStyleProperty(CSSPropertyID::kHeight, display_parameters.size,
                           CSSPrimitiveValue::UnitType::kPercentage);
  }

  // The 'text-align' property on the (root) List of WebVTT Node Objects must
  // be set to the value in the second cell of the row of the table below
  // whose first cell is the value of the corresponding cue's WebVTT cue
  // text alignment:
  SetInlineStyleProperty(CSSPropertyID::kTextAlign,
                         display_parameters.text_align);

  // TODO(foolip): The position adjustment for non-snap-to-lines cues has
  // been removed from the spec:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=19178
  if (std::isnan(display_parameters.snap_to_lines_position)) {
    // 10.13.1 Set up x and y:
    // Note: x and y are set through the CSS left and top above.
    // 10.13.2 Position the boxes in boxes such that the point x% along the
    // width of the bounding box of the boxes in boxes is x% of the way
    // across the width of the video's rendering area, and the point y%
    // along the height of the bounding box of the boxes in boxes is y%
    // of the way across the height of the video's rendering area, while
    // maintaining the relative positions of the boxes in boxes to each
    // other.
    SetInlineStyleProperty(CSSPropertyID::kTransform,
                           String::Format("translate(-%.2f%%, -%.2f%%)",
                                          position.x(), position.y()));
    // Longhands of `white-space: pre`.
    SetInlineStyleProperty(CSSPropertyID::kWhiteSpaceCollapse,
                           CSSValueID::kPreserve);
    SetInlineStyleProperty(CSSPropertyID::kTextWrapMode, CSSValueID::kNowrap);
  }

  // The snap-to-lines position is propagated to VttCueLayoutAlgorithm.
  snap_to_lines_position_ = display_parameters.snap_to_lines_position;
}

LayoutObject* VTTCueBox::CreateLayoutObject(const ComputedStyle& style) {
  // If WebVTT Regions are used, the regular WebVTT layout algorithm is no
  // longer necessary, since cues having the region parameter set do not have
  // any positioning parameters. Also, in this case, the regions themselves
  // have positioning information.
  if (IsInRegion())
    return HTMLDivElement::CreateLayoutObject(style);

  // We create a standard block-flow container.
  // See the comment in vtt_cue_layout_algorithm.h about how we adjust
  // VTTCueBox positions.
  return MakeGarbageCollected<LayoutBlockFlow>(this);
}

Node::InsertionNotificationRequest VTTCueBox::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected() && !IsInRegion()) {
    DCHECK(!box_size_observer_);
    box_size_observer_ =
        ResizeObserver::Create(GetDocument().domWindow(),
                               MakeGarbageCollected<VttCueBoxResizeDelegate>());
    box_size_observer_->observe(this);
    RevertAdjustment();
  }
  return HTMLDivElement::InsertedInto(insertion_point);
}

void VTTCueBox::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);
  if (!box_size_observer_)
    return;
  box_size_observer_->disconnect();
  box_size_observer_.Clear();
}

bool VTTCueBox::IsInRegion() const {
  return parentNode() && !IsA<TextTrackContainer>(parentNode());
}

LayoutUnit& VTTCueBox::StartAdjustment(LayoutUnit new_value,
                                       base::PassKey<VttCueLayoutAlgorithm>) {
  adjusted_position_ = new_value;
  DCHECK(IsAdjusted()) << new_value;
  return adjusted_position_;
}

void VTTCueBox::RevertAdjustment() {
  adjusted_position_ = LayoutUnit::Min();
}

LayoutUnit VTTCueBox::AdjustedPosition(
    LayoutUnit full_dimention,
    base::PassKey<VttCueLayoutAlgorithm>) const {
  return IsAdjusted()
             ? adjusted_position_
             : LayoutUnit(full_dimention * original_percent_position_ / 100);
}

}  // namespace blink
