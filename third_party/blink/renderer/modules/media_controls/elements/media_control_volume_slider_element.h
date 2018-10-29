// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_VOLUME_SLIDER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_VOLUME_SLIDER_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_slider_element.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlVolumeSliderElement final : public MediaControlSliderElement {
 public:
  explicit MediaControlVolumeSliderElement(MediaControlsImpl&);

  // TODO: who calls this?
  void SetVolume(double);

  void OpenSlider();
  void CloseSlider();

  // MediaControlInputElement overrides.
  bool WillRespondToMouseMoveEvents() override;
  bool WillRespondToMouseClickEvents() override;

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(*event); }

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event&) override;
  bool KeepEventInNode(const Event&) const override;
  void SetVolumeInternal(double);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_VOLUME_SLIDER_ELEMENT_H_
