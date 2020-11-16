// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::WebFullscreenVideoStatus;

namespace {

// If a video takes more that this much of the viewport, it's counted as
// fullscreen without applying the fullscreen heuristics.
// (Assuming we're in the fullscreen mode.)
constexpr float kMostlyFillViewportIntersectionThreshold = 0.85f;

// If a video takes less that this much of the viewport, we don't
// apply the fullscreen heuristics and just declare it not fullscreen.
// A portrait ultrawide video (21:9) playing on a landscape ultrawide screen
// takes about 18% of the screen, that's why 15% looks like a reasonable
// lowerbound of a real-world fullscreen video.
constexpr float kMinPossibleFullscreenIntersectionThreshold = 0.15f;

// This is how much of the viewport around the video can be taken by
// margins and framing for it to still be counted as fullscreen.
// It is measured only in the dominant direction, because of potential ratio
// mismatch that would cause big margins in the other direction.
// For example: portrain video on a landscape screen.
constexpr float kMaxAllowedVideoMarginRatio = 0.15;

// This is how much of the video can be hidden by something
// before it is nor longer counted as fullscreen.
// This helps to disregard custom controls, ads, accidental markup mistakes.
constexpr float kMaxAllowedPortionOfVideoOffScreen = 0.25;

// This heuristic handles a case of videos with an aspect ratio
// different from the screen's aspect ratio.
// Examples: A 4:3 video playing on a 16:9 screen.
//           A portrait video playing on a landscape screen.
// In a nutshell:
//  1. The video should occupy most of the viewport in at least one dimension.
//  2. The video should be almost fully visible on the screen.
bool IsFullscreenVideoOfDifferentRatio(const IntSize& video_size,
                                       const IntSize& viewport_size,
                                       const IntSize& intersection_size) {
  if (video_size.IsEmpty() || viewport_size.IsEmpty())
    return false;

  const float x_occupation_proportion =
      1.0f * intersection_size.Width() / viewport_size.Width();
  const float y_occupation_proportion =
      1.0f * intersection_size.Height() / viewport_size.Height();

  // The video should occupy most of the viewport in at least one dimension.
  if (std::max(x_occupation_proportion, y_occupation_proportion) <
      (1.0 - kMaxAllowedVideoMarginRatio)) {
    return false;
  }

  // The video should be almost fully visible on the screen.
  return video_size.Area() * (1.0 - kMaxAllowedPortionOfVideoOffScreen) <=
         intersection_size.Area();
}

}  // anonymous namespace

MediaCustomControlsFullscreenDetector::MediaCustomControlsFullscreenDetector(
    HTMLVideoElement& video)
    : video_element_(video), viewport_intersection_observer_(nullptr) {
  if (VideoElement().isConnected())
    Attach();
}

void MediaCustomControlsFullscreenDetector::Attach() {
  VideoElement().addEventListener(event_type_names::kLoadedmetadata, this,
                                  true);
  VideoElement().GetDocument().addEventListener(
      event_type_names::kWebkitfullscreenchange, this, true);
  VideoElement().GetDocument().addEventListener(
      event_type_names::kFullscreenchange, this, true);

  // Ideally we'd like to monitor all minute intersection changes here,
  // because any change can potentially affect the fullscreen heuristics,
  // but it's not practical from perf point of view. Given that the heuristics
  // are more of a guess that exact science, it wouldn't be well spent CPU
  // cycles anyway. That's why the observer only triggers on 10% steps in
  // viewport area occupation.
  const WTF::Vector<float> thresholds{
      kMinPossibleFullscreenIntersectionThreshold,
      0.2,
      0.3,
      0.4,
      0.5,
      0.6,
      0.7,
      0.8,
      kMostlyFillViewportIntersectionThreshold};
  viewport_intersection_observer_ = IntersectionObserver::Create(
      {}, thresholds, &(video_element_->GetDocument()),
      WTF::BindRepeating(
          &MediaCustomControlsFullscreenDetector::OnIntersectionChanged,
          WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kMediaIntersectionObserver,
      IntersectionObserver::kDeliverDuringPostLifecycleSteps,
      IntersectionObserver::kFractionOfRoot, 0, false, true);
  viewport_intersection_observer_->observe(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Detach() {
  if (viewport_intersection_observer_) {
    viewport_intersection_observer_->disconnect();
    viewport_intersection_observer_ = nullptr;
  }
  VideoElement().removeEventListener(event_type_names::kLoadedmetadata, this,
                                     true);
  VideoElement().GetDocument().removeEventListener(
      event_type_names::kWebkitfullscreenchange, this, true);
  VideoElement().GetDocument().removeEventListener(
      event_type_names::kFullscreenchange, this, true);
  VideoElement().SetIsEffectivelyFullscreen(
      WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
}

void MediaCustomControlsFullscreenDetector::Invoke(ExecutionContext* context,
                                                   Event* event) {
  DCHECK(event->type() == event_type_names::kLoadedmetadata ||
         event->type() == event_type_names::kWebkitfullscreenchange ||
         event->type() == event_type_names::kFullscreenchange);

  // Video is not loaded yet.
  if (VideoElement().getReadyState() < HTMLMediaElement::kHaveMetadata)
    return;

  TriggerObservation();
}

void MediaCustomControlsFullscreenDetector::ContextDestroyed() {
  Detach();
}

void MediaCustomControlsFullscreenDetector::ReportEffectivelyFullscreen(
    bool effectively_fullscreen) {
  if (!effectively_fullscreen) {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
    return;
  }

  // Picture-in-Picture can be disabled by the website when the API is enabled.
  bool picture_in_picture_allowed =
      !RuntimeEnabledFeatures::PictureInPictureEnabled() &&
      !VideoElement().FastHasAttribute(
          html_names::kDisablepictureinpictureAttr);

  if (picture_in_picture_allowed) {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled);
  } else {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kFullscreenAndPictureInPictureDisabled);
  }
}

void MediaCustomControlsFullscreenDetector::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  if (!viewport_intersection_observer_ || entries.IsEmpty())
    return;

  auto* layout = VideoElement().GetLayoutObject();
  if (!layout || entries.back()->intersectionRatio() <
                     kMinPossibleFullscreenIntersectionThreshold) {
    // Video is not shown at all.
    VideoElement().SetIsDominantVisibleContent(false);
    ReportEffectivelyFullscreen(false);
    return;
  }

  const bool is_mostly_filling_viewport =
      entries.back()->intersectionRatio() >=
      kMostlyFillViewportIntersectionThreshold;
  VideoElement().SetIsDominantVisibleContent(is_mostly_filling_viewport);

  if (!IsVideoOrParentFullscreen()) {
    // The video is outside of a fullscreen element.
    // This is definitely not a fullscreen video experience.
    ReportEffectivelyFullscreen(false);
    return;
  }

  if (is_mostly_filling_viewport) {
    // Video takes most part (85%) of the screen, report fullscreen.
    ReportEffectivelyFullscreen(true);
    return;
  }

  const IntersectionGeometry& geometry = entries.back()->GetGeometry();
  IntSize target_size = RoundedIntSize(geometry.TargetRect().size);
  IntSize intersection_size = RoundedIntSize(geometry.IntersectionRect().size);
  IntSize root_size = RoundedIntSize(geometry.RootRect().size);

  ReportEffectivelyFullscreen(IsFullscreenVideoOfDifferentRatio(
      target_size, root_size, intersection_size));
}

void MediaCustomControlsFullscreenDetector::TriggerObservation() {
  if (!viewport_intersection_observer_)
    return;

  // Removing and re-adding the observable element is just a way to
  // trigger the observation callback and reevaluate the intersection ratio.
  viewport_intersection_observer_->unobserve(&VideoElement());
  viewport_intersection_observer_->observe(&VideoElement());
}

bool MediaCustomControlsFullscreenDetector::IsVideoOrParentFullscreen() {
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(VideoElement().GetDocument());
  if (!fullscreen_element)
    return false;

  return fullscreen_element->contains(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(viewport_intersection_observer_);
}

// static
bool MediaCustomControlsFullscreenDetector::
    IsFullscreenVideoOfDifferentRatioForTesting(
        const IntSize& video_size,
        const IntSize& viewport_size,
        const IntSize& intersection_size) {
  return IsFullscreenVideoOfDifferentRatio(video_size, viewport_size,
                                           intersection_size);
}

}  // namespace blink
