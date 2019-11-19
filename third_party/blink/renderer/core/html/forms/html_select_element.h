/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_ELEMENT_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/type_ahead.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AutoscrollController;
class ExceptionState;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;
class HTMLOptionElementOrHTMLOptGroupElement;
class HTMLElementOrLong;
class LayoutUnit;
class PopupMenu;

class CORE_EXPORT HTMLSelectElement final
    : public HTMLFormControlElementWithState,
      private TypeAheadDataSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSelectElement(Document&);
  ~HTMLSelectElement() override;

  int selectedIndex() const;
  void setSelectedIndex(int);
  // `listIndex' version of |selectedIndex|.
  int SelectedListIndex() const;

  // For ValidityState
  String validationMessage() const override;
  bool ValueMissing() const override;

  String DefaultToolTip() const override;
  void ResetImpl() override;

  unsigned length() const;
  void setLength(unsigned, ExceptionState&);

  // TODO(tkent): Rename |size| to |Size|. This is not an implementation of
  // |size| IDL attribute.
  unsigned size() const { return size_; }
  bool IsMultiple() const { return is_multiple_; }

  bool UsesMenuList() const;

  void add(const HTMLOptionElementOrHTMLOptGroupElement&,
           const HTMLElementOrLong&,
           ExceptionState&);

  using Node::remove;
  void remove(int index);

  String value() const;
  void setValue(const String&, bool send_events = false);
  String SuggestedValue() const;
  void SetSuggestedValue(const String&);

  // |options| and |selectedOptions| are not safe to be used in in
  // HTMLOptionElement::removedFrom() and insertedInto() because their cache
  // is inconsistent in these functions.
  HTMLOptionsCollection* options();
  HTMLCollection* selectedOptions();

  // This is similar to |options| HTMLCollection.  But this is safe in
  // HTMLOptionElement::removedFrom() and insertedInto().
  // OptionList supports only forward iteration.
  OptionList GetOptionList() const { return OptionList(*this); }

  void OptionElementChildrenChanged(const HTMLOptionElement&);

  void InvalidateSelectedItems();

  using ListItems = HeapVector<Member<HTMLElement>>;
  // We prefer |optionList()| to |listItems()|.
  const ListItems& GetListItems() const;

  void AccessKeyAction(bool send_mouse_events) override;
  void SelectOptionByAccessKey(HTMLOptionElement*);

  void SetOption(unsigned index, HTMLOptionElement*, ExceptionState&);

  Element* namedItem(const AtomicString& name);
  HTMLOptionElement* item(unsigned index);

  void ScrollToSelection();
  void ScrollToOption(HTMLOptionElement*);

  bool CanSelectAll() const;
  void SelectAll();
  void ListBoxOnChange();
  int ActiveSelectionEndListIndex() const;
  HTMLOptionElement* ActiveSelectionEnd() const;
  void SetActiveSelectionAnchor(HTMLOptionElement*);
  void SetActiveSelectionEnd(HTMLOptionElement*);

  // For use in the implementation of HTMLOptionElement.
  void OptionSelectionStateChanged(HTMLOptionElement*, bool option_is_selected);
  void OptionInserted(HTMLOptionElement&, bool option_is_selected);
  void OptionRemoved(HTMLOptionElement&);
  bool AnonymousIndexedSetter(unsigned, HTMLOptionElement*, ExceptionState&);

  void OptGroupInsertedOrRemoved(HTMLOptGroupElement&);
  void HrInsertedOrRemoved(HTMLHRElement&);

  HTMLOptionElement* SpatialNavigationFocusedOption();
  void HandleMouseRelease();

  int ListIndexForOption(const HTMLOptionElement&);

  // Helper functions for popup menu implementations.
  String ItemText(const Element&) const;
  bool ItemIsDisplayNone(Element&) const;
  // itemComputedStyle() returns nullptr only if the owner Document is not
  // active.  So, It returns a valid object when we open a popup.
  const ComputedStyle* ItemComputedStyle(Element&) const;
  // Text starting offset in LTR.
  LayoutUnit ClientPaddingLeft() const;
  // Text starting offset in RTL.
  LayoutUnit ClientPaddingRight() const;
  void SelectOptionByPopup(int list_index);
  void SelectMultipleOptionsByPopup(const Vector<int>& list_indices);
  // A popup is canceled when the popup was hidden without selecting an item.
  void PopupDidCancel();
  // Provisional selection is a selection made using arrow keys or type ahead.
  void ProvisionalSelectionChanged(unsigned);
  void PopupDidHide();
  bool PopupIsVisible() const { return popup_is_visible_; }
  HTMLOptionElement* OptionToBeShown() const;
  void ShowPopup();
  void HidePopup();
  PopupMenu* Popup() const { return popup_.Get(); }
  void DidMutateSubtree();

  void ResetTypeAheadSessionForTesting();

  // Used for slot assignment.
  static bool CanAssignToSelectSlot(const Node&);

  bool HasNonInBodyInsertionMode() const override { return true; }

  void Trace(Visitor*) override;
  void CloneNonAttributePropertiesFrom(const Element&,
                                       CloneChildrenFlag) override;

 private:
  const AtomicString& FormControlType() const override;

  bool MayTriggerVirtualKeyboard() const override;

  void DispatchFocusEvent(
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities) override;
  void DispatchBlurEvent(Element* new_focused_element,
                         WebFocusType,
                         InputDeviceCapabilities* source_capabilities) override;

  bool CanStartSelection() const override { return false; }

  bool IsEnumeratable() const override { return true; }
  bool IsInteractiveContent() const override;
  bool IsLabelable() const override { return true; }

  FormControlState SaveFormControlState() const override;
  void RestoreFormControlState(const FormControlState&) override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void DidRecalcStyle(const StyleRecalcChange) override;
  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(bool performing_reattach = false) override;
  void AppendToFormData(FormData&) override;
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;

  void DefaultEventHandler(Event&) override;

  void DispatchInputAndChangeEventForMenuList();

  void SetRecalcListItems();
  void RecalcListItems() const;
  enum ResetReason { kResetReasonSelectedOptionRemoved, kResetReasonOthers };
  void ResetToDefaultSelection(ResetReason = kResetReasonOthers);
  void TypeAheadFind(const KeyboardEvent&);
  void SaveLastSelection();
  void SaveListboxActiveSelection();
  // Returns the first selected OPTION, or nullptr.
  HTMLOptionElement* SelectedOption() const;

  bool IsOptionalFormControl() const override {
    return !IsRequiredFormControl();
  }
  bool IsRequiredFormControl() const override;

  bool HasPlaceholderLabelOption() const;

  enum SelectOptionFlag {
    kDeselectOtherOptionsFlag = 1 << 0,
    kDispatchInputAndChangeEventFlag = 1 << 1,
    kMakeOptionDirtyFlag = 1 << 2,
  };
  typedef unsigned SelectOptionFlags;
  void SelectOption(HTMLOptionElement*, SelectOptionFlags);
  bool DeselectItemsWithoutValidation(
      HTMLOptionElement* element_to_exclude = nullptr);
  void ParseMultipleAttribute(const AtomicString&);
  HTMLOptionElement* LastSelectedOption() const;
  void UpdateSelectedState(HTMLOptionElement*, bool multi, bool shift);
  void MenuListDefaultEventHandler(Event&);
  void HandlePopupOpenKeyboardEvent(Event&);
  bool ShouldOpenPopupForKeyDownEvent(const KeyboardEvent&);
  bool ShouldOpenPopupForKeyPressEvent(const KeyboardEvent&);
  void ListBoxDefaultEventHandler(Event&);
  void SetOptionsChangedOnLayoutObject();
  wtf_size_t SearchOptionsForValue(const String&,
                                   wtf_size_t list_index_start,
                                   wtf_size_t list_index_end) const;
  void UpdateListBoxSelection(bool deselect_other_options, bool scroll = true);
  void UpdateMultiSelectListBoxFocus();
  void SetIndexToSelectOnCancel(int list_index);
  void SetSuggestedOption(HTMLOptionElement*);
  void ToggleSelection(HTMLOptionElement&);

  // Returns nullptr if listIndex is out of bounds, or it doesn't point an
  // HTMLOptionElement.
  HTMLOptionElement* OptionAtListIndex(int list_index) const;
  enum SkipDirection { kSkipBackwards = -1, kSkipForwards = 1 };
  HTMLOptionElement* NextValidOption(int list_index,
                                     SkipDirection,
                                     int skip) const;
  HTMLOptionElement* NextSelectableOption(HTMLOptionElement*) const;
  HTMLOptionElement* PreviousSelectableOption(HTMLOptionElement*) const;
  HTMLOptionElement* FirstSelectableOption() const;
  HTMLOptionElement* LastSelectableOption() const;
  HTMLOptionElement* NextSelectableOptionPageAway(HTMLOptionElement*,
                                                  SkipDirection) const;
  HTMLOptionElement* EventTargetOption(const Event&);
  AutoscrollController* GetAutoscrollController() const;
  void ScrollToOptionTask();

  bool AreAuthorShadowsAllowed() const override { return false; }
  void FinishParsingChildren() override;

  // TypeAheadDataSource functions.
  int IndexOfSelectedOption() const override;
  int OptionCount() const override;
  String OptionAtIndex(int index) const override;

  void ObserveTreeMutation();
  void UnobserveTreeMutation();

  // Apply changes to rendering as a result of attribute changes (multiple,
  // size).
  void ChangeRendering();

  // list_items_ contains HTMLOptionElement, HTMLOptGroupElement, and
  // HTMLHRElement objects.
  mutable ListItems list_items_;
  Vector<bool> last_on_change_selection_;
  Vector<bool> cached_state_for_active_selection_;
  TypeAhead type_ahead_;
  unsigned size_;
  Member<HTMLOptionElement> last_on_change_option_;
  Member<HTMLOptionElement> active_selection_anchor_;
  Member<HTMLOptionElement> active_selection_end_;
  Member<HTMLOptionElement> option_to_scroll_to_;
  Member<HTMLOptionElement> suggested_option_;
  bool is_multiple_;
  bool is_in_non_contiguous_selection_;
  bool active_selection_state_;
  mutable bool should_recalc_list_items_;
  bool is_autofilled_by_preview_;

  class PopupUpdater;
  Member<PopupUpdater> popup_updater_;
  Member<PopupMenu> popup_;
  int index_to_select_on_cancel_;
  bool popup_is_visible_;

  FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, FirstSelectableOption);
  FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, LastSelectableOption);
  FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, NextSelectableOption);
  FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, PreviousSelectableOption);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECT_ELEMENT_H_
