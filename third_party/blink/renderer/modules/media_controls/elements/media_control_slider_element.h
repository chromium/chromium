// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_SLIDER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_SLIDER_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_input_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class Element;
class MediaControlsImpl;
class ResizeObserver;

// MediaControlInputElement with additional logic for sliders.
class MODULES_EXPORT MediaControlSliderElement
    : public MediaControlInputElement {
 public:
  void Trace(Visitor*) const override;

  // Width in CSS pixels * layoutZoomFactor (ignores CSS transforms for
  // simplicity; deliberately ignores pinch zoom's pageScaleFactor).
  int TrackWidth();

  void OnControlsShown();
  void OnControlsHidden();

 protected:
  friend class MediaControlsImplTest;

  class MediaControlSliderElementResizeObserverDelegate;

  MediaControlSliderElement(MediaControlsImpl&);

  void SetupBarSegments();
  void SetBeforeSegmentFraction(double fraction);
  void SetAfterSegmentFraction(double fraction);

  void NotifyElementSizeChanged();

  Element& GetTrackElement();

  float ZoomFactor() const;

 private:
  double before_segment_fraction_ = 0.0;
  double after_segment_fraction_ = 0.0;

  Member<HTMLDivElement> segment_highlight_before_;
  Member<HTMLDivElement> segment_highlight_after_;

  Member<ResizeObserver> resize_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_SLIDER_ELEMENT_H_
