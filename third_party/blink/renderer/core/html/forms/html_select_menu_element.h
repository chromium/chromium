// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"

namespace blink {

class Document;

// The HTMLSelectMenuElement implements the <selectmenu> HTML element.
// The <selectmenu> element is similar to <select>, but allows site authors
// freedom to customize the element's appearance and shadow DOM structure.
// This feature is still under development, and is not part of the HTML
// standard. It can be enabled by passing
// --enable-blink-features=HTMLSelectMenuElement. See
// https://groups.google.com/u/1/a/chromium.org/g/blink-dev/c/9TcfjaOs5zg/m/WAiv6WpUAAAJ
// for more details.
class CORE_EXPORT HTMLSelectMenuElement final
    : public HTMLFormControlElementWithState {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSelectMenuElement(Document&);

  HTMLOptionElement* selectedOption() const;
  String value() const;
  void setValue(const String&, bool send_events = false);
  bool open() const;

  // For ValidityState
  String validationMessage() const override;
  bool ValueMissing() const override;

  void ResetImpl() override;

  void Trace(Visitor*) const override;

  enum class PartType { kNone, kButton, kListBox, kOption };

  // If node is a flat tree descendant of an HTMLSelectMenuElement
  // and is registered as a part of that HTMLSelectMenuElement,
  // returns that HTMLSelectMenuElement. Else returns null.
  static HTMLSelectMenuElement* OwnerSelectMenu(Node* node);

  // For use in the implementation of HTMLOptionElement.
  void OptionSelectionStateChanged(HTMLOptionElement*, bool option_is_selected);
  void OptionElementChildrenChanged(const HTMLOptionElement& option);
  void OptionElementValueChanged(const HTMLOptionElement& option);

  PartType AssignedPartType(Node* node) const;

  Element* ButtonPart() const { return button_part_; }

 private:
  class SelectMutationCallback;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void DidMoveToNewDocument(Document& old_document) override;
  void OpenListbox();
  void CloseListbox();

  HTMLOptionElement* FirstOptionPart() const;
  Element* FirstValidButtonPart() const;
  Element* FirstValidListboxPart() const;
  Element* FirstValidSelectedValuePart() const;
  void EnsureButtonPartIsValid();
  void EnsureSelectedValuePartIsValid();
  void EnsureListboxPartIsValid();
  void SetSelectedOption(HTMLOptionElement* selected_option);
  void SelectNextOption();
  void SelectPreviousOption();
  void UpdateSelectedValuePartContents();

  void ButtonPartInserted(Element*);
  void ButtonPartRemoved(Element*);
  void UpdateButtonPart();
  void SelectedValuePartInserted(Element*);
  void SelectedValuePartRemoved(Element*);
  void UpdateSelectedValuePart();
  void ListboxPartInserted(Element*);
  void ListboxPartRemoved(Element*);
  void UpdateListboxPart();
  void OptionPartInserted(HTMLOptionElement*);
  void OptionPartRemoved(HTMLOptionElement*);
  void ResetOptionParts();
  void ResetToDefaultSelection();
  void DispatchInputAndChangeEventsIfNeeded();
  void DispatchInputEvent();
  void DispatchChangeEvent();

  bool IsValidButtonPart(const Node* node, bool show_warning) const;
  bool IsValidListboxPart(const Node* node, bool show_warning) const;
  bool IsValidOptionPart(const Node* node, bool show_warning) const;

  void SetButtonPart(Element* new_button_part);
  void SetListboxPart(HTMLPopupElement* new_listbox_part);

  bool IsRequiredFormControl() const override;
  bool IsOptionalFormControl() const override;

  bool IsLabelable() const override;

  // HTMLFormControlElementWithState overrides:
  const AtomicString& FormControlType() const override;
  bool MayTriggerVirtualKeyboard() const override;
  bool AlwaysCreateUserAgentShadowRoot() const override { return false; }
  void AppendToFormData(FormData&) override;
  bool SupportsFocus() const override { return HTMLElement::SupportsFocus(); }
  FormControlState SaveFormControlState() const override;
  void RestoreFormControlState(const FormControlState&) override;

  class ButtonPartEventListener : public NativeEventListener {
   public:
    explicit ButtonPartEventListener(HTMLSelectMenuElement* select_menu_element)
        : select_menu_element_(select_menu_element) {}
    void Invoke(ExecutionContext*, Event*) override;

    void Trace(Visitor* visitor) const override {
      visitor->Trace(select_menu_element_);
      NativeEventListener::Trace(visitor);
    }

   private:
    Member<HTMLSelectMenuElement> select_menu_element_;
  };

  class OptionPartEventListener : public NativeEventListener {
   public:
    explicit OptionPartEventListener(HTMLSelectMenuElement* select_menu_element)
        : select_menu_element_(select_menu_element) {}
    void Invoke(ExecutionContext*, Event*) override;

    void Trace(Visitor* visitor) const override {
      visitor->Trace(select_menu_element_);
      NativeEventListener::Trace(visitor);
    }

   private:
    Member<HTMLSelectMenuElement> select_menu_element_;
  };

  static constexpr char kButtonPartName[] = "button";
  static constexpr char kSelectedValuePartName[] = "selected-value";
  static constexpr char kListboxPartName[] = "listbox";

  Member<ButtonPartEventListener> button_part_listener_;
  Member<OptionPartEventListener> option_part_listener_;

  Member<SelectMutationCallback> select_mutation_callback_;

  Member<Element> button_part_;
  Member<Element> selected_value_part_;
  Member<HTMLPopupElement> listbox_part_;
  HeapLinkedHashSet<Member<HTMLOptionElement>> option_parts_;
  Member<HTMLSlotElement> button_slot_;
  Member<HTMLSlotElement> listbox_slot_;
  Member<HTMLOptionElement> selected_option_;
  Member<HTMLOptionElement> selected_option_when_listbox_opened_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
