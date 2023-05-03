// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_

#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"
#include "third_party/blink/renderer/core/html/forms/type_ahead.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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
    : public HTMLFormControlElementWithState,
      public LocalFrameView::LifecycleNotificationObserver,
      public TypeAheadDataSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSelectMenuElement(Document&);

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  // TypeAheadDataSource:
  int IndexOfSelectedOption() const override;
  int OptionCount() const override;
  String OptionAtIndex(int index) const override;

  HTMLOptionElement* selectedOption() const;
  String value() const;
  void setValue(const String&,
                bool send_events = false,
                WebAutofillState autofill_state = WebAutofillState::kNotFilled);
  String valueForBinding() const { return value(); }
  void setValueForBinding(const String&);
  bool open() const;

  void SetAutofillValue(const String& value, WebAutofillState autofill_state);

  // For ValidityState
  String validationMessage() const override;
  bool ValueMissing() const override;

  void CloneNonAttributePropertiesFrom(const Element&,
                                       CloneChildrenFlag) override;
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

  HTMLElement* ButtonPart() const { return button_part_; }

  bool IsRichlyEditableForAccessibility() const override { return false; }

  using ListItems = HeapVector<Member<HTMLOptionElement>>;

  // Returns list of HTMLOptionElements which are direct children of the
  // HTMLSelectMenuElement.
  // TODO(http://crbug.com/1422027): Expose iterator similar to
  // HTMLSelectElement::GetOptionList().
  const ListItems& GetListItems() const;

  void ListboxWasClosed();

  void ResetTypeAheadSessionForTesting();

 private:
  class SelectMutationCallback;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void DidMoveToNewDocument(Document& old_document) override;
  void DisabledAttributeChanged() override;
  void OpenListbox();
  void CloseListbox();
  bool TypeAheadFind(const KeyboardEvent& event, int charCode);

  HTMLOptionElement* FirstOptionPart() const;
  HTMLElement* FirstValidButtonPart() const;
  HTMLElement* FirstValidListboxPart() const;
  HTMLElement* FirstValidSelectedValuePart() const;
  void EnsureButtonPartIsValid();
  void EnsureSelectedValuePartIsValid();
  void EnsureListboxPartIsValid();
  void SetSelectedOption(HTMLOptionElement* selected_option,
                         bool send_events = false,
                         WebAutofillState = WebAutofillState::kNotFilled);
  void SelectNextOption();
  void SelectPreviousOption();
  void UpdateSelectedValuePartContents();

  void RecalcListItems() const;
  HTMLOptionElement* OptionAtListIndex(int list_index) const;

  void ButtonPartInserted(HTMLElement*);
  void ButtonPartRemoved(HTMLElement*);
  void UpdateButtonPart();
  void SelectedValuePartInserted(HTMLElement*);
  void SelectedValuePartRemoved(HTMLElement*);
  void UpdateSelectedValuePart();
  void ListboxPartInserted(HTMLElement*);
  void ListboxPartRemoved(HTMLElement*);
  void UpdateListboxPart();
  void OptionPartInserted(HTMLOptionElement*);
  void OptionPartRemoved(HTMLOptionElement*);
  void QueueCheckForMissingParts();
  void ResetOptionParts();
  void ResetToDefaultSelection();
  void DispatchInputAndChangeEventsIfNeeded();
  void DispatchInputEvent();
  void DispatchChangeEvent();

  bool IsValidButtonPart(const Node* node, bool show_warning) const;
  bool IsValidListboxPart(const Node* node, bool show_warning) const;
  bool IsValidOptionPart(const Node* node, bool show_warning) const;

  void SetButtonPart(HTMLElement* new_button_part);
  // Returns true if the listbox part actually changed to something different.
  bool SetListboxPart(HTMLElement* new_listbox_part);

  bool IsRequiredFormControl() const override;
  bool IsOptionalFormControl() const override;

  bool IsEnumeratable() const override { return true; }
  bool IsLabelable() const override;

  // HTMLFormControlElementWithState overrides:
  const AtomicString& FormControlType() const override;
  void DefaultEventHandler(Event&) override;
  bool MayTriggerVirtualKeyboard() const override;
  bool AlwaysCreateUserAgentShadowRoot() const override { return false; }
  void AppendToFormData(FormData&) override;
  bool SupportsFocus() const override { return false; }
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

    void AddEventListeners(HTMLElement* button_part);
    void RemoveEventListeners(HTMLElement* button_part);
    bool HandleKeyboardEvent(const KeyboardEvent& event);

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

    void AddEventListeners(HTMLOptionElement* option_part);
    void RemoveEventListeners(HTMLOptionElement* option_part);
    bool HandleKeyboardEvent(const KeyboardEvent& event);

   private:
    Member<HTMLSelectMenuElement> select_menu_element_;
  };

  static constexpr char kButtonPartName[] = "button";
  static constexpr char kSelectedValuePartName[] = "selected-value";
  static constexpr char kListboxPartName[] = "listbox";
  static constexpr char kMarkerPartName[] = "marker";

  TypeAhead type_ahead_;

  Member<ButtonPartEventListener> button_part_listener_;
  Member<OptionPartEventListener> option_part_listener_;

  Member<SelectMutationCallback> select_mutation_callback_;

  Member<HTMLElement> button_part_;
  Member<HTMLElement> selected_value_part_;
  Member<HTMLElement> listbox_part_;
  HeapLinkedHashSet<Member<HTMLOptionElement>> option_parts_;
  Member<HTMLSlotElement> button_slot_;
  Member<HTMLSlotElement> listbox_slot_;
  Member<HTMLSlotElement> marker_slot_;
  Member<HTMLSlotElement> selected_value_slot_;
  Member<HTMLOptionElement> selected_option_;
  Member<HTMLOptionElement> selected_option_when_listbox_opened_;
  bool queued_check_for_missing_parts_{false};

  bool should_recalc_list_items_{true};

  // Initialized lazily. Use GetListItems() to get up to date value.
  mutable ListItems list_items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_MENU_ELEMENT_H_
