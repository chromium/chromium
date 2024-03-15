// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_VIDEO_VISIBILITY_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_VIDEO_VISIBILITY_TRACKER_H_

#include "base/functional/callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"

namespace blink {

class Document;
class HTMLVideoElement;

// This class tracks the percentage of an HTMLVideoElement that is visible to
// the user (visibility percentage) and reports whether the element's visibility
// is greater or equal than a given threshold (|visibility_threshold_|) or not.
//
// "Visible" in this context is defined as intersecting with the viewport and
// not occluded by other html elements within the page, with the exception of
// MediaControls.
class CORE_EXPORT MediaVideoVisibilityTracker final
    : public GarbageCollected<MediaVideoVisibilityTracker>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  static constexpr base::TimeDelta kMinimumAllowedHitTestInterval =
      base::Milliseconds(500);

  using ReportVisibilityCb = base::RepeatingCallback<void(bool)>;
  using TrackerAttachedToDocument = WeakMember<Document>;

  MediaVideoVisibilityTracker(
      HTMLVideoElement& video,
      float visibility_threshold,
      ReportVisibilityCb report_visibility_cb,
      base::TimeDelta hit_test_interval = kMinimumAllowedHitTestInterval);
  ~MediaVideoVisibilityTracker();

  // Updates the visibility tracker state by attaching/detaching the tracker as
  // needed. It is safe to call this method regardless of whether the tracker is
  // already attached/detached.
  void UpdateVisibilityTrackerState();

  // Called by the |HTMLVideoElement| |DidMoveToNewDocument| method to detach
  // the visibility tracker.
  void ElementDidMoveToNewDocument();
  void Trace(Visitor*) const override;

 private:
  // Friend class for testing.
  friend class MediaVideoVisibilityTrackerTest;
  friend class HTMLMediaElementTest;

  HTMLVideoElement& VideoElement() const { return *video_element_; }

  // Registers the tracker for lifecycle notifications.
  void Attach();
  void Detach();

  ListBasedHitTestBehavior ComputeOcclusion(const Node& node);
  bool MeetsVisibilityThreshold(const PhysicalRect& rect);
  void ReportVisibility(bool meets_visibility_threshold);
  void OnIntersectionChanged();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  // `video_element_` creates |this|.
  Member<HTMLVideoElement> video_element_;

  // Threshold used to report whether a video element is sufficiently visible or
  // not. A video element with visibility greater or equal than
  // |visibility_threshold_| is considered to meet the visibility threshold.
  float visibility_threshold_ = 1.0;
  float accumulated_area_ = 0.0;
  float video_element_area_ = 0.0;
  VectorOf<PhysicalRect> occluding_rects_;
  PhysicalRect intersection_rect_;
  ReportVisibilityCb report_visibility_cb_;
  base::TimeTicks last_hit_test_timestamp_;
  const base::TimeDelta hit_test_interval_;

  // Keeps track of the |Document| to which the tracker has registered for
  // lifecycle notifications.
  TrackerAttachedToDocument tracker_attached_to_document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_VIDEO_VISIBILITY_TRACKER_H_
