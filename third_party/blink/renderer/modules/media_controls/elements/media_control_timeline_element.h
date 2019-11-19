// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TIMELINE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TIMELINE_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_slider_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_metrics.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class Event;
class HTMLDivElement;
class MediaControlsImpl;

class MediaControlTimelineElement : public MediaControlSliderElement {
 public:
  MODULES_EXPORT explicit MediaControlTimelineElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;

  // FIXME: An "earliest possible position" will be needed once that concept
  // is supported by HTMLMediaElement, see https://crbug.com/137275
  void SetPosition(double);
  void SetDuration(double);

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(*event); }

  void RenderBarSegments();

  // Inform the timeline that the Media Controls have been shown or hidden.
  void OnControlsShown();
  void OnControlsHidden();

  void Trace(blink::Visitor*) override;

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event&) override;
  bool KeepEventInNode(const Event&) const override;

  // Checks if we can begin or end a scrubbing event. If the event is a pointer
  // event then it needs to start and end with valid pointer events. If the
  // event is a pointer event followed by a touch event then it can only be
  // ended when the touch has ended.
  bool BeginScrubbingEvent(Event&);
  bool EndScrubbingEvent(Event&);

  MediaControlTimelineMetrics metrics_;

  bool is_touching_ = false;

  bool controls_hidden_ = false;
};

}  // namespace blink

#endif  // MediaControlTimelineElement
