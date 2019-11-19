// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_BROWSER_CONTROLS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_BROWSER_CONTROLS_H_

#include "cc/input/browser_controls_state.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {
class Page;
class FloatSize;

// This class encapsulate data and logic required to show/hide browser controls
// duplicating cc::BrowserControlsOffsetManager behaviour.  Browser controls'
// self-animation to completion is still handled by compositor and kicks in
// when scrolling is complete (i.e, upon ScrollEnd or FlingEnd). Browser
// controls can exist at the top or bottom of the screen and potentially at the
// same time. Bottom controls differ from top in that, if they exist alone,
// never translate the content down and scroll immediately, regardless of the
// controls' offset.
class CORE_EXPORT BrowserControls final
    : public GarbageCollected<BrowserControls> {
 public:
  explicit BrowserControls(const Page&);

  void Trace(blink::Visitor*);

  // The height the top controls are hidden; used for viewport adjustments
  // while the controls are resizing.
  float UnreportedSizeAdjustment();
  // The amount that browser controls are currently shown.
  float ContentOffset();

  float TopHeight() const { return top_height_; }
  float BottomHeight() const { return bottom_height_; }
  float TotalHeight() const { return top_height_ + bottom_height_; }
  bool ShrinkViewport() const { return shrink_viewport_; }
  void SetHeight(float top_height, float bottom_height, bool shrink_viewport);

  float TopShownRatio() const { return top_shown_ratio_; }
  float BottomShownRatio() const { return bottom_shown_ratio_; }
  void SetShownRatio(float top_ratio, float bottom_ratio);

  void UpdateConstraintsAndState(cc::BrowserControlsState constraints,
                                 cc::BrowserControlsState current,
                                 bool animate);

  void ScrollBegin();

  // Scrolls browser controls vertically if possible and returns the remaining
  // scroll amount.
  FloatSize ScrollBy(FloatSize scroll_delta);

  cc::BrowserControlsState PermittedState() const { return permitted_state_; }

 private:
  void ResetBaseline();
  float BottomContentOffset();

  Member<const Page> page_;

  // The browser controls height regardless of whether it is visible or not.
  float top_height_;
  float bottom_height_;

  // The browser controls shown amount (normalized from 0 to 1) since the last
  // compositor commit. This value is updated from two sources:
  // (1) compositor (impl) thread at the beginning of frame if it has
  //     scrolled browser controls since last commit.
  // (2) blink (main) thread updates this value if it scrolls browser controls
  //     when responding to gesture scroll events.
  // This value is reflected in web layer tree and is synced with compositor
  // during the commit.
  float top_shown_ratio_;
  float bottom_shown_ratio_;

  // Content offset when last re-baseline occurred.
  float baseline_content_offset_;

  // Accumulated scroll delta since last re-baseline.
  float accumulated_scroll_delta_;

  // If this is true, then the embedder shrunk the WebView size by the top
  // controls height.
  bool shrink_viewport_;

  // Constraints on the browser controls state
  cc::BrowserControlsState permitted_state_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_BROWSER_CONTROLS_H_
