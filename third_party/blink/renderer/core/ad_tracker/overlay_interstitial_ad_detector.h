// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class Element;
class LocalFrame;

// Detects overlay interstitials and records a use counter when an instance is
// found. Two kinds are considered: pop-ups, which block the main content of
// the page, and prestitials, a subset of pop-ups that appear while the page
// is still loading and before the user has interacted with it.
//
// Better Ads Standards definitions:
// https://www.betterads.org/desktop-pop-up-ad/
// https://www.betterads.org/mobile-pop-up-ad/
// https://www.betterads.org/desktop-prestitial-ad/
// https://www.betterads.org/mobile-prestitial-ad/
//
// Heuristic:
// We do hit testing at the center of the browser viewport at regular intervals,
// starting from the first contentful paint. The top element is an interstitial
// pop-up candidate if the following conditions are met:
// 1) It's immobile to scrolling (e.g. position:fixed).
// 2) The size is large.
// 3) It's created without user gesture.
//
// The candidate will be actually counted as an overlay pop-up instance after we
// have checked some status at its dismissal time. On dismissal, if the main
// frame scrolling offset hasn't changed since the candidate's appearance, we
// count it as an overlay pop-up; otherwise, we skip that candidate because it
// could be a parallax/scroller ad.
//
// A pop-up ad is additionally counted as a prestitial ad if, when it appeared:
// 1) It was within `kPrestitialWindow` of the first contentful paint.
// 2) The page hadn't been hidden since navigation. Rendering is throttled
//    while a page is hidden, so its paint timings would no longer reflect what
//    the user saw.
// 3) The user hadn't interacted with the page: the main frame had no sticky
//    user activation and the user hadn't scrolled.
//
// Besides, we explicitly prevent mid-roll ads (during a video play) from being
// categorized as pop-ups.
//
// We could potentially miss some true positive cases: the user could click at
// an empty space which activates the user gesture, and coincidentally the
// pop-up automatically shows up; the user could make some scrolling
// before closing the pop-up; etc. However, we accept the trade-off exchanging a
// lower rate of false positive for an increase in the rate of false negatives.
class CORE_EXPORT OverlayInterstitialAdDetector {
 public:
  // A pop-up ad appearing later than this after the first contentful paint is
  // not considered a prestitial.
  static constexpr base::TimeDelta kPrestitialWindow = base::Seconds(10);

  OverlayInterstitialAdDetector() = default;
  OverlayInterstitialAdDetector(const OverlayInterstitialAdDetector&) = delete;
  OverlayInterstitialAdDetector& operator=(
      const OverlayInterstitialAdDetector&) = delete;
  ~OverlayInterstitialAdDetector() = default;

  void MaybeFireDetection(LocalFrame* outermost_main_frame);

 private:
  // Records the detection of an overlay pop-up. If `is_ad` is true, the
  // pop-up is also counted as an overlay pop-up ad and `ad_element` (if
  // non-null) is tagged as such for the ad highlight label. `ad_element` may be
  // null if the element is no longer alive at detection time. If
  // `is_prestitial_ad` is also true, i.e. the pop-up met the prestitial
  // conditions when it appeared, it is further counted as a prestitial ad.
  void OnPopupDetected(LocalFrame* outermost_main_frame,
                       bool is_ad,
                       bool is_prestitial_ad,
                       Element* ad_element);

  bool started_detection_ = false;

  // Set when the viewport size has changed since the last detection, so that
  // the element then found at the viewport center is marked unqualified rather
  // than considered as a candidate.
  bool viewport_size_changed_ = false;

  // The following members are valid only when |started_detection_| is true.
  base::Time last_detection_time_;
  gfx::Size last_detection_outermost_main_frame_size_;

  DOMNodeId candidate_id_ = kInvalidDOMNodeId;
  bool candidate_is_ad_ = false;

  // The following members are valid only when |candidate_id_| is valid.
  int candidate_start_outermost_main_frame_scroll_position_ = 0;

  // Whether the candidate met the prestitial conditions when it appeared. See
  // the class comment.
  bool candidate_is_prestitial_ = false;

  // The node id of the last element that was detected as unqualified to be an
  // overlay pop-up. We compare any potential candidate with the last
  // unqualified element and skip it if they are equal.
  //
  // It allows us to exclude some false positive cases. e.g. an
  // overlay was excluded from the initial consideration because it was created
  // with a gesture. After 5 seconds the gesture would be gone, but we still
  // want to exclude it as it was originally created with a gesture.
  //
  // Another advantage is this saves some computation cost. e.g. if an ad was
  // unqualified because it didn't have a viewport constraint position, then we
  // can skip it on its next occurrence without computing the style again.
  DOMNodeId last_unqualified_element_id_ = kInvalidDOMNodeId;

  bool popup_detected_ = false;
  bool popup_ad_detected_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_
