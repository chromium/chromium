// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_POPUP_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_POPUP_MENU_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"

namespace blink {

class MediaControlsImpl;

class MediaControlPopupMenuElement : public MediaControlDivElement {
 public:
  ~MediaControlPopupMenuElement() override;

  void SetIsWanted(bool) override;

  // Only use when !IsModern to get the element that should be used an as anchor
  // for the popup.
  virtual Element* PopupAnchor() const = 0;

  // Callback run when an item is selected from the popup menu.
  virtual void OnItemSelected();

  // Node override.
  void DefaultEventHandler(Event&) override;
  bool KeepEventInNode(const Event&) const override;
  void RemovedFrom(ContainerNode&) override;

  void Trace(blink::Visitor*) override;

 protected:
  MediaControlPopupMenuElement(MediaControlsImpl&, MediaControlElementType);

  void SetPosition();

 private:
  class EventListener;

  Element* EffectivePopupAnchor() const;

  void HideIfNotFocused();

  void SelectFirstItem();

  // Actions called by the EventListener object when specific evenst are
  // received.
  void SelectNextItem();
  void SelectPreviousitem();
  void CloseFromKeyboard();

  Member<EventListener> event_listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_POPUP_MENU_ELEMENT_H_
