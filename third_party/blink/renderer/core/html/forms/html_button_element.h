/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_BUTTON_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_BUTTON_ELEMENT_H_

#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

namespace blink {

class HTMLButtonElement final : public HTMLFormControlElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLButtonElement(Document&);

  void setType(const AtomicString&);

  const AtomicString& Value() const;

  bool WillRespondToMouseClickEvents() override;

 private:
  enum Type { SUBMIT, RESET, BUTTON };

  const AtomicString& FormControlType() const override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;

  // HTMLFormControlElement always creates one, but buttons don't need it.
  bool AlwaysCreateUserAgentShadowRoot() const override { return false; }

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void DefaultEventHandler(Event&) override;
  bool HasActivationBehavior() const override;

  void AppendToFormData(FormData&) override;

  bool IsEnumeratable() const override { return true; }
  bool IsLabelable() const override { return true; }
  bool TypeShouldForceLegacyLayout() const final { return true; }
  bool IsInteractiveContent() const override;
  bool MatchesDefaultPseudoClass() const override;

  bool CanBeSuccessfulSubmitButton() const override;
  bool IsActivatedSubmit() const override;
  void SetActivatedSubmit(bool flag) override;

  void AccessKeyAction(bool send_mouse_events) override;
  bool IsURLAttribute(const Attribute&) const override;

  bool CanStartSelection() const override { return false; }

  bool IsOptionalFormControl() const override { return true; }
  bool RecalcWillValidate() const override;

  int DefaultTabIndex() const override;

  // TODO(crbug.com/1013385): Remove PreDispatchEventHandler, DidPreventDefault,
  //   and DefaultEventHandlerInternal. They are here to temporarily fix form
  //   double-submit.
  EventDispatchHandlingState* PreDispatchEventHandler(Event&) override;
  void DidPreventDefault(const Event&) final;
  void DefaultEventHandlerInternal(Event&);

  Type type_;
  bool is_activated_submit_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_BUTTON_ELEMENT_H_
