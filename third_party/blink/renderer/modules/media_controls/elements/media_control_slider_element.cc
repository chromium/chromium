// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_slider_element.h"

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace {

void SetSegmentDivPosition(blink::HTMLDivElement* segment,
                           blink::MediaControlSliderElement::Position position,
                           int width,
                           float zoom_factor) {
  int segment_width =
      clampTo<int>(floor((position.width * width) / zoom_factor));
  int segment_left = clampTo<int>(floor((position.left * width) / zoom_factor));
  int current_width = 0;
  int current_left = 0;

  // Get the current width and left for the segment. If the box is not present
  // then it will be a nullptr so we should assume zero.
  blink::LayoutBox* box = segment->GetLayoutBox();
  if (box) {
    current_width = box->PixelSnappedWidth();
    current_left = box->LogicalLeft().ToInt();
  }

  // If the width and left has not changed then do not update the segment.
  if (segment_width == current_width && segment_left == current_left)
    return;

  StringBuilder builder;
  builder.Append("width: ");
  builder.AppendNumber(segment_width);
  builder.Append("px; left: ");
  builder.AppendNumber(segment_left);
  builder.Append("px;");
  segment->setAttribute("style", builder.ToAtomicString());
}

}  // namespace.

namespace blink {

class MediaControlSliderElement::MediaControlSliderElementResizeObserverDelegate
    final : public ResizeObserver::Delegate {
 public:
  explicit MediaControlSliderElementResizeObserverDelegate(
      MediaControlSliderElement* element)
      : element_(element) {
    DCHECK(element);
  }
  ~MediaControlSliderElementResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    DCHECK_EQ(entries[0]->target(), element_);
    element_->NotifyElementSizeChanged();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(element_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlSliderElement> element_;
};

MediaControlSliderElement::MediaControlSliderElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls),
      before_segment_position_(0, 0),
      after_segment_position_(0, 0),
      segment_highlight_before_(nullptr),
      segment_highlight_after_(nullptr),
      resize_observer_(ResizeObserver::Create(
          GetDocument(),
          MakeGarbageCollected<MediaControlSliderElementResizeObserverDelegate>(
              this))) {
  setType(input_type_names::kRange);
  setAttribute(html_names::kStepAttr, "any");
  OnControlsShown();
}

Element& MediaControlSliderElement::GetTrackElement() {
  // The timeline element has a shadow root with the following
  // structure:
  //
  // #shadow-root
  //   - div
  //     - div::-webkit-slider-runnable-track#track
  Element* track = GetShadowRoot()->getElementById(AtomicString("track"));
  DCHECK(track);
  return *track;
}

void MediaControlSliderElement::SetupBarSegments() {
  DCHECK((segment_highlight_after_ && segment_highlight_before_) ||
         (!segment_highlight_after_ && !segment_highlight_before_));

  if (segment_highlight_after_ || segment_highlight_before_)
    return;

  Element& track = GetTrackElement();
  track.SetShadowPseudoId("-internal-media-controls-segmented-track");

  // Add the following structure to #track.
  //
  // div::internal-track-segment-background (container)
  //   - div::internal-track-segment-highlight-before (blue highlight)
  //   - div::internal-track-segment-highlight-after (dark gray highlight)
  HTMLDivElement* background = MediaControlElementsHelper::CreateDiv(
      "-internal-track-segment-background", &track);
  segment_highlight_before_ = MediaControlElementsHelper::CreateDiv(
      "-internal-track-segment-highlight-before", background);
  segment_highlight_after_ = MediaControlElementsHelper::CreateDiv(
      "-internal-track-segment-highlight-after", background);
}

void MediaControlSliderElement::SetBeforeSegmentPosition(
    MediaControlSliderElement::Position position) {
  DCHECK(segment_highlight_before_);
  before_segment_position_ = position;
  SetSegmentDivPosition(segment_highlight_before_, before_segment_position_,
                        TrackWidth(), ZoomFactor());
}

void MediaControlSliderElement::SetAfterSegmentPosition(
    MediaControlSliderElement::Position position) {
  DCHECK(segment_highlight_after_);
  after_segment_position_ = position;
  SetSegmentDivPosition(segment_highlight_after_, after_segment_position_,
                        TrackWidth(), ZoomFactor());
}

int MediaControlSliderElement::TrackWidth() {
  LayoutBoxModelObject* box = GetTrackElement().GetLayoutBoxModelObject();
  return box ? box->OffsetWidth().Round() : 0;
}

float MediaControlSliderElement::ZoomFactor() const {
  if (!GetDocument().GetLayoutView())
    return 1;
  return GetDocument().GetLayoutView()->ZoomFactor();
}

void MediaControlSliderElement::NotifyElementSizeChanged() {
  SetSegmentDivPosition(segment_highlight_before_, before_segment_position_,
                        TrackWidth(), ZoomFactor());
  SetSegmentDivPosition(segment_highlight_after_, after_segment_position_,
                        TrackWidth(), ZoomFactor());
}

void MediaControlSliderElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(segment_highlight_before_);
  visitor->Trace(segment_highlight_after_);
  visitor->Trace(resize_observer_);
  MediaControlInputElement::Trace(visitor);
}

void MediaControlSliderElement::OnControlsShown() {
  resize_observer_->observe(this);
}

void MediaControlSliderElement::OnControlsHidden() {
  resize_observer_->disconnect();
}

}  // namespace blink
