/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPTION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPTION_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class ExceptionState;
class HTMLDataListElement;
class HTMLSelectElement;

class CORE_EXPORT HTMLOptionElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLOptionElement* CreateForJSConstructor(Document&,
                                                   const String& data,
                                                   const AtomicString& value,
                                                   bool default_selected,
                                                   bool selected,
                                                   ExceptionState&);

  explicit HTMLOptionElement(Document&);

  // A text to be shown to users.  The difference from |label()| is |label()|
  // returns an empty string if |label| content attribute is empty.
  // |displayLabel()| returns the value string in that case.
  String DisplayLabel() const;

  // |text| IDL attribute implementations.
  String text() const;
  void setText(const String&);

  int index() const;

  String value() const;
  void setValue(const AtomicString&);

  bool Selected() const;
  void SetSelected(bool);
  bool selectedForBinding() const;
  void setSelectedForBinding(bool);

  HTMLDataListElement* OwnerDataListElement() const;
  HTMLSelectElement* OwnerSelectElement() const;

  String label() const;
  void setLabel(const AtomicString&);

  bool OwnElementDisabled() const;

  bool IsDisabledFormControl() const override;
  String DefaultToolTip() const override;

  String TextIndentedToRespectGroupLabel() const;

  // Update 'selectedness'.
  void SetSelectedState(bool);
  // Update 'dirtiness'.
  void SetDirty(bool);

  HTMLFormElement* form() const;
  bool SpatialNavigationFocused() const;

  bool IsDisplayNone() const;

  int ListIndex() const;

  void SetMultiSelectFocusedState(bool);
  bool IsMultiSelectFocused() const;

 private:
  ~HTMLOptionElement() override;

  bool SupportsFocus() const override;
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void AccessKeyAction(bool) override;
  void ChildrenChanged(const ChildrenChange&) override;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;

  String CollectOptionInnerText() const;

  void UpdateLabel();

  // Represents 'selectedness'.
  // https://html.spec.whatwg.org/C/#concept-option-selectedness
  bool is_selected_;
  // Represents 'dirtiness'.
  // https://html.spec.whatwg.org/C/#concept-option-dirtiness
  bool is_dirty_ = false;
  // Represents the option being focused on in a multi-select non-contiguous
  // traversal via the keyboard.
  bool is_multi_select_focused_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPTION_ELEMENT_H_
