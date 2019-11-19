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

  // Callback run when an item is selected from the popup menu.
  virtual void OnItemSelected();

  // Node override.
  void DefaultEventHandler(Event&) override;
  bool KeepEventInNode(const Event&) const override;
  void RemovedFrom(ContainerNode&) override;

  void Trace(blink::Visitor*) override;

  // When clicking the scroll bar, chrome will find its first focusable parent
  // and focus on it. In order to prevent popup menu from losing focus (which
  // will close the menu), we are setting the popup menu support focus and mouse
  // focusable.
  bool IsMouseFocusable() const override { return true; }
  bool SupportsFocus() const override { return true; }

 protected:
  MediaControlPopupMenuElement(MediaControlsImpl&);

  void SetPosition();

 private:
  class EventListener;

  Element* PopupAnchor() const;

  void HideIfNotFocused();

  bool FocusListItemIfDisplayed(Node* node);
  void SelectFirstItem();

  // Actions called by the EventListener object when specific evenst are
  // received.
  void SelectNextItem();
  void SelectPreviousitem();
  void CloseFromKeyboard();

  Member<EventListener> event_listener_;
  Member<Element> last_focused_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_POPUP_MENU_ELEMENT_H_
