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
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class ExceptionState;
class HTMLDataListElement;
class HTMLSelectElement;
class OptionTextObserver;

class CORE_EXPORT HTMLOptionElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLOptionElement* CreateForJSConstructor(
      Document& document,
      const String& data,
      ExceptionState& exception_state) {
    return CreateForJSConstructor(document, data, AtomicString(), false, false,
                                  exception_state);
  }
  static HTMLOptionElement* CreateForJSConstructor(Document&,
                                                   const String& data,
                                                   const AtomicString& value,
                                                   bool default_selected,
                                                   bool selected,
                                                   ExceptionState&);

  explicit HTMLOptionElement(Document&);
  ~HTMLOptionElement() override;
  void Trace(Visitor* visitor) const override;

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
  void DefaultEventHandler(Event&) override;

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

  void SetWasOptionInsertedCalled(bool flag) {
    was_option_inserted_called_ = flag;
  }
  bool WasOptionInsertedCalled() const { return was_option_inserted_called_; }

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  // These methods mutate the shadowroot to switch between rendering all
  // children or only text content. SetTextOnlyRendering is used for
  // appearance:base-select. The mechanism by which these methods render all
  // children or only text content is that the UA shadowroot has a manually
  // updated text node for text-only mode or a slot element which just slots all
  // nodes into it for the render everything mode. SetTextOnlyRendering switches
  // the ShadowRoot state based on the provided argument.
  void SetTextOnlyRendering(bool);

  // Callback for OptionTextObserver.
  void DidChangeTextContent();

  bool IsRichlyEditableForAccessibility() const override { return false; }

 private:
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  void AccessKeyAction(SimulatedClickCreationScope) override;
  void ChildrenChanged(const ChildrenChange&) override;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;

  String CollectOptionInnerText() const;

  void UpdateLabel();

  Member<OptionTextObserver> text_observer_;

  // Represents 'selectedness'.
  // https://html.spec.whatwg.org/C/#concept-option-selectedness
  bool is_selected_;
  // Represents 'dirtiness'.
  // https://html.spec.whatwg.org/C/#concept-option-dirtiness
  bool is_dirty_ = false;
  // Represents the option being focused on in a multi-select non-contiguous
  // traversal via the keyboard.
  bool is_multi_select_focused_ = false;

  // True while HTMLSelectElement::OptionInserted(this) and OptionRemoved(this);
  // This flag is necessary to detect a state where DOM tree is updated and
  // OptionInserted() is not called yet.
  bool was_option_inserted_called_ = false;

  friend class HTMLOptionElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPTION_ELEMENT_H_
