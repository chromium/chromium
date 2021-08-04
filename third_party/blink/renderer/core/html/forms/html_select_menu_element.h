// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/html/html_element.h"

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
class HTMLSelectMenuElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSelectMenuElement(Document&);

  String value();
  void setValue(const String&, bool send_events = false);
  bool open() const;

  void Trace(Visitor*) const override;

 private:
  class SelectMutationCallback;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void OpenListbox();
  void CloseListbox();
  void UpdatePartElements();

  Element* FirstOptionPart() const;
  Element* FirstValidButtonPart() const;
  Element* FirstValidListboxPart() const;
  Element* FirstValidSelectedValuePart() const;
  HTMLSlotElement* ButtonSlot() const;
  HTMLSlotElement* ListboxSlot() const;
  void EnsureSelectedOptionIsValid();
  Element* SelectedOption();
  void SetSelectedOption(Element* selected_option);
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
  void OptionPartInserted(Element*);
  void OptionPartRemoved(Element*);

  bool IsValidButtonPart(const Element* part, bool show_warning) const;
  bool IsValidListboxPart(const Element* part, bool show_warning) const;
  bool IsValidOptionPart(const Element* part, bool show_warning) const;

  void SetButtonPart(Element* new_button_part);
  void SetListboxPart(HTMLPopupElement* new_listbox_part);

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

  class SlotChangeEventListener : public NativeEventListener {
   public:
    explicit SlotChangeEventListener(HTMLSelectMenuElement* select_menu_element)
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
  static constexpr char kOptionPartName[] = "option";

  Member<ButtonPartEventListener> button_part_listener_;
  Member<OptionPartEventListener> option_part_listener_;
  Member<SlotChangeEventListener> slotchange_listener_;

  Member<SelectMutationCallback> select_mutation_callback_;

  Member<Element> button_part_;
  Member<Element> selected_value_part_;
  Member<HTMLPopupElement> listbox_part_;
  HeapLinkedHashSet<Member<Element>> option_parts_;
  Member<HTMLSlotElement> button_slot_;
  Member<HTMLSlotElement> listbox_slot_;
  Member<Element> selected_option_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
