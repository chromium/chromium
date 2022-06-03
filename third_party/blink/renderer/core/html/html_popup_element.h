// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
class Document;
class HTMLSelectMenuElement;

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

  Element* anchor() const;

  // This is used by invoking elements (which have a "popup" attribute)
  // to invoke the popup.
  void Invoke(Element* invoker);

  static void HandleLightDismiss(const Event&);

  // TODO(crbug.com/1197720): The popup position should be provided by the new
  // anchored positioning scheme.
  void SetNeedsRepositioningForSelectMenu(bool flag);
  bool NeedsRepositioningForSelectMenu() const;
  void SetOwnerSelectMenuElement(HTMLSelectMenuElement*);
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject(
      const StyleRecalcContext&) final;

  void Trace(Visitor*) const override;

 private:
  void ScheduleHideEvent();
  void MarkStyleDirty();
  void focus(const FocusParams& params) override;
  Element* GetFocusableArea(bool autofocus_only) const;
  void SetFocus();
  bool IsKeyboardFocusable() const override;
  bool IsMouseFocusable() const override;

  Node::InsertionNotificationRequest InsertedInto(
      ContainerNode& insertion_point) override;
  void RemovedFrom(ContainerNode&) override;
  void ParserDidSetAttributes() override;

  // TODO(crbug.com/1197720): The popup position should be provided by the new
  // anchored positioning scheme.
  void AdjustPopupPositionForSelectMenu(ComputedStyle&);

  void PushNewPopupElement(HTMLPopupElement*);
  void PopPopupElement(HTMLPopupElement*);
  HTMLPopupElement* TopmostPopupElement();

  static const HTMLPopupElement* NearestOpenAncestralPopup(Node*);

  bool open_;
  bool had_initiallyopen_when_parsed_;
  WeakMember<Element> invoker_;

  bool needs_repositioning_for_select_menu_;
  WeakMember<HTMLSelectMenuElement> owner_select_menu_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_POPUP_ELEMENT_H_
