/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/html_element_or_long.h"
#include "third_party/blink/renderer/bindings/core/v8/html_option_element_or_html_opt_group_element.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_list_box.h"
#include "third_party/blink/renderer/core/layout/layout_menu_list.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

using namespace HTMLNames;

// Upper limit of list_items_. According to the HTML standard, options larger
// than this limit doesn't work well because |selectedIndex| IDL attribute is
// signed.
static const unsigned kMaxListItems = INT_MAX;

HTMLSelectElement::HTMLSelectElement(Document& document)
    : HTMLFormControlElementWithState(selectTag, document),
      type_ahead_(this),
      size_(0),
      last_on_change_option_(nullptr),
      is_multiple_(false),
      active_selection_state_(false),
      should_recalc_list_items_(false),
      is_autofilled_by_preview_(false),
      index_to_select_on_cancel_(-1),
      popup_is_visible_(false) {
  SetHasCustomStyleCallbacks();
}

HTMLSelectElement* HTMLSelectElement::Create(Document& document) {
  HTMLSelectElement* select = new HTMLSelectElement(document);
  select->EnsureUserAgentShadowRoot();
  return select;
}

HTMLSelectElement::~HTMLSelectElement() = default;

// static
bool HTMLSelectElement::CanAssignToSelectSlot(const Node& node) {
  return node.HasTagName(optionTag) || node.HasTagName(optgroupTag) ||
         node.HasTagName(hrTag);
}

const AtomicString& HTMLSelectElement::FormControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, select_multiple, ("select-multiple"));
  DEFINE_STATIC_LOCAL(const AtomicString, select_one, ("select-one"));
  return is_multiple_ ? select_multiple : select_one;
}

bool HTMLSelectElement::HasPlaceholderLabelOption() const {
  // The select element has no placeholder label option if it has an attribute
  // "multiple" specified or a display size of non-1.
  //
  // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec
  // 3, 2010. "size() != 1" is correct.  Using "size() > 1" here because
  // size() may be 0 in WebKit.  See the discussion at
  // https://bugs.webkit.org/show_bug.cgi?id=43887
  //
  // "0 size()" happens when an attribute "size" is absent or an invalid size
  // attribute is specified.  In this case, the display size should be assumed
  // as the default.  The default display size is 1 for non-multiple select
  // elements, and 4 for multiple select elements.
  //
  // Finally, if size() == 0 and non-multiple, the display size can be assumed
  // as 1.
  if (IsMultiple() || size() > 1)
    return false;

  // TODO(tkent): This function is called in CSS selector matching. Using
  // listItems() might have performance impact.
  if (GetListItems().size() == 0 || !IsHTMLOptionElement(GetListItems()[0]))
    return false;
  return ToHTMLOptionElement(GetListItems()[0])->value().IsEmpty();
}

String HTMLSelectElement::validationMessage() const {
  if (!willValidate())
    return String();
  if (CustomError())
    return CustomValidationMessage();
  if (ValueMissing()) {
    return GetLocale().QueryString(
        WebLocalizedString::kValidationValueMissingForSelect);
  }
  return String();
}

bool HTMLSelectElement::ValueMissing() const {
  if (!willValidate())
    return false;

  if (!IsRequired())
    return false;

  int first_selection_index = selectedIndex();

  // If a non-placeholer label option is selected (firstSelectionIndex > 0),
  // it's not value-missing.
  return first_selection_index < 0 ||
         (!first_selection_index && HasPlaceholderLabelOption());
}

String HTMLSelectElement::DefaultToolTip() const {
  if (Form() && Form()->NoValidate())
    return String();
  return validationMessage();
}

void HTMLSelectElement::SelectMultipleOptionsByPopup(
    const Vector<int>& list_indices) {
  DCHECK(UsesMenuList());
  DCHECK(IsMultiple());
  for (wtf_size_t i = 0; i < list_indices.size(); ++i) {
    bool add_selection_if_not_first = i > 0;
    if (HTMLOptionElement* option = OptionAtListIndex(list_indices[i]))
      UpdateSelectedState(option, add_selection_if_not_first, false);
  }
  SetNeedsValidityCheck();
  // TODO(tkent): Using listBoxOnChange() is very confusing.
  ListBoxOnChange();
}

bool HTMLSelectElement::UsesMenuList() const {
  if (LayoutTheme::GetTheme().DelegatesMenuListRendering())
    return true;

  return !is_multiple_ && size_ <= 1;
}

int HTMLSelectElement::ActiveSelectionEndListIndex() const {
  HTMLOptionElement* option = ActiveSelectionEnd();
  return option ? option->ListIndex() : -1;
}

HTMLOptionElement* HTMLSelectElement::ActiveSelectionEnd() const {
  if (active_selection_end_)
    return active_selection_end_.Get();
  return LastSelectedOption();
}

void HTMLSelectElement::add(
    const HTMLOptionElementOrHTMLOptGroupElement& element,
    const HTMLElementOrLong& before,
    ExceptionState& exception_state) {
  HTMLElement* element_to_insert;
  DCHECK(!element.IsNull());
  if (element.IsHTMLOptionElement())
    element_to_insert = element.GetAsHTMLOptionElement();
  else
    element_to_insert = element.GetAsHTMLOptGroupElement();

  HTMLElement* before_element;
  if (before.IsHTMLElement())
    before_element = before.GetAsHTMLElement();
  else if (before.IsLong())
    before_element = options()->item(before.GetAsLong());
  else
    before_element = nullptr;

  InsertBefore(element_to_insert, before_element, exception_state);
  SetNeedsValidityCheck();
}

void HTMLSelectElement::remove(int option_index) {
  if (HTMLOptionElement* option = item(option_index))
    option->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

String HTMLSelectElement::value() const {
  if (HTMLOptionElement* option = SelectedOption())
    return option->value();
  return "";
}

void HTMLSelectElement::setValue(const String& value, bool send_events) {
  HTMLOptionElement* option = nullptr;
  // Find the option with value() matching the given parameter and make it the
  // current selection.
  for (auto* const item : GetOptionList()) {
    if (item->value() == value) {
      option = item;
      break;
    }
  }

  HTMLOptionElement* previous_selected_option = SelectedOption();
  SetSuggestedOption(nullptr);
  if (is_autofilled_by_preview_)
    SetAutofillState(WebAutofillState::kNotFilled);
  SelectOptionFlags flags = kDeselectOtherOptionsFlag | kMakeOptionDirtyFlag;
  if (send_events)
    flags |= kDispatchInputAndChangeEventFlag;
  SelectOption(option, flags);

  if (send_events && previous_selected_option != option && !UsesMenuList())
    ListBoxOnChange();
}

String HTMLSelectElement::SuggestedValue() const {
  return suggested_option_ ? suggested_option_->value() : "";
}

void HTMLSelectElement::SetSuggestedValue(const String& value) {
  if (value.IsNull()) {
    SetSuggestedOption(nullptr);
    return;
  }

  for (auto* const option : GetOptionList()) {
    if (option->value() == value) {
      SetSuggestedOption(option);
      is_autofilled_by_preview_ = true;
      return;
    }
  }

  SetSuggestedOption(nullptr);
}

bool HTMLSelectElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == alignAttr) {
    // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
    // See http://bugs.webkit.org/show_bug.cgi?id=12072
    return false;
  }

  return HTMLFormControlElementWithState::IsPresentationAttribute(name);
}

void HTMLSelectElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == sizeAttr) {
    unsigned old_size = size_;
    if (!ParseHTMLNonNegativeInteger(params.new_value, size_))
      size_ = 0;
    SetNeedsValidityCheck();
    if (size_ != old_size) {
      if (InActiveDocument())
        LazyReattachIfAttached();
      ResetToDefaultSelection();
      if (!UsesMenuList())
        SaveListboxActiveSelection();
    }
  } else if (params.name == multipleAttr) {
    ParseMultipleAttribute(params.new_value);
  } else if (params.name == accesskeyAttr) {
    // FIXME: ignore for the moment.
    //
  } else {
    HTMLFormControlElementWithState::ParseAttribute(params);
  }
}

bool HTMLSelectElement::MayTriggerVirtualKeyboard() const {
  return true;
}

bool HTMLSelectElement::CanSelectAll() const {
  return !UsesMenuList();
}

LayoutObject* HTMLSelectElement::CreateLayoutObject(const ComputedStyle&) {
  if (UsesMenuList())
    return new LayoutMenuList(this);
  return new LayoutListBox(this);
}

HTMLCollection* HTMLSelectElement::selectedOptions() {
  return EnsureCachedCollection<HTMLCollection>(kSelectedOptions);
}

HTMLOptionsCollection* HTMLSelectElement::options() {
  return EnsureCachedCollection<HTMLOptionsCollection>(kSelectOptions);
}

void HTMLSelectElement::OptionElementChildrenChanged(
    const HTMLOptionElement& option) {
  SetNeedsValidityCheck();

  if (GetLayoutObject()) {
    if (option.Selected() && UsesMenuList())
      GetLayoutObject()->UpdateFromElement();
    if (AXObjectCache* cache =
            GetLayoutObject()->GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(this);
  }
}

void HTMLSelectElement::AccessKeyAction(bool send_mouse_events) {
  focus();
  DispatchSimulatedClick(
      nullptr, send_mouse_events ? kSendMouseUpDownEvents : kSendNoEvents);
}

Element* HTMLSelectElement::namedItem(const AtomicString& name) {
  return options()->namedItem(name);
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index) {
  return options()->item(index);
}

void HTMLSelectElement::SetOption(unsigned index,
                                  HTMLOptionElement* option,
                                  ExceptionState& exception_state) {
  int diff = index - length();
  // We should check |index >= maxListItems| first to avoid integer overflow.
  if (index >= kMaxListItems ||
      GetListItems().size() + diff + 1 > kMaxListItems) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        String::Format("Blocked to expand the option list and set an option at "
                       "index=%u.  The maximum list length is %u.",
                       index, kMaxListItems)));
    return;
  }
  HTMLOptionElementOrHTMLOptGroupElement element;
  element.SetHTMLOptionElement(option);
  HTMLElementOrLong before;
  // Out of array bounds? First insert empty dummies.
  if (diff > 0) {
    setLength(index, exception_state);
    // Replace an existing entry?
  } else if (diff < 0) {
    before.SetHTMLElement(options()->item(index + 1));
    remove(index);
  }
  if (exception_state.HadException())
    return;
  // Finally add the new element.
  EventQueueScope scope;
  add(element, before, exception_state);
  if (diff >= 0 && option->Selected())
    OptionSelectionStateChanged(option, true);
}

void HTMLSelectElement::setLength(unsigned new_len,
                                  ExceptionState& exception_state) {
  // We should check |newLen > maxListItems| first to avoid integer overflow.
  if (new_len > kMaxListItems ||
      GetListItems().size() + new_len - length() > kMaxListItems) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        String::Format("Blocked to expand the option list to %u items.  The "
                       "maximum list length is %u.",
                       new_len, kMaxListItems)));
    return;
  }
  int diff = length() - new_len;

  if (diff < 0) {  // Add dummy elements.
    do {
      AppendChild(HTMLOptionElement::Create(GetDocument()), exception_state);
      if (exception_state.HadException())
        break;
    } while (++diff);
  } else {
    // Removing children fires mutation events, which might mutate the DOM
    // further, so we first copy out a list of elements that we intend to
    // remove then attempt to remove them one at a time.
    HeapVector<Member<HTMLOptionElement>> items_to_remove;
    size_t option_index = 0;
    for (auto* const option : GetOptionList()) {
      if (option_index++ >= new_len) {
        DCHECK(option->parentNode());
        items_to_remove.push_back(option);
      }
    }

    for (auto& item : items_to_remove) {
      if (item->parentNode())
        item->parentNode()->RemoveChild(item.Get(), exception_state);
    }
  }
  SetNeedsValidityCheck();
}

bool HTMLSelectElement::IsRequiredFormControl() const {
  return IsRequired();
}

HTMLOptionElement* HTMLSelectElement::OptionAtListIndex(int list_index) const {
  if (list_index < 0)
    return nullptr;
  const ListItems& items = GetListItems();
  if (static_cast<wtf_size_t>(list_index) >= items.size())
    return nullptr;
  return ToHTMLOptionElementOrNull(items[list_index]);
}

// Returns the 1st valid OPTION |skip| items from |listIndex| in direction
// |direction| if there is one.
// Otherwise, it returns the valid OPTION closest to that boundary which is past
// |listIndex| if there is one.
// Otherwise, it returns nullptr.
// Valid means that it is enabled and visible.
HTMLOptionElement* HTMLSelectElement::NextValidOption(int list_index,
                                                      SkipDirection direction,
                                                      int skip) const {
  DCHECK(direction == kSkipBackwards || direction == kSkipForwards);
  const ListItems& list_items = GetListItems();
  HTMLOptionElement* last_good_option = nullptr;
  int size = list_items.size();
  for (list_index += direction; list_index >= 0 && list_index < size;
       list_index += direction) {
    --skip;
    HTMLElement* element = list_items[list_index];
    if (!IsHTMLOptionElement(*element))
      continue;
    if (ToHTMLOptionElement(*element).IsDisplayNone())
      continue;
    if (element->IsDisabledFormControl())
      continue;
    if (!UsesMenuList() && !element->GetLayoutObject())
      continue;
    last_good_option = ToHTMLOptionElement(element);
    if (skip <= 0)
      break;
  }
  return last_good_option;
}

HTMLOptionElement* HTMLSelectElement::NextSelectableOption(
    HTMLOptionElement* start_option) const {
  return NextValidOption(start_option ? start_option->ListIndex() : -1,
                         kSkipForwards, 1);
}

HTMLOptionElement* HTMLSelectElement::PreviousSelectableOption(
    HTMLOptionElement* start_option) const {
  return NextValidOption(
      start_option ? start_option->ListIndex() : GetListItems().size(),
      kSkipBackwards, 1);
}

HTMLOptionElement* HTMLSelectElement::FirstSelectableOption() const {
  // TODO(tkent): This is not efficient.  nextSlectableOption(nullptr) is
  // faster.
  return NextValidOption(GetListItems().size(), kSkipBackwards, INT_MAX);
}

HTMLOptionElement* HTMLSelectElement::LastSelectableOption() const {
  // TODO(tkent): This is not efficient.  previousSlectableOption(nullptr) is
  // faster.
  return NextValidOption(-1, kSkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in
// direction |direction|.
HTMLOptionElement* HTMLSelectElement::NextSelectableOptionPageAway(
    HTMLOptionElement* start_option,
    SkipDirection direction) const {
  const ListItems& items = GetListItems();
  // Can't use size_ because LayoutObject forces a minimum size.
  int page_size = 0;
  if (GetLayoutObject()->IsListBox()) {
    // -1 so we still show context.
    page_size = ToLayoutListBox(GetLayoutObject())->size() - 1;
  }

  // One page away, but not outside valid bounds.
  // If there is a valid option item one page away, the index is chosen.
  // If there is no exact one page away valid option, returns startIndex or
  // the most far index.
  int start_index = start_option ? start_option->ListIndex() : -1;
  int edge_index = (direction == kSkipForwards) ? 0 : (items.size() - 1);
  int skip_amount =
      page_size +
      ((direction == kSkipForwards) ? start_index : (edge_index - start_index));
  return NextValidOption(edge_index, direction, skip_amount);
}

void HTMLSelectElement::SelectAll() {
  DCHECK(!UsesMenuList());
  if (!GetLayoutObject() || !is_multiple_)
    return;

  // Save the selection so it can be compared to the new selectAll selection
  // when dispatching change events.
  SaveLastSelection();

  active_selection_state_ = true;
  SetActiveSelectionAnchor(NextSelectableOption(nullptr));
  SetActiveSelectionEnd(PreviousSelectableOption(nullptr));

  UpdateListBoxSelection(false, false);
  ListBoxOnChange();
  SetNeedsValidityCheck();
}

void HTMLSelectElement::SaveLastSelection() {
  if (UsesMenuList()) {
    last_on_change_option_ = SelectedOption();
    return;
  }

  last_on_change_selection_.clear();
  for (auto& element : GetListItems()) {
    last_on_change_selection_.push_back(
        IsHTMLOptionElement(*element) &&
        ToHTMLOptionElement(element)->Selected());
  }
}

void HTMLSelectElement::SetActiveSelectionAnchor(HTMLOptionElement* option) {
  active_selection_anchor_ = option;
  if (!UsesMenuList())
    SaveListboxActiveSelection();
}

void HTMLSelectElement::SaveListboxActiveSelection() {
  // Cache the selection state so we can restore the old selection as the new
  // selection pivots around this anchor index.
  // Example:
  // 1. Press the mouse button on the second OPTION
  //   active_selection_anchor_ points the second OPTION.
  // 2. Drag the mouse pointer onto the fifth OPTION
  //   active_selection_end_ points the fifth OPTION, OPTIONs at 1-4 indices
  //   are selected.
  // 3. Drag the mouse pointer onto the fourth OPTION
  //   active_selection_end_ points the fourth OPTION, OPTIONs at 1-3 indices
  //   are selected.
  //   UpdateListBoxSelection needs to clear selection of the fifth OPTION.
  cached_state_for_active_selection_.resize(0);
  for (auto* const option : GetOptionList()) {
    cached_state_for_active_selection_.push_back(option->Selected());
  }
}

void HTMLSelectElement::SetActiveSelectionEnd(HTMLOptionElement* option) {
  active_selection_end_ = option;
}

void HTMLSelectElement::UpdateListBoxSelection(bool deselect_other_options,
                                               bool scroll) {
  DCHECK(GetLayoutObject());
  DCHECK(GetLayoutObject()->IsListBox() || is_multiple_);

  int active_selection_anchor_index =
      active_selection_anchor_ ? active_selection_anchor_->index() : -1;
  int active_selection_end_index =
      active_selection_end_ ? active_selection_end_->index() : -1;
  int start =
      std::min(active_selection_anchor_index, active_selection_end_index);
  int end = std::max(active_selection_anchor_index, active_selection_end_index);

  int i = 0;
  for (auto* const option : GetOptionList()) {
    if (option->IsDisabledFormControl() || !option->GetLayoutObject()) {
      ++i;
      continue;
    }
    if (i >= start && i <= end) {
      option->SetSelectedState(active_selection_state_);
      option->SetDirty(true);
    } else if (deselect_other_options ||
               i >= static_cast<int>(
                        cached_state_for_active_selection_.size())) {
      option->SetSelectedState(false);
      option->SetDirty(true);
    } else {
      option->SetSelectedState(cached_state_for_active_selection_[i]);
    }
    ++i;
  }

  SetNeedsValidityCheck();
  if (scroll)
    ScrollToSelection();
  NotifyFormStateChanged();
}

void HTMLSelectElement::ListBoxOnChange() {
  DCHECK(!UsesMenuList() || is_multiple_);

  const ListItems& items = GetListItems();

  // If the cached selection list is empty, or the size has changed, then fire
  // dispatchFormControlChangeEvent, and return early.
  // FIXME: Why? This looks unreasonable.
  if (last_on_change_selection_.IsEmpty() ||
      last_on_change_selection_.size() != items.size()) {
    DispatchChangeEvent();
    return;
  }

  // Update last_on_change_selection_ and fire a 'change' event.
  bool fire_on_change = false;
  for (unsigned i = 0; i < items.size(); ++i) {
    HTMLElement* element = items[i];
    bool selected = IsHTMLOptionElement(*element) &&
                    ToHTMLOptionElement(element)->Selected();
    if (selected != last_on_change_selection_[i])
      fire_on_change = true;
    last_on_change_selection_[i] = selected;
  }

  if (fire_on_change) {
    DispatchInputEvent();
    DispatchChangeEvent();
  }
}

void HTMLSelectElement::DispatchInputAndChangeEventForMenuList() {
  DCHECK(UsesMenuList());

  HTMLOptionElement* selected_option = SelectedOption();
  if (last_on_change_option_.Get() != selected_option) {
    last_on_change_option_ = selected_option;
    DispatchInputEvent();
    DispatchChangeEvent();
  }
}

void HTMLSelectElement::ScrollToSelection() {
  if (!IsFinishedParsingChildren())
    return;
  if (UsesMenuList())
    return;
  ScrollToOption(ActiveSelectionEnd());
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ListboxActiveIndexChanged(this);
}

void HTMLSelectElement::SetOptionsChangedOnLayoutObject() {
  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (!UsesMenuList())
      return;
    ToLayoutMenuList(layout_object)
        ->SetNeedsLayoutAndPrefWidthsRecalc(
            LayoutInvalidationReason::kMenuOptionsChanged);
  }
}

const HTMLSelectElement::ListItems& HTMLSelectElement::GetListItems() const {
  if (should_recalc_list_items_) {
    RecalcListItems();
  } else {
#if DCHECK_IS_ON()
    HeapVector<Member<HTMLElement>> items = list_items_;
    RecalcListItems();
    DCHECK(items == list_items_);
#endif
  }

  return list_items_;
}

void HTMLSelectElement::InvalidateSelectedItems() {
  if (HTMLCollection* collection =
          CachedCollection<HTMLCollection>(kSelectedOptions))
    collection->InvalidateCache();
}

void HTMLSelectElement::SetRecalcListItems() {
  // FIXME: This function does a bunch of confusing things depending on if it
  // is in the document or not.

  should_recalc_list_items_ = true;

  SetOptionsChangedOnLayoutObject();
  if (!isConnected()) {
    if (HTMLOptionsCollection* collection =
            CachedCollection<HTMLOptionsCollection>(kSelectOptions))
      collection->InvalidateCache();
    InvalidateSelectedItems();
  }

  if (GetLayoutObject()) {
    if (AXObjectCache* cache =
            GetLayoutObject()->GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(this);
  }
}

void HTMLSelectElement::RecalcListItems() const {
  TRACE_EVENT0("blink", "HTMLSelectElement::recalcListItems");
  list_items_.resize(0);

  should_recalc_list_items_ = false;

  for (Element* current_element = ElementTraversal::FirstWithin(*this);
       current_element && list_items_.size() < kMaxListItems;) {
    if (!current_element->IsHTMLElement()) {
      current_element =
          ElementTraversal::NextSkippingChildren(*current_element, this);
      continue;
    }
    HTMLElement& current = ToHTMLElement(*current_element);

    // We should ignore nested optgroup elements. The HTML parser flatten
    // them.  However we need to ignore nested optgroups built by DOM APIs.
    // This behavior matches to IE and Firefox.
    if (IsHTMLOptGroupElement(current)) {
      if (current.parentNode() != this) {
        current_element = ElementTraversal::NextSkippingChildren(current, this);
        continue;
      }
      list_items_.push_back(&current);
      if (Element* next_element = ElementTraversal::FirstWithin(current)) {
        current_element = next_element;
        continue;
      }
    }

    if (IsHTMLOptionElement(current))
      list_items_.push_back(&current);

    if (IsHTMLHRElement(current))
      list_items_.push_back(&current);

    // In conforming HTML code, only <optgroup> and <option> will be found
    // within a <select>. We call NodeTraversal::nextSkippingChildren so
    // that we only step into those tags that we choose to. For web-compat,
    // we should cope with the case where odd tags like a <div> have been
    // added but we handle this because such tags have already been removed
    // from the <select>'s subtree at this point.
    current_element =
        ElementTraversal::NextSkippingChildren(*current_element, this);
  }
}

void HTMLSelectElement::ResetToDefaultSelection(ResetReason reason) {
  // https://html.spec.whatwg.org/multipage/forms.html#ask-for-a-reset
  if (IsMultiple())
    return;
  HTMLOptionElement* first_enabled_option = nullptr;
  HTMLOptionElement* last_selected_option = nullptr;
  bool did_change = false;
  int option_index = 0;
  // We can't use HTMLSelectElement::options here because this function is
  // called in Node::insertedInto and Node::removedFrom before invalidating
  // node collections.
  for (auto* const option : GetOptionList()) {
    if (option->Selected()) {
      if (last_selected_option) {
        last_selected_option->SetSelectedState(false);
        did_change = true;
      }
      last_selected_option = option;
    }
    if (!first_enabled_option && !option->IsDisabledFormControl()) {
      first_enabled_option = option;
      if (reason == kResetReasonSelectedOptionRemoved) {
        // There must be no selected OPTIONs.
        break;
      }
    }
    ++option_index;
  }
  if (!last_selected_option && size_ <= 1 &&
      (!first_enabled_option ||
       (first_enabled_option && !first_enabled_option->Selected()))) {
    SelectOption(first_enabled_option,
                 reason == kResetReasonSelectedOptionRemoved
                     ? 0
                     : kDeselectOtherOptionsFlag);
    last_selected_option = first_enabled_option;
    did_change = true;
  }
  if (did_change)
    SetNeedsValidityCheck();
  last_on_change_option_ = last_selected_option;
}

HTMLOptionElement* HTMLSelectElement::SelectedOption() const {
  for (auto* const option : GetOptionList()) {
    if (option->Selected())
      return option;
  }
  return nullptr;
}

int HTMLSelectElement::selectedIndex() const {
  unsigned index = 0;

  // Return the number of the first option selected.
  for (auto* const option : GetOptionList()) {
    if (option->Selected())
      return index;
    ++index;
  }

  return -1;
}

void HTMLSelectElement::setSelectedIndex(int index) {
  SelectOption(item(index), kDeselectOtherOptionsFlag | kMakeOptionDirtyFlag);
}

int HTMLSelectElement::SelectedListIndex() const {
  int index = 0;
  for (const auto& item : GetListItems()) {
    if (IsHTMLOptionElement(item) && ToHTMLOptionElement(item)->Selected())
      return index;
    ++index;
  }
  return -1;
}

void HTMLSelectElement::SetSuggestedOption(HTMLOptionElement* option) {
  if (suggested_option_ == option)
    return;
  suggested_option_ = option;

  if (LayoutObject* layout_object = GetLayoutObject()) {
    layout_object->UpdateFromElement();
    ScrollToOption(option);
  }
  if (PopupIsVisible())
    popup_->UpdateFromElement(PopupMenu::kBySelectionChange);
}

void HTMLSelectElement::ScrollToOption(HTMLOptionElement* option) {
  if (!option)
    return;
  if (UsesMenuList())
    return;
  bool has_pending_task = option_to_scroll_to_;
  // We'd like to keep an HTMLOptionElement reference rather than the index of
  // the option because the task should work even if unselected option is
  // inserted before executing scrollToOptionTask().
  option_to_scroll_to_ = option;
  if (!has_pending_task) {
    GetDocument()
        .GetTaskRunner(TaskType::kUserInteraction)
        ->PostTask(FROM_HERE, WTF::Bind(&HTMLSelectElement::ScrollToOptionTask,
                                        WrapPersistent(this)));
  }
}

void HTMLSelectElement::ScrollToOptionTask() {
  HTMLOptionElement* option = option_to_scroll_to_.Release();
  if (!option || !isConnected())
    return;
  // OptionRemoved() makes sure option_to_scroll_to_ doesn't have an option with
  // another owner.
  DCHECK_EQ(option->OwnerSelectElement(), this);
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (!GetLayoutObject() || !GetLayoutObject()->IsListBox())
    return;
  LayoutRect bounds = option->BoundingBoxForScrollIntoView();
  ToLayoutListBox(GetLayoutObject())->ScrollToRect(bounds);
}

void HTMLSelectElement::OptionSelectionStateChanged(HTMLOptionElement* option,
                                                    bool option_is_selected) {
  DCHECK_EQ(option->OwnerSelectElement(), this);
  if (option_is_selected)
    SelectOption(option, IsMultiple() ? 0 : kDeselectOtherOptionsFlag);
  else if (!UsesMenuList() || IsMultiple())
    SelectOption(nullptr, IsMultiple() ? 0 : kDeselectOtherOptionsFlag);
  else
    ResetToDefaultSelection();
}

void HTMLSelectElement::OptionInserted(HTMLOptionElement& option,
                                       bool option_is_selected) {
  DCHECK_EQ(option.OwnerSelectElement(), this);
  SetRecalcListItems();
  if (option_is_selected) {
    SelectOption(&option, IsMultiple() ? 0 : kDeselectOtherOptionsFlag);
  } else {
    // No need to reset if we already have a selected option.
    if (!last_on_change_option_)
      ResetToDefaultSelection();
  }
  SetNeedsValidityCheck();
  last_on_change_selection_.clear();

  if (!GetDocument().IsActive())
    return;

  GetDocument()
      .GetFrame()
      ->GetPage()
      ->GetChromeClient()
      .SelectFieldOptionsChanged(*this);
}

void HTMLSelectElement::OptionRemoved(HTMLOptionElement& option) {
  SetRecalcListItems();
  if (option.Selected())
    ResetToDefaultSelection(kResetReasonSelectedOptionRemoved);
  else if (!last_on_change_option_)
    ResetToDefaultSelection();
  if (last_on_change_option_ == &option)
    last_on_change_option_.Clear();
  if (option_to_scroll_to_ == &option)
    option_to_scroll_to_.Clear();
  if (active_selection_anchor_ == &option)
    active_selection_anchor_.Clear();
  if (active_selection_end_ == &option)
    active_selection_end_.Clear();
  if (suggested_option_ == &option)
    SetSuggestedOption(nullptr);
  if (option.Selected())
    SetAutofillState(WebAutofillState::kNotFilled);
  SetNeedsValidityCheck();
  last_on_change_selection_.clear();

  if (!GetDocument().IsActive())
    return;

  GetDocument()
      .GetFrame()
      ->GetPage()
      ->GetChromeClient()
      .SelectFieldOptionsChanged(*this);
}

void HTMLSelectElement::OptGroupInsertedOrRemoved(
    HTMLOptGroupElement& optgroup) {
  SetRecalcListItems();
  SetNeedsValidityCheck();
  last_on_change_selection_.clear();
}

void HTMLSelectElement::HrInsertedOrRemoved(HTMLHRElement& hr) {
  SetRecalcListItems();
  last_on_change_selection_.clear();
}

// TODO(tkent): This function is not efficient.  It contains multiple O(N)
// operations. crbug.com/577989.
void HTMLSelectElement::SelectOption(HTMLOptionElement* element,
                                     SelectOptionFlags flags) {
  TRACE_EVENT0("blink", "HTMLSelectElement::selectOption");

  bool should_update_popup = false;

  // SelectedOption() is O(N).
  if (IsAutofilled() && SelectedOption() != element)
    SetAutofillState(WebAutofillState::kNotFilled);

  if (element) {
    if (!element->Selected())
      should_update_popup = true;
    element->SetSelectedState(true);
    if (flags & kMakeOptionDirtyFlag)
      element->SetDirty(true);
  }

  // DeselectItemsWithoutValidation() is O(N).
  if (flags & kDeselectOtherOptionsFlag)
    should_update_popup |= DeselectItemsWithoutValidation(element);

  // We should update active selection after finishing OPTION state change
  // because setActiveSelectionAnchorIndex() stores OPTION's selection state.
  if (element) {
    // setActiveSelectionAnchor is O(N).
    if (!active_selection_anchor_ || !IsMultiple() ||
        flags & kDeselectOtherOptionsFlag)
      SetActiveSelectionAnchor(element);
    if (!active_selection_end_ || !IsMultiple() ||
        flags & kDeselectOtherOptionsFlag)
      SetActiveSelectionEnd(element);
  }

  // Need to update last_on_change_option_ before
  // LayoutMenuList::UpdateFromElement.
  bool should_dispatch_events = false;
  if (UsesMenuList()) {
    should_dispatch_events = (flags & kDispatchInputAndChangeEventFlag) &&
                             last_on_change_option_ != element;
    last_on_change_option_ = element;
  }

  // For the menu list case, this is what makes the selected element appear.
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->UpdateFromElement();
  // PopupMenu::UpdateFromElement() posts an O(N) task.
  if (PopupIsVisible() && should_update_popup)
    popup_->UpdateFromElement(PopupMenu::kBySelectionChange);

  ScrollToSelection();
  SetNeedsValidityCheck();

  if (UsesMenuList()) {
    if (should_dispatch_events) {
      DispatchInputEvent();
      DispatchChangeEvent();
    }
    if (LayoutObject* layout_object = GetLayoutObject()) {
      // Need to check UsesMenuList() again because event handlers might
      // change the status.
      if (UsesMenuList()) {
        // DidSelectOption() is O(N) because of HTMLOptionElement::index().
        ToLayoutMenuList(layout_object)->DidSelectOption(element);
      }
    }
  }

  NotifyFormStateChanged();

  if (LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()) &&
      GetDocument().IsActive()) {
    GetDocument()
        .GetPage()
        ->GetChromeClient()
        .DidChangeSelectionInSelectControl(*this);
  }
}

void HTMLSelectElement::DispatchFocusEvent(
    Element* old_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during blur event dispatch.
  if (UsesMenuList())
    SaveLastSelection();
  HTMLFormControlElementWithState::DispatchFocusEvent(old_focused_element, type,
                                                      source_capabilities);
}

void HTMLSelectElement::DispatchBlurEvent(
    Element* new_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  type_ahead_.ResetSession();
  // We only need to fire change events here for menu lists, because we fire
  // change events for list boxes whenever the selection change is actually
  // made.  This matches other browsers' behavior.
  if (UsesMenuList())
    DispatchInputAndChangeEventForMenuList();
  last_on_change_selection_.clear();
  if (PopupIsVisible())
    HidePopup();
  HTMLFormControlElementWithState::DispatchBlurEvent(new_focused_element, type,
                                                     source_capabilities);
}

// Returns true if selection state of any OPTIONs is changed.
bool HTMLSelectElement::DeselectItemsWithoutValidation(
    HTMLOptionElement* exclude_element) {
  if (!IsMultiple() && UsesMenuList() && last_on_change_option_ &&
      last_on_change_option_ != exclude_element) {
    last_on_change_option_->SetSelectedState(false);
    return true;
  }
  bool did_update_selection = false;
  for (auto* const option : GetOptionList()) {
    if (option != exclude_element) {
      if (option->Selected())
        did_update_selection = true;
      option->SetSelectedState(false);
    }
  }
  return did_update_selection;
}

FormControlState HTMLSelectElement::SaveFormControlState() const {
  const ListItems& items = GetListItems();
  wtf_size_t length = items.size();
  FormControlState state;
  for (wtf_size_t i = 0; i < length; ++i) {
    if (!IsHTMLOptionElement(*items[i]))
      continue;
    HTMLOptionElement* option = ToHTMLOptionElement(items[i]);
    if (!option->Selected())
      continue;
    state.Append(option->value());
    state.Append(String::Number(i));
    if (!IsMultiple())
      break;
  }
  return state;
}

wtf_size_t HTMLSelectElement::SearchOptionsForValue(
    const String& value,
    wtf_size_t list_index_start,
    wtf_size_t list_index_end) const {
  const ListItems& items = GetListItems();
  wtf_size_t loop_end_index = std::min(items.size(), list_index_end);
  for (wtf_size_t i = list_index_start; i < loop_end_index; ++i) {
    if (!IsHTMLOptionElement(items[i]))
      continue;
    if (ToHTMLOptionElement(items[i])->value() == value)
      return i;
  }
  return kNotFound;
}

void HTMLSelectElement::RestoreFormControlState(const FormControlState& state) {
  RecalcListItems();

  const ListItems& items = GetListItems();
  wtf_size_t items_size = items.size();
  if (items_size == 0)
    return;

  SelectOption(nullptr, kDeselectOtherOptionsFlag);

  // The saved state should have at least one value and an index.
  DCHECK_GE(state.ValueSize(), 2u);
  if (!IsMultiple()) {
    unsigned index = state[1].ToUInt();
    if (index < items_size && IsHTMLOptionElement(items[index]) &&
        ToHTMLOptionElement(items[index])->value() == state[0]) {
      ToHTMLOptionElement(items[index])->SetSelectedState(true);
      ToHTMLOptionElement(items[index])->SetDirty(true);
      last_on_change_option_ = ToHTMLOptionElement(items[index]);
    } else {
      wtf_size_t found_index = SearchOptionsForValue(state[0], 0, items_size);
      if (found_index != kNotFound) {
        ToHTMLOptionElement(items[found_index])->SetSelectedState(true);
        ToHTMLOptionElement(items[found_index])->SetDirty(true);
        last_on_change_option_ = ToHTMLOptionElement(items[found_index]);
      }
    }
  } else {
    wtf_size_t start_index = 0;
    for (wtf_size_t i = 0; i < state.ValueSize(); i += 2) {
      const String& value = state[i];
      const unsigned index = state[i + 1].ToUInt();
      if (index < items_size && IsHTMLOptionElement(items[index]) &&
          ToHTMLOptionElement(items[index])->value() == value) {
        ToHTMLOptionElement(items[index])->SetSelectedState(true);
        ToHTMLOptionElement(items[index])->SetDirty(true);
        start_index = index + 1;
      } else {
        wtf_size_t found_index =
            SearchOptionsForValue(value, start_index, items_size);
        if (found_index == kNotFound)
          found_index = SearchOptionsForValue(value, 0, start_index);
        if (found_index == kNotFound)
          continue;
        ToHTMLOptionElement(items[found_index])->SetSelectedState(true);
        ToHTMLOptionElement(items[found_index])->SetDirty(true);
        start_index = found_index + 1;
      }
    }
  }

  SetNeedsValidityCheck();
}

void HTMLSelectElement::ParseMultipleAttribute(const AtomicString& value) {
  bool old_multiple = is_multiple_;
  HTMLOptionElement* old_selected_option = SelectedOption();
  is_multiple_ = !value.IsNull();
  SetNeedsValidityCheck();
  LazyReattachIfAttached();
  // Restore selectedIndex after changing the multiple flag to preserve
  // selection as single-line and multi-line has different defaults.
  if (old_multiple != is_multiple_) {
    // Preserving the first selection is compatible with Firefox and
    // WebKit. However Edge seems to "ask for a reset" simply.  As of 2016
    // March, the HTML specification says nothing about this.
    if (old_selected_option)
      SelectOption(old_selected_option, kDeselectOtherOptionsFlag);
    else
      ResetToDefaultSelection();
  }
}

void HTMLSelectElement::AppendToFormData(FormData& form_data) {
  const AtomicString& name = GetName();
  if (name.IsEmpty())
    return;

  for (auto* const option : GetOptionList()) {
    if (option->Selected() && !option->IsDisabledFormControl())
      form_data.AppendFromElement(name, option->value());
  }
}

void HTMLSelectElement::ResetImpl() {
  for (auto* const option : GetOptionList()) {
    option->SetSelectedState(option->FastHasAttribute(selectedAttr));
    option->SetDirty(false);
  }
  ResetToDefaultSelection();
  SetNeedsValidityCheck();
}

void HTMLSelectElement::HandlePopupOpenKeyboardEvent(Event& event) {
  focus();
  // Calling focus() may cause us to lose our layoutObject. Return true so
  // that our caller doesn't process the event further, but don't set
  // the event as handled.
  if (!GetLayoutObject() || !GetLayoutObject()->IsMenuList() ||
      IsDisabledFormControl())
    return;
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during selectOption, which gets called from
  // selectOptionByPopup, which gets called after the user makes a selection
  // from the menu.
  SaveLastSelection();
  ShowPopup();
  event.SetDefaultHandled();
  return;
}

bool HTMLSelectElement::ShouldOpenPopupForKeyDownEvent(
    const KeyboardEvent& key_event) {
  const String& key = key_event.key();
  LayoutTheme& layout_theme = LayoutTheme::GetTheme();

  if (IsSpatialNavigationEnabled(GetDocument().GetFrame()))
    return false;

  return ((layout_theme.PopsMenuByArrowKeys() &&
           (key == "ArrowDown" || key == "ArrowUp")) ||
          (layout_theme.PopsMenuByAltDownUpOrF4Key() &&
           (key == "ArrowDown" || key == "ArrowUp") && key_event.altKey()) ||
          (layout_theme.PopsMenuByAltDownUpOrF4Key() &&
           (!key_event.altKey() && !key_event.ctrlKey() && key == "F4")));
}

bool HTMLSelectElement::ShouldOpenPopupForKeyPressEvent(
    const KeyboardEvent& event) {
  LayoutTheme& layout_theme = LayoutTheme::GetTheme();
  int key_code = event.keyCode();

  return ((layout_theme.PopsMenuBySpaceKey() && key_code == ' ' &&
           !type_ahead_.HasActiveSession(event)) ||
          (layout_theme.PopsMenuByReturnKey() && key_code == '\r'));
}

void HTMLSelectElement::MenuListDefaultEventHandler(Event& event) {
  if (event.type() == EventTypeNames::keydown) {
    if (!GetLayoutObject() || !event.IsKeyboardEvent())
      return;

    auto& key_event = ToKeyboardEvent(event);
    if (ShouldOpenPopupForKeyDownEvent(key_event)) {
      HandlePopupOpenKeyboardEvent(event);
      return;
    }

    // When using spatial navigation, we want to be able to navigate away
    // from the select element when the user hits any of the arrow keys,
    // instead of changing the selection.
    if (IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
      if (!active_selection_state_)
        return;
    }

    // The key handling below shouldn't be used for non spatial navigation
    // mode Mac
    if (LayoutTheme::GetTheme().PopsMenuByArrowKeys() &&
        !IsSpatialNavigationEnabled(GetDocument().GetFrame()))
      return;

    int ignore_modifiers = WebInputEvent::kShiftKey |
                           WebInputEvent::kControlKey | WebInputEvent::kAltKey |
                           WebInputEvent::kMetaKey;
    if (key_event.GetModifiers() & ignore_modifiers)
      return;

    const String& key = key_event.key();
    bool handled = true;
    const ListItems& list_items = GetListItems();
    HTMLOptionElement* option = SelectedOption();
    int list_index = option ? option->ListIndex() : -1;

    if (key == "ArrowDown" || key == "ArrowRight")
      option = NextValidOption(list_index, kSkipForwards, 1);
    else if (key == "ArrowUp" || key == "ArrowLeft")
      option = NextValidOption(list_index, kSkipBackwards, 1);
    else if (key == "PageDown")
      option = NextValidOption(list_index, kSkipForwards, 3);
    else if (key == "PageUp")
      option = NextValidOption(list_index, kSkipBackwards, 3);
    else if (key == "Home")
      option = NextValidOption(-1, kSkipForwards, 1);
    else if (key == "End")
      option = NextValidOption(list_items.size(), kSkipBackwards, 1);
    else
      handled = false;

    if (handled && option) {
      SelectOption(option, kDeselectOtherOptionsFlag | kMakeOptionDirtyFlag |
                               kDispatchInputAndChangeEventFlag);
    }

    if (handled)
      event.SetDefaultHandled();
  }

  if (event.type() == EventTypeNames::keypress) {
    if (!GetLayoutObject() || !event.IsKeyboardEvent())
      return;

    int key_code = ToKeyboardEvent(event).keyCode();
    if (key_code == ' ' &&
        IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
      // Use space to toggle arrow key handling for selection change or
      // spatial navigation.
      active_selection_state_ = !active_selection_state_;
      event.SetDefaultHandled();
      return;
    }

    auto& key_event = ToKeyboardEvent(event);
    if (ShouldOpenPopupForKeyPressEvent(key_event)) {
      HandlePopupOpenKeyboardEvent(event);
      return;
    }

    if (!LayoutTheme::GetTheme().PopsMenuByReturnKey() && key_code == '\r') {
      if (Form())
        Form()->SubmitImplicitly(event, false);
      DispatchInputAndChangeEventForMenuList();
      event.SetDefaultHandled();
    }
  }

  if (event.type() == EventTypeNames::mousedown && event.IsMouseEvent() &&
      ToMouseEvent(event).button() ==
          static_cast<short>(WebPointerProperties::Button::kLeft)) {
    InputDeviceCapabilities* source_capabilities =
        GetDocument()
            .domWindow()
            ->GetInputDeviceCapabilities()
            ->FiresTouchEvents(ToMouseEvent(event).FromTouch());
    focus(FocusParams(SelectionBehaviorOnFocus::kRestore, kWebFocusTypeNone,
                      source_capabilities));
    if (GetLayoutObject() && GetLayoutObject()->IsMenuList() &&
        !IsDisabledFormControl()) {
      if (PopupIsVisible()) {
        HidePopup();
      } else {
        // Save the selection so it can be compared to the new selection
        // when we call onChange during selectOption, which gets called
        // from selectOptionByPopup, which gets called after the user
        // makes a selection from the menu.
        SaveLastSelection();
        // TODO(lanwei): Will check if we need to add
        // InputDeviceCapabilities here when select menu list gets
        // focus, see https://crbug.com/476530.
        ShowPopup();
      }
    }
    event.SetDefaultHandled();
  }
}

void HTMLSelectElement::UpdateSelectedState(HTMLOptionElement* clicked_option,
                                            bool multi,
                                            bool shift) {
  DCHECK(clicked_option);
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during mouseup, or after autoscroll finishes.
  SaveLastSelection();

  active_selection_state_ = true;

  bool shift_select = is_multiple_ && shift;
  bool multi_select = is_multiple_ && multi && !shift;

  // Keep track of whether an active selection (like during drag selection),
  // should select or deselect.
  if (clicked_option->Selected() && multi_select) {
    active_selection_state_ = false;
    clicked_option->SetSelectedState(false);
    clicked_option->SetDirty(true);
  }

  // If we're not in any special multiple selection mode, then deselect all
  // other items, excluding the clicked option. If no option was clicked, then
  // this will deselect all items in the list.
  if (!shift_select && !multi_select)
    DeselectItemsWithoutValidation(clicked_option);

  // If the anchor hasn't been set, and we're doing a single selection or a
  // shift selection, then initialize the anchor to the first selected index.
  if (!active_selection_anchor_ && !multi_select)
    SetActiveSelectionAnchor(SelectedOption());

  // Set the selection state of the clicked option.
  if (!clicked_option->IsDisabledFormControl()) {
    clicked_option->SetSelectedState(true);
    clicked_option->SetDirty(true);
  }

  // If there was no selectedIndex() for the previous initialization, or If
  // we're doing a single selection, or a multiple selection (using cmd or
  // ctrl), then initialize the anchor index to the listIndex that just got
  // clicked.
  if (!active_selection_anchor_ || !shift_select)
    SetActiveSelectionAnchor(clicked_option);

  SetActiveSelectionEnd(clicked_option);
  UpdateListBoxSelection(!multi_select);
}

HTMLOptionElement* HTMLSelectElement::EventTargetOption(const Event& event) {
  Node* target_node = event.target()->ToNode();
  if (!target_node || !IsHTMLOptionElement(*target_node))
    return nullptr;
  return ToHTMLOptionElement(target_node);
}

int HTMLSelectElement::ListIndexForOption(const HTMLOptionElement& option) {
  const ListItems& items = GetListItems();
  wtf_size_t length = items.size();
  for (wtf_size_t i = 0; i < length; ++i) {
    if (items[i].Get() == &option)
      return i;
  }
  return -1;
}

AutoscrollController* HTMLSelectElement::GetAutoscrollController() const {
  if (Page* page = GetDocument().GetPage())
    return &page->GetAutoscrollController();
  return nullptr;
}

void HTMLSelectElement::HandleMouseRelease() {
  // We didn't start this click/drag on any options.
  if (last_on_change_selection_.IsEmpty())
    return;
  ListBoxOnChange();
}

void HTMLSelectElement::ListBoxDefaultEventHandler(Event& event) {
  if (event.type() == EventTypeNames::gesturetap && event.IsGestureEvent()) {
    focus();
    // Calling focus() may cause us to lose our layoutObject or change the
    // layoutObject type, in which case do not want to handle the event.
    if (!GetLayoutObject() || !GetLayoutObject()->IsListBox())
      return;

    // Convert to coords relative to the list box if needed.
    auto& gesture_event = ToGestureEvent(event);
    if (HTMLOptionElement* option = EventTargetOption(gesture_event)) {
      if (!IsDisabledFormControl()) {
        UpdateSelectedState(option, true, gesture_event.shiftKey());
        ListBoxOnChange();
      }
      event.SetDefaultHandled();
    }

  } else if (event.type() == EventTypeNames::mousedown &&
             event.IsMouseEvent() &&
             ToMouseEvent(event).button() ==
                 static_cast<short>(WebPointerProperties::Button::kLeft)) {
    focus();
    // Calling focus() may cause us to lose our layoutObject, in which case
    // do not want to handle the event.
    if (!GetLayoutObject() || !GetLayoutObject()->IsListBox() ||
        IsDisabledFormControl())
      return;

    // Convert to coords relative to the list box if needed.
    auto& mouse_event = ToMouseEvent(event);
    if (HTMLOptionElement* option = EventTargetOption(mouse_event)) {
      if (!option->IsDisabledFormControl()) {
#if defined(OS_MACOSX)
        UpdateSelectedState(option, mouse_event.metaKey(),
                            mouse_event.shiftKey());
#else
        UpdateSelectedState(option, mouse_event.ctrlKey(),
                            mouse_event.shiftKey());
#endif
      }
      if (LocalFrame* frame = GetDocument().GetFrame())
        frame->GetEventHandler().SetMouseDownMayStartAutoscroll();

      event.SetDefaultHandled();
    }

  } else if (event.type() == EventTypeNames::mousemove &&
             event.IsMouseEvent()) {
    auto& mouse_event = ToMouseEvent(event);
    if (mouse_event.button() !=
            static_cast<short>(WebPointerProperties::Button::kLeft) ||
        !mouse_event.ButtonDown())
      return;

    if (LayoutObject* object = GetLayoutObject())
      object->GetFrameView()->UpdateAllLifecyclePhasesExceptPaint();

    if (Page* page = GetDocument().GetPage()) {
      page->GetAutoscrollController().StartAutoscrollForSelection(
          GetLayoutObject());
    }
    // Mousedown didn't happen in this element.
    if (last_on_change_selection_.IsEmpty())
      return;

    if (HTMLOptionElement* option = EventTargetOption(mouse_event)) {
      if (!IsDisabledFormControl()) {
        if (is_multiple_) {
          // Only extend selection if there is something selected.
          if (!active_selection_anchor_)
            return;

          SetActiveSelectionEnd(option);
          UpdateListBoxSelection(false);
        } else {
          SetActiveSelectionAnchor(option);
          SetActiveSelectionEnd(option);
          UpdateListBoxSelection(true);
        }
      }
    }

  } else if (event.type() == EventTypeNames::mouseup && event.IsMouseEvent() &&
             ToMouseEvent(event).button() ==
                 static_cast<short>(WebPointerProperties::Button::kLeft) &&
             GetLayoutObject()) {
    if (GetDocument().GetPage() &&
        GetDocument()
            .GetPage()
            ->GetAutoscrollController()
            .AutoscrollInProgressFor(ToLayoutBox(GetLayoutObject())))
      GetDocument().GetPage()->GetAutoscrollController().StopAutoscroll();
    else
      HandleMouseRelease();

  } else if (event.type() == EventTypeNames::keydown) {
    if (!event.IsKeyboardEvent())
      return;
    const String& key = ToKeyboardEvent(event).key();

    bool handled = false;
    HTMLOptionElement* end_option = nullptr;
    if (!active_selection_end_) {
      // Initialize the end index
      if (key == "ArrowDown" || key == "PageDown") {
        HTMLOptionElement* start_option = LastSelectedOption();
        handled = true;
        if (key == "ArrowDown") {
          end_option = NextSelectableOption(start_option);
        } else {
          end_option =
              NextSelectableOptionPageAway(start_option, kSkipForwards);
        }
      } else if (key == "ArrowUp" || key == "PageUp") {
        HTMLOptionElement* start_option = SelectedOption();
        handled = true;
        if (key == "ArrowUp") {
          end_option = PreviousSelectableOption(start_option);
        } else {
          end_option =
              NextSelectableOptionPageAway(start_option, kSkipBackwards);
        }
      }
    } else {
      // Set the end index based on the current end index.
      if (key == "ArrowDown") {
        end_option = NextSelectableOption(active_selection_end_.Get());
        handled = true;
      } else if (key == "ArrowUp") {
        end_option = PreviousSelectableOption(active_selection_end_.Get());
        handled = true;
      } else if (key == "PageDown") {
        end_option = NextSelectableOptionPageAway(active_selection_end_.Get(),
                                                  kSkipForwards);
        handled = true;
      } else if (key == "PageUp") {
        end_option = NextSelectableOptionPageAway(active_selection_end_.Get(),
                                                  kSkipBackwards);
        handled = true;
      }
    }
    if (key == "Home") {
      end_option = FirstSelectableOption();
      handled = true;
    } else if (key == "End") {
      end_option = LastSelectableOption();
      handled = true;
    }

    if (IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
      // Check if the selection moves to the boundary.
      if (key == "ArrowLeft" || key == "ArrowRight" ||
          ((key == "ArrowDown" || key == "ArrowUp") &&
           end_option == active_selection_end_))
        return;
    }

    if (end_option && handled) {
      // Save the selection so it can be compared to the new selection
      // when dispatching change events immediately after making the new
      // selection.
      SaveLastSelection();

      SetActiveSelectionEnd(end_option);

      bool select_new_item =
          !is_multiple_ || ToKeyboardEvent(event).shiftKey() ||
          !IsSpatialNavigationEnabled(GetDocument().GetFrame());
      if (select_new_item)
        active_selection_state_ = true;
      // If the anchor is unitialized, or if we're going to deselect all
      // other options, then set the anchor index equal to the end index.
      bool deselect_others =
          !is_multiple_ ||
          (!ToKeyboardEvent(event).shiftKey() && select_new_item);
      if (!active_selection_anchor_ || deselect_others) {
        if (deselect_others)
          DeselectItemsWithoutValidation();
        SetActiveSelectionAnchor(active_selection_end_.Get());
      }

      ScrollToOption(end_option);
      if (select_new_item) {
        UpdateListBoxSelection(deselect_others);
        ListBoxOnChange();
      } else {
        ScrollToSelection();
      }

      event.SetDefaultHandled();
    }

  } else if (event.type() == EventTypeNames::keypress) {
    if (!event.IsKeyboardEvent())
      return;
    int key_code = ToKeyboardEvent(event).keyCode();

    if (key_code == '\r') {
      if (Form())
        Form()->SubmitImplicitly(event, false);
      event.SetDefaultHandled();
    } else if (is_multiple_ && key_code == ' ' &&
               IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
      // Use space to toggle selection change.
      active_selection_state_ = !active_selection_state_;
      UpdateSelectedState(active_selection_end_.Get(), true /*multi*/,
                          false /*shift*/);
      ListBoxOnChange();
      event.SetDefaultHandled();
    }
  }
}

void HTMLSelectElement::DefaultEventHandler(Event& event) {
  if (!GetLayoutObject())
    return;

  if (event.type() == EventTypeNames::click ||
      event.type() == EventTypeNames::change) {
    user_has_edited_the_field_ = true;
  }

  if (IsDisabledFormControl()) {
    HTMLFormControlElementWithState::DefaultEventHandler(event);
    return;
  }

  if (UsesMenuList())
    MenuListDefaultEventHandler(event);
  else
    ListBoxDefaultEventHandler(event);
  if (event.DefaultHandled())
    return;

  if (event.type() == EventTypeNames::keypress && event.IsKeyboardEvent()) {
    auto& keyboard_event = ToKeyboardEvent(event);
    if (!keyboard_event.ctrlKey() && !keyboard_event.altKey() &&
        !keyboard_event.metaKey() &&
        WTF::Unicode::IsPrintableChar(keyboard_event.charCode())) {
      TypeAheadFind(keyboard_event);
      event.SetDefaultHandled();
      return;
    }
  }
  HTMLFormControlElementWithState::DefaultEventHandler(event);
}

HTMLOptionElement* HTMLSelectElement::LastSelectedOption() const {
  const ListItems& items = GetListItems();
  for (wtf_size_t i = items.size(); i;) {
    if (HTMLOptionElement* option = OptionAtListIndex(--i)) {
      if (option->Selected())
        return option;
    }
  }
  return nullptr;
}

int HTMLSelectElement::IndexOfSelectedOption() const {
  return SelectedListIndex();
}

int HTMLSelectElement::OptionCount() const {
  return GetListItems().size();
}

String HTMLSelectElement::OptionAtIndex(int index) const {
  if (HTMLOptionElement* option = OptionAtListIndex(index)) {
    if (!option->IsDisabledFormControl())
      return option->DisplayLabel();
  }
  return String();
}

void HTMLSelectElement::TypeAheadFind(const KeyboardEvent& event) {
  int index = type_ahead_.HandleEvent(
      event, TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);
  if (index < 0)
    return;
  SelectOption(OptionAtListIndex(index), kDeselectOtherOptionsFlag |
                                             kMakeOptionDirtyFlag |
                                             kDispatchInputAndChangeEventFlag);
  if (!UsesMenuList())
    ListBoxOnChange();
}

void HTMLSelectElement::SelectOptionByAccessKey(HTMLOptionElement* option) {
  // First bring into focus the list box.
  if (!IsFocused())
    AccessKeyAction(false);

  if (!option || option->OwnerSelectElement() != this)
    return;
  EventQueueScope scope;
  // If this index is already selected, unselect. otherwise update the
  // selected index.
  SelectOptionFlags flags = kDispatchInputAndChangeEventFlag |
                            (IsMultiple() ? 0 : kDeselectOtherOptionsFlag);
  if (option->Selected()) {
    if (UsesMenuList())
      SelectOption(nullptr, flags);
    else
      option->SetSelectedState(false);
  } else {
    SelectOption(option, flags);
  }
  option->SetDirty(true);
  if (UsesMenuList())
    return;
  ListBoxOnChange();
  ScrollToSelection();
}

unsigned HTMLSelectElement::length() const {
  unsigned options = 0;
  for (auto* const option : GetOptionList()) {
    ALLOW_UNUSED_LOCAL(option);
    ++options;
  }
  return options;
}

void HTMLSelectElement::FinishParsingChildren() {
  HTMLFormControlElementWithState::FinishParsingChildren();
  if (UsesMenuList())
    return;
  ScrollToOption(SelectedOption());
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ListboxActiveIndexChanged(this);
}

bool HTMLSelectElement::AnonymousIndexedSetter(
    unsigned index,
    HTMLOptionElement* value,
    ExceptionState& exception_state) {
  if (!value) {  // undefined or null
    remove(index);
    return true;
  }
  SetOption(index, value, exception_state);
  return true;
}

bool HTMLSelectElement::IsInteractiveContent() const {
  return true;
}

bool HTMLSelectElement::SupportsAutofocus() const {
  return true;
}

void HTMLSelectElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(list_items_);
  visitor->Trace(last_on_change_option_);
  visitor->Trace(active_selection_anchor_);
  visitor->Trace(active_selection_end_);
  visitor->Trace(option_to_scroll_to_);
  visitor->Trace(suggested_option_);
  visitor->Trace(popup_);
  visitor->Trace(popup_updater_);
  HTMLFormControlElementWithState::Trace(visitor);
}

void HTMLSelectElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  root.AppendChild(
      HTMLSlotElement::CreateUserAgentCustomAssignSlot(GetDocument()));
}

HTMLOptionElement* HTMLSelectElement::SpatialNavigationFocusedOption() {
  if (!IsSpatialNavigationEnabled(GetDocument().GetFrame()))
    return nullptr;
  HTMLOptionElement* focused_option = ActiveSelectionEnd();
  if (!focused_option)
    focused_option = FirstSelectableOption();
  return focused_option;
}

String HTMLSelectElement::ItemText(const Element& element) const {
  String item_string;
  if (auto* optgroup = ToHTMLOptGroupElementOrNull(element))
    item_string = optgroup->GroupLabelText();
  else if (auto* option = ToHTMLOptionElementOrNull(element))
    item_string = option->TextIndentedToRespectGroupLabel();

  if (GetLayoutObject() && GetLayoutObject()->Style())
    GetLayoutObject()->Style()->ApplyTextTransform(&item_string);
  return item_string;
}

bool HTMLSelectElement::ItemIsDisplayNone(Element& element) const {
  if (auto* option = ToHTMLOptionElementOrNull(element))
    return option->IsDisplayNone();
  const ComputedStyle* style = ItemComputedStyle(element);
  return !style || style->Display() == EDisplay::kNone;
}

const ComputedStyle* HTMLSelectElement::ItemComputedStyle(
    Element& element) const {
  return element.GetComputedStyle() ? element.GetComputedStyle()
                                    : element.EnsureComputedStyle();
}

LayoutUnit HTMLSelectElement::ClientPaddingLeft() const {
  if (GetLayoutObject() && GetLayoutObject()->IsMenuList())
    return ToLayoutMenuList(GetLayoutObject())->ClientPaddingLeft();
  return LayoutUnit();
}

LayoutUnit HTMLSelectElement::ClientPaddingRight() const {
  if (GetLayoutObject() && GetLayoutObject()->IsMenuList())
    return ToLayoutMenuList(GetLayoutObject())->ClientPaddingRight();
  return LayoutUnit();
}

void HTMLSelectElement::PopupDidHide() {
  popup_is_visible_ = false;
  UnobserveTreeMutation();
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
    if (GetLayoutObject() && GetLayoutObject()->IsMenuList())
      cache->DidHideMenuListPopup(ToLayoutMenuList(GetLayoutObject()));
  }
}

void HTMLSelectElement::SetIndexToSelectOnCancel(int list_index) {
  index_to_select_on_cancel_ = list_index;
  if (GetLayoutObject())
    GetLayoutObject()->UpdateFromElement();
}

HTMLOptionElement* HTMLSelectElement::OptionToBeShown() const {
  if (HTMLOptionElement* option = OptionAtListIndex(index_to_select_on_cancel_))
    return option;
  if (suggested_option_)
    return suggested_option_;
  // TODO(tkent): We should not call optionToBeShown() in isMultiple() case.
  if (IsMultiple())
    return SelectedOption();
  DCHECK_EQ(SelectedOption(), last_on_change_option_);
  return last_on_change_option_;
}

void HTMLSelectElement::SelectOptionByPopup(int list_index) {
  DCHECK(UsesMenuList());
  // Check to ensure a page navigation has not occurred while the popup was
  // up.
  Document& doc = GetDocument();
  if (&doc != doc.GetFrame()->GetDocument())
    return;

  SetIndexToSelectOnCancel(-1);

  HTMLOptionElement* option = OptionAtListIndex(list_index);
  // Bail out if this index is already the selected one, to avoid running
  // unnecessary JavaScript that can mess up autofill when there is no actual
  // change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and
  // <rdar://7467917>).  The selectOption function does not behave this way,
  // possibly because other callers need a change event even in cases where
  // the selected option is not change.
  if (option == SelectedOption())
    return;
  SelectOption(option, kDeselectOtherOptionsFlag | kMakeOptionDirtyFlag |
                           kDispatchInputAndChangeEventFlag);
}

void HTMLSelectElement::PopupDidCancel() {
  if (index_to_select_on_cancel_ >= 0)
    SelectOptionByPopup(index_to_select_on_cancel_);
}

void HTMLSelectElement::ProvisionalSelectionChanged(unsigned list_index) {
  SetIndexToSelectOnCancel(list_index);
}

void HTMLSelectElement::ShowPopup() {
  if (PopupIsVisible())
    return;
  if (GetDocument().GetPage()->GetChromeClient().HasOpenedPopup())
    return;
  if (!GetLayoutObject() || !GetLayoutObject()->IsMenuList())
    return;
  if (VisibleBoundsInVisualViewport().IsEmpty())
    return;

  if (!popup_) {
    popup_ = GetDocument().GetPage()->GetChromeClient().OpenPopupMenu(
        *GetDocument().GetFrame(), *this);
  }
  if (!popup_)
    return;

  popup_is_visible_ = true;
  ObserveTreeMutation();

  LayoutMenuList* menu_list = ToLayoutMenuList(GetLayoutObject());
  popup_->Show();
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->DidShowMenuListPopup(menu_list);
}

void HTMLSelectElement::HidePopup() {
  if (popup_)
    popup_->Hide();
}

void HTMLSelectElement::DidRecalcStyle(StyleRecalcChange change) {
  HTMLFormControlElementWithState::DidRecalcStyle(change);
  if (PopupIsVisible())
    popup_->UpdateFromElement(PopupMenu::kByStyleChange);
}

void HTMLSelectElement::DetachLayoutTree(const AttachContext& context) {
  HTMLFormControlElementWithState::DetachLayoutTree(context);
  if (popup_)
    popup_->DisconnectClient();
  popup_is_visible_ = false;
  popup_ = nullptr;
  UnobserveTreeMutation();
}

void HTMLSelectElement::ResetTypeAheadSessionForTesting() {
  type_ahead_.ResetSession();
}

// PopupUpdater notifies updates of the specified SELECT element subtree to
// a PopupMenu object.
class HTMLSelectElement::PopupUpdater : public MutationObserver::Delegate {
 public:
  explicit PopupUpdater(HTMLSelectElement& select)
      : select_(select), observer_(MutationObserver::Create(this)) {
    MutationObserverInit init;
    init.setAttributeOldValue(true);
    init.setAttributes(true);
    // Observe only attributes which affect popup content.
    init.setAttributeFilter({"disabled", "label", "selected", "value"});
    init.setCharacterData(true);
    init.setCharacterDataOldValue(true);
    init.setChildList(true);
    init.setSubtree(true);
    observer_->observe(select_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return &select_->GetDocument();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    // We disconnect the MutationObserver when a popup is closed.  However
    // MutationObserver can call back after disconnection.
    if (!select_->PopupIsVisible())
      return;
    for (const auto& record : records) {
      if (record->type() == "attributes") {
        const Element& element = *ToElement(record->target());
        if (record->oldValue() == element.getAttribute(record->attributeName()))
          continue;
      } else if (record->type() == "characterData") {
        if (record->oldValue() == record->target()->nodeValue())
          continue;
      }
      select_->DidMutateSubtree();
      return;
    }
  }

  void Dispose() { observer_->disconnect(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(select_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<HTMLSelectElement> select_;
  Member<MutationObserver> observer_;
};

void HTMLSelectElement::ObserveTreeMutation() {
  DCHECK(!popup_updater_);
  popup_updater_ = new PopupUpdater(*this);
}

void HTMLSelectElement::UnobserveTreeMutation() {
  if (!popup_updater_)
    return;
  popup_updater_->Dispose();
  popup_updater_ = nullptr;
}

void HTMLSelectElement::DidMutateSubtree() {
  DCHECK(PopupIsVisible());
  DCHECK(popup_);
  popup_->UpdateFromElement(PopupMenu::kByDOMChange);
}

void HTMLSelectElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    CloneChildrenFlag flag) {
  const auto& source_element = static_cast<const HTMLSelectElement&>(source);
  user_has_edited_the_field_ = source_element.user_has_edited_the_field_;
  HTMLFormControlElement::CloneNonAttributePropertiesFrom(source, flag);
}

}  // namespace blink
