// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::WebFullscreenVideoStatus;

namespace {

constexpr base::TimeDelta kCheckFullscreenInterval =
    base::TimeDelta::FromSeconds(1);
constexpr float kMostlyFillViewportThresholdOfOccupationProportion = 0.85f;
constexpr float kMostlyFillViewportThresholdOfVisibleProportion = 0.75f;

}  // anonymous namespace

MediaCustomControlsFullscreenDetector::MediaCustomControlsFullscreenDetector(
    HTMLVideoElement& video)
    : video_element_(video),
      viewport_intersection_observer_(nullptr),
      check_viewport_intersection_timer_(
          video.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaCustomControlsFullscreenDetector::
              OnCheckViewportIntersectionTimerFired) {
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
  viewport_intersection_observer_ = IntersectionObserver::Create(
      {}, {}, &(video_element_->GetDocument()),
      WTF::BindRepeating(
          &MediaCustomControlsFullscreenDetector::OnIntersectionChanged,
          WrapWeakPersistent(this)),
      IntersectionObserver::kDeliverDuringPostLifecycleSteps,
      IntersectionObserver::kFractionOfTarget, 0, false, true);
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
  check_viewport_intersection_timer_.Stop();

  VideoElement().SetIsEffectivelyFullscreen(
      WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
}

bool MediaCustomControlsFullscreenDetector::ComputeIsDominantVideoForTests(
    const IntSize& target_size,
    const IntSize& root_size,
    const IntSize& intersection_size) {
  if (target_size.IsEmpty() || root_size.IsEmpty())
    return false;

  const float x_occupation_proportion =
      1.0f * intersection_size.Width() / root_size.Width();
  const float y_occupation_proportion =
      1.0f * intersection_size.Height() / root_size.Height();

  // If the viewport is mostly occupied by the video, return true.
  if (std::min(x_occupation_proportion, y_occupation_proportion) >=
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return true;
  }

  // If neither of the dimensions of the viewport is mostly occupied by the
  // video, return false.
  if (std::max(x_occupation_proportion, y_occupation_proportion) <
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return false;
  }

  // If the video is mostly visible in the indominant dimension, return true.
  // Otherwise return false.
  if (x_occupation_proportion > y_occupation_proportion) {
    return target_size.Height() *
               kMostlyFillViewportThresholdOfVisibleProportion <
           intersection_size.Height();
  }
  return target_size.Width() * kMostlyFillViewportThresholdOfVisibleProportion <
         intersection_size.Width();
}

void MediaCustomControlsFullscreenDetector::Invoke(ExecutionContext* context,
                                                   Event* event) {
  DCHECK(event->type() == event_type_names::kLoadedmetadata ||
         event->type() == event_type_names::kWebkitfullscreenchange ||
         event->type() == event_type_names::kFullscreenchange);

  // Video is not loaded yet.
  if (VideoElement().getReadyState() < HTMLMediaElement::kHaveMetadata)
    return;

  if (!VideoElement().isConnected() || !IsVideoOrParentFullscreen()) {
    check_viewport_intersection_timer_.Stop();

    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
    return;
  }

  check_viewport_intersection_timer_.StartOneShot(kCheckFullscreenInterval,
                                                  FROM_HERE);
}

void MediaCustomControlsFullscreenDetector::ContextDestroyed() {
  Detach();
}

void MediaCustomControlsFullscreenDetector::
    OnCheckViewportIntersectionTimerFired(TimerBase*) {
  DCHECK(IsVideoOrParentFullscreen());
  if (viewport_intersection_observer_)
    viewport_intersection_observer_->observe(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  if (!viewport_intersection_observer_ || !VideoElement().GetLayoutObject())
    return;

  // We only want a single notification, then stop observing.
  viewport_intersection_observer_->disconnect();

  const IntersectionGeometry& geometry = entries.back()->GetGeometry();

  // Target and intersection rects must be converted from CSS to device pixels.
  float zoom = VideoElement().GetLayoutObject()->StyleRef().EffectiveZoom();
  PhysicalSize target_size = geometry.TargetRect().size;
  target_size.Scale(zoom);
  PhysicalSize intersection_size = geometry.IntersectionRect().size;
  intersection_size.Scale(zoom);
  PhysicalSize root_size = geometry.RootRect().size;

  bool is_dominant = ComputeIsDominantVideoForTests(
      RoundedIntSize(target_size), RoundedIntSize(root_size),
      RoundedIntSize(intersection_size));

  if (!is_dominant) {
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

bool MediaCustomControlsFullscreenDetector::IsVideoOrParentFullscreen() {
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(VideoElement().GetDocument());
  if (!fullscreen_element)
    return false;

  return fullscreen_element->contains(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Trace(Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(viewport_intersection_observer_);
}

}  // namespace blink
