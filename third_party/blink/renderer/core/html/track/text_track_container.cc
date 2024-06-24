/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/text_track_container.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/cue_timeline.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "ui/accessibility/accessibility_features.h"

namespace blink {

namespace {

class VideoElementResizeDelegate final : public ResizeObserver::Delegate {
 public:
  VideoElementResizeDelegate(TextTrackContainer& container)
      : ResizeObserver::Delegate(), text_track_container_(container) {}

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(entries.size(), 1u);
    DCHECK(IsA<HTMLVideoElement>(entries[0]->target()));
    text_track_container_->UpdateDefaultFontSize(
        entries[0]->target()->GetLayoutObject());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(text_track_container_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<TextTrackContainer> text_track_container_;
};

}  // namespace

TextTrackContainer::TextTrackContainer(HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      media_element_(&media_element),
      default_font_size_(0) {
  SetShadowPseudoId(AtomicString("-webkit-media-text-track-container"));
  if (IsA<HTMLVideoElement>(*media_element_))
    ObserveSizeChanges(*media_element_);
}

void TextTrackContainer::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  visitor->Trace(video_size_observer_);
  HTMLDivElement::Trace(visitor);
}

Node::InsertionNotificationRequest TextTrackContainer::InsertedInto(
    ContainerNode& root) {
  if (!video_size_observer_ && media_element_->isConnected() &&
      IsA<HTMLVideoElement>(*media_element_)) {
    ObserveSizeChanges(*media_element_);
  }

  return HTMLDivElement::InsertedInto(root);
}

void TextTrackContainer::RemovedFrom(ContainerNode& insertion_point) {
  DCHECK(!media_element_->isConnected());

  HTMLDivElement::RemovedFrom(insertion_point);

  if (video_size_observer_) {
    video_size_observer_->disconnect();
    video_size_observer_.Clear();
  }
}

LayoutObject* TextTrackContainer::CreateLayoutObject(
    const ComputedStyle& style) {
  return MakeGarbageCollected<LayoutBlockFlow>(this);
}

void TextTrackContainer::ObserveSizeChanges(Element& element) {
  video_size_observer_ = ResizeObserver::Create(
      GetDocument().domWindow(),
      MakeGarbageCollected<VideoElementResizeDelegate>(*this));
  video_size_observer_->observe(&element);
}

void TextTrackContainer::UpdateDefaultFontSize(
    LayoutObject* media_layout_object) {
  if (!media_layout_object || !IsA<LayoutVideo>(media_layout_object))
    return;
  // FIXME: The video size is used to calculate the font size (a workaround
  // for lack of per-spec vh/vw support) but the whole media element is used
  // for cue rendering. This is inconsistent. See also the somewhat related
  // spec bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=28105
  PhysicalSize video_size = To<LayoutBox>(media_layout_object)->ContentSize();
  LayoutUnit smallest_dimension = std::min(video_size.height, video_size.width);
  float font_size = smallest_dimension * 0.05f;
  if (media_layout_object->GetFrame())
    font_size /= media_layout_object->GetFrame()->LayoutZoomFactor();

  // Avoid excessive FP precision issue.
  // C11 5.2.4.2.2:9 requires assignment and cast to remove extra precision, but
  // the behavior is currently not portable. font_size may have precision higher
  // than default_font_size_ thus straight comparison can fail despite they cast
  // to the same float value.
  volatile float& current_font_size = default_font_size_;
  float old_font_size = current_font_size;
  current_font_size = font_size;
  if (current_font_size == old_font_size)
    return;
  SetInlineStyleProperty(CSSPropertyID::kFontSize, default_font_size_,
                         CSSPrimitiveValue::UnitType::kPixels);
}

void TextTrackContainer::UpdateDisplay(HTMLMediaElement& media_element,
                                       ExposingControls exposing_controls) {
  if (!media_element.TextTracksVisible()) {
    RemoveChildren();
    return;
  }

  // http://dev.w3.org/html5/webvtt/#dfn-rules-for-updating-the-display-of-webvtt-text-tracks

  // 1. If the media element is an audio element, or is another playback
  // mechanism with no rendering area, abort these steps. There is nothing to
  // render.
  if (IsA<HTMLAudioElement>(media_element))
    return;

  // 2. Let video be the media element or other playback mechanism.
  auto& video = To<HTMLVideoElement>(media_element);

  // 3. Let output be an empty list of absolutely positioned CSS block boxes.

  // Note: This is a layout algorithm, expressed terms of appending CSS block
  // boxes to output, and the "apply WebVTT cue settings" part is implemented
  // in VttCueLayoutAlgorithm. Here we merely create the DOM tree from which
  // the layout tree is built and append it to this TextTrackContainer.

  // 4. If the user agent is exposing a user interface for video, add to
  // output one or more completely transparent positioned CSS block boxes that
  // cover the same region as the user interface.

  // Note: Overlap checking for the controls is implemented in
  // VttCueLayoutAlgorithm without a placeholder box (element or layout object).

  // 5. If the last time these rules were run, the user agent was not exposing
  // a user interface for video, but now it is, optionally let reset be true.
  // Otherwise, let reset be false.
  bool reset = exposing_controls == kDidStartExposingControls;

  // 6. Let tracks be the subset of video's list of text tracks that have as
  // their rules for updating the text track rendering these rules for
  // updating the display of WebVTT text tracks, and whose text track mode is
  // showing or showing by default.
  // 7. Let cues be an empty list of text track cues.
  // 8. For each track track in tracks, append to cues all the cues from
  // track's list of cues that have their text track cue active flag set.
  const CueList& active_cues = video.GetCueTimeline().CurrentlyActiveCues();

  // 9. If reset is false, then, for each text track cue cue in cues: if cue's
  // text track cue display state has a set of CSS boxes, then add those boxes
  // to output, and remove cue from cues.

  // Note: Removing all children will cause them to be re-inserted below,
  // invalidating the layout.
  // effect
  if (reset)
    RemoveChildren();

  // 10. For each text track cue cue in cues that has not yet had
  // corresponding CSS boxes added to output, in text track cue order, run the
  // following substeps:
  double movie_time = video.currentTime();
  for (const auto& active_cue : active_cues) {
    TextTrackCue* cue = active_cue.Data();

    DCHECK(cue->IsActive());
    if (!cue->track() || !cue->track()->IsRendered() || !cue->IsActive())
      continue;

    if (!cue->track()->IsSpokenKind()) {
      cue->UpdateDisplay(*this);
    }

    cue->UpdatePastAndFutureNodes(movie_time);
  }

  // 11. Return output.
  // See the note for step 3 for why there is no output to return.
}

}  // namespace blink
