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

 private:
  void ScheduleHideEvent();
  void MarkStyleDirty();
  bool IsKeyboardFocusable() const override;
  bool IsMouseFocusable() const override;

  Node::InsertionNotificationRequest InsertedInto(
      ContainerNode& insertion_point) override;
  void RemovedFrom(ContainerNode&) override;
  void ParserDidSetAttributes() override;

  void PushNewPopupElement(HTMLPopupElement*);
  void PopPopupElement(HTMLPopupElement*);

  bool open_;
  bool had_initiallyopen_when_parsed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_
