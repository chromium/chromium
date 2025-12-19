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
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

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

  HTMLSelectElement* OwnerSelectElement() const {
    return nearest_ancestor_select_;
  }

  String label() const;
  void setLabel(const AtomicString&);

  bool OwnElementDisabled() const;

  bool IsDisabledFormControl() const override;
  String DefaultToolTip() const override;
  void DefaultEventHandler(Event&) override;

  String TextIndentedToRespectGroupLabel() const;

  // Update 'selectedness'.
  // Setting skip_mutation_observer_update to true prevents this method from
  // calling UpdateMutationObserver.
  void SetSelectedState(bool selected,
                        bool skip_mutation_observer_update = false);
  // Update 'dirtiness'.
  void SetDirty(bool);

  HTMLElement* formForBinding() const override;
  bool SpatialNavigationFocused() const;

  bool IsDisplayNone(bool ensure_style);

  int ListIndex() const;

  void SetMultiSelectFocusedState(bool);
  bool IsMultiSelectFocused() const;

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void FinishParsingChildren() override;

  // Callback for OptionTextObserver.
  void DidChangeTextContent();

  bool IsRichlyEditableForAccessibility() const override { return false; }

  // This method returns true if the provided element is the label_container_ of
  // an HTMLOptionElement.
  static bool IsLabelContainerElement(const Element& element);

  void UpdateMutationObserver(bool in_style_recalc);
  bool HasMutationObserver() const { return text_observer_; }

 private:
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;
  bool IsKeyboardFocusableSlow(UpdateBehavior update_behavior) const override;
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  void AccessKeyAction(SimulatedClickCreationScope) override;
  void ChildrenChanged(const ChildrenChange&) override;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;

  String CollectOptionInnerText() const;

  void UpdateLabel();

  void DefaultEventHandlerInternal(Event&);

  void RecalcOwnerSelectElement() const;

  // Helper to choose the option for customizable select event handling in
  // DefaultEventHandler. Depending on the state of OwnerSelectElement, it may
  // toggle selectedness and dirtiness, deselect other options, close the
  // select's picker, and set default handled on the event.
  void ChooseOption(Event&);

  bool IsVisibleInViewport();
  bool NeedsMutationObserver();

  Member<OptionTextObserver> text_observer_;

  // The closest ancestor <select> in the DOM tree, without crossing any shadow
  // boundaries. This is cached as a performance optimization for
  // OwnerSelectElement(), and is kept up to date in InsertedInto() and
  // RemovedFrom(). Because it is only updated in InsertedInto and RemovedFrom,
  // there may be times where it isn't up to date with the actual nearest
  // ancestor select in the DOM, such as in HTMLOptionElement::ChildrenChanged
  // before InsertedInto gets called.
  // TODO(crbug.com/1511354): Consider using a flat tree traversal here
  // instead of a node traversal. That would probably also require changing
  // HTMLOptionsCollection to support flat tree traversals as well.
  Member<HTMLSelectElement> nearest_ancestor_select_;

  // The closest ancestor <optgroup> in the DOM tree. This is created and
  // maintained just like nearest_ancestor_select_, but doesn't account for any
  // <optgroup> element ancestor above nearest_ancestor_select_.
  Member<HTMLOptGroupElement> nearest_ancestor_optgroup_;

  // label_container_ contains the text content of DisplayLabel(). Based on UA
  // style rules, it is rendered when this option is not inside of a select
  // element with appearance:base-select.
  Member<HTMLElement> label_container_;

  // DidChangeTextContent and UpdateLabel may update copies of the text content
  // of this element into the UA ShadowRoot of this element and the nearest
  // ancestor select element. This may be triggered as the result of the
  // appearance of the nearest ancestor select element switching from base
  // appearance to auto appearance, in which case we can't update the DOM yet
  // because we are in style recalc. In this case, update_label_task_ holds a
  // task to call DidChangeTextContent after style recalc is done.
  TaskHandle update_label_task_;

  // Represents 'selectedness'.
  // https://html.spec.whatwg.org/C/#concept-option-selectedness
  bool is_selected_;
  // Represents 'dirtiness'.
  // https://html.spec.whatwg.org/C/#concept-option-dirtiness
  bool is_dirty_ = false;
  // Represents the option being focused on in a multi-select non-contiguous
  // traversal via the keyboard.
  bool is_multi_select_focused_ = false;
  // Gets set to true when a child element is inserted into this option
  // element. Never gets set back to false once set to true.
  bool was_element_inserted_ = false;

  friend class HTMLOptionElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPTION_ELEMENT_H_
