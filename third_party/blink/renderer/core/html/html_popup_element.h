// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class Document;

// The HTMLPopupElement implements the <popup> HTML element. The popup element
// can be used to construct a topmost popup dialog. This feature is still
// under development, and is not part of the HTML standard. It can be enabled
// by passing --enable-blink-features=HTMLPopupElement. See
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/Popup/explainer.md
// for more details.
class HTMLPopupElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLPopupElement(Document&);

  bool open() const;
  void hide();
  void show();

  Element* AnchorElement() const;

  // This is used by invoking elements (which have a "popup" attribute)
  // to invoke the popup.
  void Invoke(Element* invoker);

  static void HandleLightDismiss(const Event&);

  void Trace(Visitor*) const override;

 private:
  void ScheduleHideEvent();
  void MarkStyleDirty();

  void PushNewPopupElement(HTMLPopupElement*);
  void PopPopupElement(HTMLPopupElement*);
  HTMLPopupElement* TopmostPopupElement();

  static const HTMLPopupElement* NearestOpenAncestralPopup(Node*);

  bool open_;
  WeakMember<Element> invoker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_
