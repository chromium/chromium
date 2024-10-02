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
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlelement_long.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmloptgroupelement_htmloptionelement.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_selected_option_element.h"
#include "third_party/blink/renderer/core/html/forms/select_type.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_no_script_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/base/ui_base_features.h"

namespace blink {

using mojom::blink::FormControlType;

// https://html.spec.whatwg.org/#dom-htmloptionscollection-length
static const unsigned kMaxListItems = 100000;

// Default size when the multiple attribute is present but size attribute is
// absent.
const int kDefaultListBoxSize = 4;

class SelectDescendantsObserver : public MutationObserver::Delegate {
 public:
  explicit SelectDescendantsObserver(HTMLSelectElement& select)
      : select_(select), observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setChildList(true);
    init->setSubtree(true);
    observer_->observe(select_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return select_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    for (const auto& record : records) {
      if (record->type() == "childList") {
        for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
          if (auto* html_element =
                  DynamicTo<HTMLElement>(record->addedNodes()->item(i))) {
            if (!IsDescendantAllowed(html_element)) {
              // TODO(ansollan): Report an Issue to the DevTools' Issue Panel as
              // well.
              html_element->AddConsoleMessage(
                  mojom::blink::ConsoleMessageSource::kRecommendation,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  "A descendant of a <select> does not follow the content "
                  "model.");
              break;
            }
          }
        }
      }
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(select_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  bool IsDescendantAllowed(HTMLElement* element) {
    // TODO(ansollan): This should be looking at the tree structure to decide if
    // this is a valid descendant.
    return IsA<HTMLOptionElement>(element) ||
           IsA<HTMLOptGroupElement>(element) || IsA<HTMLHRElement>(element) ||
           IsA<HTMLScriptElement>(element) ||
           IsA<HTMLTemplateElement>(element) ||
           IsA<HTMLNoScriptElement>(element) ||
           IsA<HTMLButtonElement>(element) || IsA<HTMLDivElement>(element) ||
           IsA<HTMLSpanElement>(element);
  }
  Member<HTMLSelectElement> select_;
  Member<MutationObserver> observer_;
};

HTMLSelectElement::HTMLSelectElement(Document& document)
    : HTMLFormControlElementWithState(html_names::kSelectTag, document),
      type_ahead_(this),
      size_(0),
      last_on_change_option_(nullptr),
      is_multiple_(false),
      should_recalc_list_items_(false),
      index_to_select_on_cancel_(-1) {
  // Make sure SelectType is created after initializing |uses_menu_list_|.
  select_type_ = SelectType::Create(*this);
  SetHasCustomStyleCallbacks();
  EnsureUserAgentShadowRoot(SlotAssignmentMode::kManual);
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    descendants_observer_ =
        MakeGarbageCollected<SelectDescendantsObserver>(*this);
  }
}

HTMLSelectElement::~HTMLSelectElement() = default;

FormControlType HTMLSelectElement::FormControlType() const {
  return is_multiple_ ? FormControlType::kSelectMultiple
                      : FormControlType::kSelectOne;
}

const AtomicString& HTMLSelectElement::FormControlTypeAsString() const {
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
  if (GetListItems().size() == 0)
    return false;

  auto* option_element = DynamicTo<HTMLOptionElement>(GetListItems()[0].Get());
  if (!option_element)
    return false;

  return option_element->value().empty();
}

String HTMLSelectElement::validationMessage() const {
  if (!willValidate())
    return String();
  if (CustomError())
    return CustomValidationMessage();
  if (ValueMissing()) {
    return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING_SELECT);
  }
  return String();
}

bool HTMLSelectElement::ValueMissing() const {
  if (!IsRequired())
    return false;

  int first_selection_index = selectedIndex();

  // If a non-placeholder label option is selected (firstSelectionIndex > 0),
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

  HeapHashSet<Member<HTMLOptionElement>> old_selection;
  for (auto* option : GetOptionList()) {
    if (option->Selected()) {
      old_selection.insert(option);
      option->SetSelectedState(false);
    }
  }

  bool has_new_selection = false;
  for (int list_index : list_indices) {
    if (auto* option = OptionAtListIndex(list_index)) {
      option->SetSelectedState(true);
      option->SetDirty(true);
      auto iter = old_selection.find(option);
      if (iter != old_selection.end())
        old_selection.erase(iter);
      else
        has_new_selection = true;
    }
  }

  select_type_->UpdateTextStyleAndContent();
  SetNeedsValidityCheck();
  if (has_new_selection || !old_selection.empty()) {
    DispatchInputEvent();
    DispatchChangeEvent();
  }
}

unsigned HTMLSelectElement::ListBoxSize() const {
  DCHECK(!UsesMenuList());
  const unsigned specified_size = size();
  if (specified_size >= 1)
    return specified_size;
  return kDefaultListBoxSize;
}

void HTMLSelectElement::UpdateUsesMenuList() {
  if (LayoutTheme::GetTheme().DelegatesMenuListRendering())
    uses_menu_list_ = true;
  else
    uses_menu_list_ = !is_multiple_ && size_ <= 1;
}

int HTMLSelectElement::ActiveSelectionEndListIndex() const {
  HTMLOptionElement* option = ActiveSelectionEnd();
  return option ? option->ListIndex() : -1;
}

HTMLOptionElement* HTMLSelectElement::ActiveSelectionEnd() const {
  return select_type_->ActiveSelectionEnd();
}

void HTMLSelectElement::add(
    const V8UnionHTMLOptGroupElementOrHTMLOptionElement* element,
    const V8UnionHTMLElementOrLong* before,
    ExceptionState& exception_state) {
  DCHECK(element);

  HTMLElement* element_to_insert = nullptr;
  switch (element->GetContentType()) {
    case V8UnionHTMLOptGroupElementOrHTMLOptionElement::ContentType::
        kHTMLOptGroupElement:
      element_to_insert = element->GetAsHTMLOptGroupElement();
      break;
    case V8UnionHTMLOptGroupElementOrHTMLOptionElement::ContentType::
        kHTMLOptionElement:
      element_to_insert = element->GetAsHTMLOptionElement();
      break;
  }

  HTMLElement* before_element = nullptr;
  ContainerNode* target_container = this;
  if (before) {
    switch (before->GetContentType()) {
      case V8UnionHTMLElementOrLong::ContentType::kHTMLElement:
        before_element = before->GetAsHTMLElement();
        break;
      case V8UnionHTMLElementOrLong::ContentType::kLong:
        before_element = options()->item(before->GetAsLong());
        if (before_element && before_element->parentNode()) {
          target_container = before_element->parentNode();
        }
        break;
    }
  }

  target_container->InsertBefore(element_to_insert, before_element,
                                 exception_state);
  SetNeedsValidityCheck();
}

void HTMLSelectElement::remove(int option_index) {
  if (HTMLOptionElement* option = item(option_index))
    option->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

String HTMLSelectElement::Value() const {
  if (HTMLOptionElement* option = SelectedOption())
    return option->value();
  return "";
}

void HTMLSelectElement::setValueForBinding(const String& value) {
  String old_value = this->Value();
  bool was_autofilled = IsAutofilled();
  bool value_changed = old_value != value;
  SetValue(value, false,
           was_autofilled && !value_changed ? WebAutofillState::kAutofilled
                                            : WebAutofillState::kNotFilled);
  if (Page* page = GetDocument().GetPage(); page && value_changed) {
    page->GetChromeClient().JavaScriptChangedValue(*this, old_value,
                                                   was_autofilled);
  }
}

void HTMLSelectElement::SetValue(const String& value,
                                 bool send_events,
                                 WebAutofillState autofill_state) {
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
  SelectOptionFlags flags = kDeselectOtherOptionsFlag | kMakeOptionDirtyFlag;
  if (send_events)
    flags |= kDispatchInputAndChangeEventFlag;
  SelectOption(option, flags, autofill_state);

  if (send_events && previous_selected_option != option)
    select_type_->ListBoxOnChange();
}

void HTMLSelectElement::SetAutofillValue(const String& value,
                                         WebAutofillState autofill_state) {
  auto interacted_state = interacted_state_;
  SetValue(value, true, autofill_state);
  interacted_state_ = interacted_state;
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
      return;
    }
  }

  SetSuggestedOption(nullptr);
}

bool HTMLSelectElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kAlignAttr) {
    // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
    // See http://bugs.webkit.org/show_bug.cgi?id=12072
    return false;
  }

  return HTMLFormControlElementWithState::IsPresentationAttribute(name);
}

void HTMLSelectElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSizeAttr) {
    unsigned old_size = size_;
    if (!ParseHTMLNonNegativeInteger(params.new_value, size_))
      size_ = 0;
    SetNeedsValidityCheck();
    if (size_ != old_size) {
      ChangeRendering();
      UpdateUserAgentShadowTree(*UserAgentShadowRoot());
      ResetToDefaultSelection();
      select_type_->UpdateTextStyleAndContent();
      select_type_->SaveListboxActiveSelection();
    }
  } else if (params.name == html_names::kMultipleAttr) {
    ParseMultipleAttribute(params.new_value);
  } else if (params.name == html_names::kAccesskeyAttr) {
    // FIXME: ignore for the moment.
    //
  } else if (params.name == html_names::kSelectedoptionelementAttr) {
    if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
      HTMLSelectedOptionElement* old_selectedoption =
          DynamicTo<HTMLSelectedOptionElement>(
              getElementByIdIncludingDisconnected(*this, params.old_value));
      HTMLSelectedOptionElement* new_selectedoption =
          DynamicTo<HTMLSelectedOptionElement>(
              getElementByIdIncludingDisconnected(*this, params.new_value));
      if (old_selectedoption != new_selectedoption) {
        if (old_selectedoption) {
          // Clear out the contents of any <selectedoption> which we are
          // removing the association from.
          old_selectedoption->CloneContentsFromOptionElement(nullptr);
        }
        if (new_selectedoption) {
          new_selectedoption->CloneContentsFromOptionElement(SelectedOption());
        }
      }
    }
  } else {
    HTMLFormControlElementWithState::ParseAttribute(params);
  }
}

bool HTMLSelectElement::MayTriggerVirtualKeyboard() const {
  return true;
}

bool HTMLSelectElement::ShouldHaveFocusAppearance() const {
  // Don't draw focus ring for a select that has its popup open.
  if (PopupIsVisible())
    return false;

  return HTMLFormControlElementWithState::ShouldHaveFocusAppearance();
}

bool HTMLSelectElement::CanSelectAll() const {
  return !UsesMenuList();
}

LayoutObject* HTMLSelectElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (UsesMenuList()) {
    return MakeGarbageCollected<LayoutFlexibleBox>(this);
  }
  return MakeGarbageCollected<LayoutBlockFlow>(this);
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

  if (option.Selected())
    select_type_->UpdateTextStyleAndContent();
  if (GetLayoutObject()) {
    if (AXObjectCache* cache =
            GetLayoutObject()->GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(this);
  }

  if (option.Selected()) {
    if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
      for (HTMLSelectedOptionElement* selectedoption :
           TargetSelectedOptions()) {
        selectedoption->CloneContentsFromOptionElement(&option);
      }
    }
  }
}

void HTMLSelectElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  Focus(FocusParams(FocusTrigger::kUserGesture));
  DispatchSimulatedClick(nullptr, creation_scope);
}

HTMLOptionElement* HTMLSelectElement::namedItem(const AtomicString& name) {
  return To<HTMLOptionElement>(options()->namedItem(name));
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index) {
  return options()->item(index);
}

void HTMLSelectElement::SetOption(unsigned index,
                                  HTMLOptionElement* option,
                                  ExceptionState& exception_state) {
  int diff = index - length();
  // If we are adding options, we should check |index > maxListItems| first to
  // avoid integer overflow.
  if (index > length() && (index >= kMaxListItems ||
                           GetListItems().size() + diff + 1 > kMaxListItems)) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        String::Format(
            "Unable to expand the option list and set an option at index=%u. "
            "The maximum allowed list length is %u.",
            index, kMaxListItems)));
    return;
  }
  auto* element =
      MakeGarbageCollected<V8UnionHTMLOptGroupElementOrHTMLOptionElement>(
          option);
  V8UnionHTMLElementOrLong* before = nullptr;
  // Out of array bounds? First insert empty dummies.
  if (diff > 0) {
    setLength(index, exception_state);
    if (exception_state.HadException())
      return;
    // Replace an existing entry?
  } else if (diff < 0) {
    if (auto* before_element = options()->item(index + 1))
      before = MakeGarbageCollected<V8UnionHTMLElementOrLong>(before_element);
    remove(index);
  }
  // Finally add the new element.
  EventQueueScope scope;
  add(element, before, exception_state);
  if (exception_state.HadException())
    return;
  if (diff >= 0 && option->Selected())
    OptionSelectionStateChanged(option, true);
}

void HTMLSelectElement::setLength(unsigned new_len,
                                  ExceptionState& exception_state) {
  // If we are adding options, we should check |index > maxListItems| first to
  // avoid integer overflow.
  if (new_len > length() &&
      (new_len > kMaxListItems ||
       GetListItems().size() + new_len - length() > kMaxListItems)) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        String::Format("Unable to expand the option list to length %u. "
                       "The maximum allowed list length is %u.",
                       new_len, kMaxListItems)));
    return;
  }
  int diff = length() - new_len;

  if (diff < 0) {  // Add dummy elements.
    do {
      AppendChild(MakeGarbageCollected<HTMLOptionElement>(GetDocument()),
                  exception_state);
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
  return DynamicTo<HTMLOptionElement>(items[list_index].Get());
}

void HTMLSelectElement::SelectAll() {
  select_type_->SelectAll();
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

  select_type_->MaximumOptionWidthMightBeChanged();
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

  HTMLOptGroupElement* current_ancestor_optgroup = nullptr;

  for (Element* current_element = ElementTraversal::FirstWithin(*this);
       current_element && list_items_.size() < kMaxListItems;) {
    auto* current_html_element = DynamicTo<HTMLElement>(current_element);
    if (!current_html_element) {
      current_element =
          RuntimeEnabledFeatures::SelectParserRelaxationEnabled()
              ? ElementTraversal::Next(*current_element, this)
              : ElementTraversal::NextSkippingChildren(*current_element, this);
      continue;
    }

    // If there is a nested <select>, then its descendant <option>s belong to
    // it, not this.
    if (IsA<HTMLSelectElement>(current_html_element)) {
      current_element =
          ElementTraversal::NextSkippingChildren(*current_element, this);
      continue;
    }

    if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
      bool skip_children = false;
      // If the parser is allowed to have more than just <option>s and
      // <optgroup>s, then we need to iterate over all descendants.
      if (auto* current_optgroup =
              DynamicTo<HTMLOptGroupElement>(*current_html_element)) {
        if (current_ancestor_optgroup) {
          // For compat, don't look at descendants of a nested <optgroup>.
          skip_children = true;
        } else {
          current_ancestor_optgroup = current_optgroup;
          list_items_.push_back(current_html_element);
        }
      } else if (IsA<HTMLOptionElement>(*current_html_element) ||
                 IsA<HTMLHRElement>(*current_html_element)) {
        list_items_.push_back(current_html_element);
      }

      Element* (*next_element_fn)(const Node&, const Node*) =
          &ElementTraversal::Next;
      if (skip_children) {
        next_element_fn = &ElementTraversal::NextSkippingChildren;
      }
      if (current_ancestor_optgroup) {
        // In order to keep current_ancestor_optgroup up to date, try traversing
        // to the next element within it. If we can't, then we have reached the
        // end of the optgroup and should set it to nullptr.
        auto* next_within_optgroup =
            next_element_fn(*current_element, current_ancestor_optgroup);
        if (!next_within_optgroup) {
          current_ancestor_optgroup = nullptr;
          current_element = next_element_fn(*current_element, this);
        } else {
          current_element = next_within_optgroup;
        }
      } else {
        current_element = next_element_fn(*current_element, this);
      }

      continue;
    }

    // We should ignore nested optgroup elements. The HTML parser flatten
    // them. However we need to ignore nested optgroups built by DOM APIs.
    // This behavior matches to IE and Firefox.
    if (IsA<HTMLOptGroupElement>(*current_html_element)) {
      if (current_html_element->parentNode() != this) {
        current_element =
            ElementTraversal::NextSkippingChildren(*current_html_element, this);
        continue;
      }
      list_items_.push_back(current_html_element);
      if (Element* next_element =
              ElementTraversal::FirstWithin(*current_html_element)) {
        current_element = next_element;
        continue;
      }
    }

    if (IsA<HTMLOptionElement>(*current_html_element))
      list_items_.push_back(current_html_element);

    if (IsA<HTMLHRElement>(*current_html_element))
      list_items_.push_back(current_html_element);

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
  // https://html.spec.whatwg.org/C/#ask-for-a-reset
  if (IsMultiple())
    return;
  HTMLOptionElement* first_enabled_option = nullptr;
  HTMLOptionElement* last_selected_option = nullptr;
  bool did_change = false;
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
    auto* option_element = DynamicTo<HTMLOptionElement>(item.Get());
    if (option_element && option_element->Selected())
      return index;
    ++index;
  }
  return -1;
}

void HTMLSelectElement::SetSuggestedOption(HTMLOptionElement* option) {
  if (suggested_option_ == option)
    return;
  SetAutofillState(option ? WebAutofillState::kPreviewed
                          : WebAutofillState::kNotFilled);
  suggested_option_ = option;

  select_type_->DidSetSuggestedOption(option);
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

void HTMLSelectElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLFormControlElementWithState::ChildrenChanged(change);
  bool invalidate_button = false;
  if (change.type ==
      ChildrenChangeType::kFinishedBuildingDocumentFragmentTree) {
    for (Node& node : NodeTraversal::ChildrenOf(*this)) {
      ElementInserted(node);
      if (IsA<HTMLButtonElement>(node)) {
        invalidate_button = true;
      }
    }
  } else if (change.type == ChildrenChangeType::kElementInserted) {
    ElementInserted(*change.sibling_changed);
    if (IsA<HTMLButtonElement>(change.sibling_changed)) {
      invalidate_button = true;
    }
  } else if (change.type == ChildrenChangeType::kElementRemoved) {
    if (auto* option = DynamicTo<HTMLOptionElement>(change.sibling_changed)) {
      OptionRemoved(*option);
    } else if (auto* optgroup =
                   DynamicTo<HTMLOptGroupElement>(change.sibling_changed)) {
      for (auto& child_option :
           Traversal<HTMLOptionElement>::ChildrenOf(*optgroup))
        OptionRemoved(child_option);
    } else if (IsA<HTMLButtonElement>(change.sibling_changed)) {
      invalidate_button = true;
    }
  } else if (change.type == ChildrenChangeType::kAllChildrenRemoved) {
    for (Node* node : change.removed_nodes) {
      if (auto* option = DynamicTo<HTMLOptionElement>(node)) {
        OptionRemoved(*option);
      } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(node)) {
        for (auto& child_option :
             Traversal<HTMLOptionElement>::ChildrenOf(*optgroup))
          OptionRemoved(child_option);
      } else if (IsA<HTMLButtonElement>(node)) {
        invalidate_button = true;
      }
    }
  }

  if (invalidate_button &&
      RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    GetShadowRoot()->SetDelegatesFocus(IsAppearanceBaseButton() &&
                                       SlottedButton());
    PseudoStateChanged(CSSSelector::kPseudoSelectHasChildButton);
  }
}

bool HTMLSelectElement::ChildrenChangedAllChildrenRemovedNeedsList() const {
  return true;
}

void HTMLSelectElement::ElementInserted(Node& node) {
  if (auto* option = DynamicTo<HTMLOptionElement>(&node)) {
    OptionInserted(*option, option->Selected());
  } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(&node)) {
    for (auto& child_option :
         Traversal<HTMLOptionElement>::ChildrenOf(*optgroup)) {
      OptionInserted(child_option, child_option.Selected());
    }
  }
}

void HTMLSelectElement::OptionInserted(HTMLOptionElement& option,
                                       bool option_is_selected) {
  DCHECK_EQ(option.OwnerSelectElement(), this);
  option.SetWasOptionInsertedCalled(true);
  SetRecalcListItems();
  if (option_is_selected) {
    SelectOption(&option, IsMultiple() ? 0 : kDeselectOtherOptionsFlag);
  } else if (!last_on_change_option_) {
    // The newly added option is not selected and we do not already have a
    // selected option. We should re-run the selection algorithm if there is a
    // chance that the newly added option can become the selected option.
    // However, we should not re-run the algorithm if either of these is true:
    //
    // 1. The new option is disabled because disabled options can never be
    // selected.
    // 2. The size attribute is greater than 1 because the HTML spec does not
    // mention a default value for that case.
    //
    // https://html.spec.whatwg.org/multipage/form-elements.html#selectedness-setting-algorithm
    if (size_ <= 1 && !option.IsDisabledFormControl()) {
      ResetToDefaultSelection();
    }
  }
  SetNeedsValidityCheck();
  select_type_->ClearLastOnChangeSelection();

  if (!GetDocument().IsActive())
    return;

  GetDocument()
      .GetFrame()
      ->GetPage()
      ->GetChromeClient()
      .SelectFieldOptionsChanged(*this);
}

void HTMLSelectElement::OptionRemoved(HTMLOptionElement& option) {
  option.SetWasOptionInsertedCalled(false);
  SetRecalcListItems();
  if (option.Selected())
    ResetToDefaultSelection(kResetReasonSelectedOptionRemoved);
  else if (!last_on_change_option_)
    ResetToDefaultSelection();
  if (last_on_change_option_ == &option)
    last_on_change_option_.Clear();
  select_type_->OptionRemoved(option);
  if (suggested_option_ == &option)
    SetSuggestedOption(nullptr);
  if (option.Selected())
    SetAutofillState(WebAutofillState::kNotFilled);
  SetNeedsValidityCheck();
  select_type_->ClearLastOnChangeSelection();

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
  select_type_->ClearLastOnChangeSelection();
}

void HTMLSelectElement::HrInsertedOrRemoved(HTMLHRElement& hr) {
  SetRecalcListItems();
  select_type_->ClearLastOnChangeSelection();
}

// TODO(tkent): This function is not efficient.  It contains multiple O(N)
// operations. crbug.com/577989.
void HTMLSelectElement::SelectOption(HTMLOptionElement* element,
                                     SelectOptionFlags flags,
                                     WebAutofillState autofill_state) {
  TRACE_EVENT0("blink", "HTMLSelectElement::selectOption");

  bool should_update_popup = false;

  SetAutofillState(element ? autofill_state : WebAutofillState::kNotFilled);

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

  select_type_->DidSelectOption(element, flags, should_update_popup);
  NotifyFormStateChanged();

  if (LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()) &&
      GetDocument().IsActive()) {
    GetDocument()
        .GetPage()
        ->GetChromeClient()
        .DidChangeSelectionInSelectControl(*this);
  }

  if (!RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled()) {
    // We set the Autofilled state again because setting the autofill value
    // triggers JavaScript events and the site may override the autofilled
    // value, which resets the autofill state. Even if the website modifies the
    // from control element's content during the autofill operation, we want the
    // state to show as as autofilled.
    SetAutofillState(element ? autofill_state : WebAutofillState::kNotFilled);
  }

  if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    for (HTMLSelectedOptionElement* selectedoption : TargetSelectedOptions()) {
      selectedoption->CloneContentsFromOptionElement(element);
    }
  }
}

bool HTMLSelectElement::DispatchFocusEvent(
    Element* old_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  // Save the selection so it can be compared to the new selection when
  // dispatching change events during blur event dispatch.
  if (UsesMenuList())
    select_type_->SaveLastSelection();
  return HTMLFormControlElementWithState::DispatchFocusEvent(
      old_focused_element, type, source_capabilities);
}

void HTMLSelectElement::DispatchBlurEvent(
    Element* new_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  type_ahead_.ResetSession();
  select_type_->DidBlur();
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
    if (option == exclude_element)
      continue;
    if (!option->WasOptionInsertedCalled())
      continue;
    if (option->Selected())
      did_update_selection = true;
    option->SetSelectedState(false);
  }
  return did_update_selection;
}

FormControlState HTMLSelectElement::SaveFormControlState() const {
  const ListItems& items = GetListItems();
  wtf_size_t length = items.size();
  FormControlState state;
  for (wtf_size_t i = 0; i < length; ++i) {
    auto* option = DynamicTo<HTMLOptionElement>(items[i].Get());
    if (!option || !option->Selected())
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
    auto* option_element = DynamicTo<HTMLOptionElement>(items[i].Get());
    if (!option_element)
      continue;
    if (option_element->value() == value)
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
    auto* option_element =
        index < items_size ? DynamicTo<HTMLOptionElement>(items[index].Get())
                           : nullptr;
    if (option_element && option_element->value() == state[0]) {
      option_element->SetSelectedState(true);
      option_element->SetDirty(true);
      last_on_change_option_ = option_element;
    } else {
      wtf_size_t found_index = SearchOptionsForValue(state[0], 0, items_size);
      if (found_index != kNotFound) {
        auto* found_option_element =
            To<HTMLOptionElement>(items[found_index].Get());
        found_option_element->SetSelectedState(true);
        found_option_element->SetDirty(true);
        last_on_change_option_ = found_option_element;
      }
    }
  } else {
    wtf_size_t start_index = 0;
    for (wtf_size_t i = 0; i < state.ValueSize(); i += 2) {
      const String& value = state[i];
      const unsigned index = state[i + 1].ToUInt();
      auto* option_element =
          index < items_size ? DynamicTo<HTMLOptionElement>(items[index].Get())
                             : nullptr;
      if (option_element && option_element->value() == value) {
        option_element->SetSelectedState(true);
        option_element->SetDirty(true);
        start_index = index + 1;
      } else {
        wtf_size_t found_index =
            SearchOptionsForValue(value, start_index, items_size);
        if (found_index == kNotFound)
          found_index = SearchOptionsForValue(value, 0, start_index);
        if (found_index == kNotFound)
          continue;
        auto* found_option_element =
            To<HTMLOptionElement>(items[found_index].Get());
        found_option_element->SetSelectedState(true);
        found_option_element->SetDirty(true);
        start_index = found_index + 1;
      }
    }
  }

  SetNeedsValidityCheck();
  select_type_->UpdateTextStyleAndContent();
}

void HTMLSelectElement::ParseMultipleAttribute(const AtomicString& value) {
  bool old_multiple = is_multiple_;
  HTMLOptionElement* old_selected_option = SelectedOption();
  is_multiple_ = !value.IsNull();
  SetNeedsValidityCheck();
  ChangeRendering();
  UpdateUserAgentShadowTree(*UserAgentShadowRoot());
  // Restore selectedIndex after changing the multiple flag to preserve
  // selection as single-line and multi-line has different defaults.
  if (old_multiple != is_multiple_) {
    // Preserving the first selection is compatible with Firefox and
    // WebKit. However Edge seems to "ask for a reset" simply.  As of 2016
    // March, the HTML specification says nothing about this.
    if (old_selected_option) {
      // Clear last_on_change_option_ in order to disable an optimization in
      // DeselectItemsWithoutValidation().
      last_on_change_option_ = nullptr;
      SelectOption(old_selected_option, kDeselectOtherOptionsFlag);
    } else {
      ResetToDefaultSelection();
    }
  }
  select_type_->UpdateTextStyleAndContent();
}

void HTMLSelectElement::AppendToFormData(FormData& form_data) {
  const AtomicString& name = GetName();
  if (name.empty())
    return;

  for (auto* const option : GetOptionList()) {
    if (option->Selected() && !option->IsDisabledFormControl())
      form_data.AppendFromElement(name, option->value());
  }
}

void HTMLSelectElement::ResetImpl() {
  for (auto* const option : GetOptionList()) {
    option->SetSelectedState(
        option->FastHasAttribute(html_names::kSelectedAttr));
    option->SetDirty(false);
  }
  ResetToDefaultSelection();
  select_type_->UpdateTextStyleAndContent();
  SetNeedsValidityCheck();
  HTMLFormControlElementWithState::ResetImpl();
}

bool HTMLSelectElement::PopupIsVisible() const {
  return select_type_->PopupIsVisible();
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

LayoutBox* HTMLSelectElement::AutoscrollBox() {
  return !UsesMenuList() ? GetLayoutBox() : nullptr;
}

void HTMLSelectElement::StopAutoscroll() {
  if (!IsDisabledFormControl())
    select_type_->HandleMouseRelease();
}

void HTMLSelectElement::DefaultEventHandler(Event& event) {
  if (!GetLayoutObject())
    return;

  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kChange ||
      event.type() == event_type_names::kKeydown) {
    SetUserHasEditedTheField();
  }

  if (IsDisabledFormControl()) {
    HTMLFormControlElementWithState::DefaultEventHandler(event);
    return;
  }

  if (select_type_->DefaultEventHandler(event)) {
    event.SetDefaultHandled();
    return;
  }

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeypress && keyboard_event) {
    if (!keyboard_event->ctrlKey() && !keyboard_event->altKey() &&
        !keyboard_event->metaKey() &&
        WTF::unicode::IsPrintableChar(keyboard_event->charCode())) {
      TypeAheadFind(*keyboard_event);
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
      event, event.charCode(),
      TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);
  if (index < 0) {
    return;
  }

  HTMLOptionElement* option_at_index = OptionAtListIndex(index);

  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      select_type_->IsAppearanceBasePicker() &&
      select_type_->PopupIsVisible()) {
    option_at_index->Focus(FocusParams(FocusTrigger::kScript));
    return;
  }

  SelectOption(option_at_index, kDeselectOtherOptionsFlag |
                                    kMakeOptionDirtyFlag |
                                    kDispatchInputAndChangeEventFlag);

  select_type_->ListBoxOnChange();
}

void HTMLSelectElement::SelectOptionByAccessKey(HTMLOptionElement* option) {
  // First bring into focus the list box.
  if (!IsFocused())
    AccessKeyAction(SimulatedClickCreationScope::kFromUserAgent);

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
  select_type_->ListBoxOnChange();
  select_type_->ScrollToSelection();
}

unsigned HTMLSelectElement::length() const {
  unsigned options = 0;
  for ([[maybe_unused]] auto* const option : GetOptionList()) {
    ++options;
  }
  return options;
}

void HTMLSelectElement::FinishParsingChildren() {
  HTMLFormControlElementWithState::FinishParsingChildren();
  if (UsesMenuList())
    return;
  select_type_->ScrollToOption(SelectedOption());
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ListboxActiveIndexChanged(this);
}

IndexedPropertySetterResult HTMLSelectElement::AnonymousIndexedSetter(
    unsigned index,
    HTMLOptionElement* value,
    ExceptionState& exception_state) {
  if (!value) {  // undefined or null
    remove(index);
    return IndexedPropertySetterResult::kIntercepted;
  }
  SetOption(index, value, exception_state);
  return IndexedPropertySetterResult::kIntercepted;
}

bool HTMLSelectElement::IsInteractiveContent() const {
  return true;
}

void HTMLSelectElement::Trace(Visitor* visitor) const {
  visitor->Trace(list_items_);
  visitor->Trace(option_slot_);
  visitor->Trace(last_on_change_option_);
  visitor->Trace(suggested_option_);
  visitor->Trace(descendant_selectedoptions_);
  visitor->Trace(select_type_);
  visitor->Trace(descendants_observer_);
  HTMLFormControlElementWithState::Trace(visitor);
}

void HTMLSelectElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  UpdateUserAgentShadowTree(root);
  select_type_->UpdateTextStyleAndContent();
}

void HTMLSelectElement::ManuallyAssignSlots() {
  select_type_->ManuallyAssignSlots();
}

void HTMLSelectElement::UpdateUserAgentShadowTree(ShadowRoot& root) {
  // Remove all children of the ShadowRoot so that select_type_ can set it up
  // however it wants.
  Node* node = root.firstChild();
  while (node) {
    auto* will_be_removed = node;
    node = node->nextSibling();
    will_be_removed->remove();
  }
  select_type_->CreateShadowSubtree(root);
}

Element& HTMLSelectElement::InnerElement() const {
  return select_type_->InnerElement();
}

AXObject* HTMLSelectElement::PopupRootAXObject() const {
  return select_type_->PopupRootAXObject();
}

HTMLOptionElement* HTMLSelectElement::SpatialNavigationFocusedOption() {
  return select_type_->SpatialNavigationFocusedOption();
}

String HTMLSelectElement::ItemText(const Element& element) const {
  String item_string;
  if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(element))
    item_string = optgroup->GroupLabelText();
  else if (auto* option = DynamicTo<HTMLOptionElement>(element))
    item_string = option->TextIndentedToRespectGroupLabel();

  if (GetLayoutObject() && GetLayoutObject()->Style()) {
    return GetLayoutObject()->Style()->ApplyTextTransform(item_string);
  }
  return item_string;
}

bool HTMLSelectElement::ItemIsDisplayNone(Element& element) const {
  if (auto* option = DynamicTo<HTMLOptionElement>(element))
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
  DCHECK(UsesMenuList());
  auto* this_box = GetLayoutBox();
  if (!this_box || !InnerElement().GetLayoutBox()) {
    return LayoutUnit();
  }
  LayoutTheme& theme = LayoutTheme::GetTheme();
  const ComputedStyle& style = this_box->StyleRef();
  int inner_padding =
      style.IsLeftToRightDirection()
          ? theme.PopupInternalPaddingStart(style)
          : theme.PopupInternalPaddingEnd(GetDocument().GetFrame(), style);
  return this_box->PaddingLeft() + inner_padding;
}

LayoutUnit HTMLSelectElement::ClientPaddingRight() const {
  DCHECK(UsesMenuList());
  auto* this_box = GetLayoutBox();
  if (!this_box || !InnerElement().GetLayoutBox()) {
    return LayoutUnit();
  }
  LayoutTheme& theme = LayoutTheme::GetTheme();
  const ComputedStyle& style = this_box->StyleRef();
  int inner_padding =
      style.IsLeftToRightDirection()
          ? theme.PopupInternalPaddingEnd(GetDocument().GetFrame(), style)
          : theme.PopupInternalPaddingStart(style);
  return this_box->PaddingRight() + inner_padding;
}

void HTMLSelectElement::PopupDidHide() {
  select_type_->PopupDidHide();
}

void HTMLSelectElement::SetIndexToSelectOnCancel(int list_index) {
  index_to_select_on_cancel_ = list_index;
  select_type_->UpdateTextStyleAndContent();
}

HTMLOptionElement* HTMLSelectElement::OptionToBeShown() const {
  DCHECK(!IsMultiple());
  return select_type_->OptionToBeShown();
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
  select_type_->ShowPopup(PopupMenu::kOther);
}

void HTMLSelectElement::HidePopup() {
  select_type_->HidePopup();
}

PopupMenu* HTMLSelectElement::PopupForTesting() const {
  return select_type_->PopupForTesting();
}

void HTMLSelectElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLFormControlElementWithState::DidRecalcStyle(change);
  if (auto* style = GetComputedStyle()) {
    if (style->EffectiveAppearance() == ControlPart::kNoControlPart) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kSelectElementAppearanceNone);
    }
  }
  select_type_->DidRecalcStyle(change);
}

void HTMLSelectElement::AttachLayoutTree(AttachContext& context) {
  HTMLFormControlElementWithState::AttachLayoutTree(context);
  // The call to UpdateTextStyle() needs to go after the call through
  // to the base class's AttachLayoutTree() because that can sometimes do a
  // close on the LayoutObject.
  select_type_->UpdateTextStyle();

  if (const ComputedStyle* style = GetComputedStyle()) {
    if (style->UsedVisibility() != EVisibility::kHidden) {
      if (IsMultiple())
        UseCounter::Count(GetDocument(), WebFeature::kSelectElementMultiple);
      else
        UseCounter::Count(GetDocument(), WebFeature::kSelectElementSingle);
    }
  }
}

void HTMLSelectElement::DetachLayoutTree(bool performing_reattach) {
  HTMLFormControlElementWithState::DetachLayoutTree(performing_reattach);
  select_type_->DidDetachLayoutTree();
}

void HTMLSelectElement::ResetTypeAheadSessionForTesting() {
  type_ahead_.ResetSession();
}

void HTMLSelectElement::CloneNonAttributePropertiesFrom(const Element& source,
                                                        NodeCloningData& data) {
  const auto& source_element = static_cast<const HTMLSelectElement&>(source);
  interacted_state_ = source_element.interacted_state_;
  HTMLFormControlElement::CloneNonAttributePropertiesFrom(source, data);
}

void HTMLSelectElement::ChangeRendering() {
  select_type_->DidDetachLayoutTree();
  bool old_uses_menu_list = UsesMenuList();
  UpdateUsesMenuList();
  if (UsesMenuList() != old_uses_menu_list) {
    select_type_->WillBeDestroyed();
    select_type_ = SelectType::Create(*this);

    if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
      // Make <option>s render all child content when in MenuList mode in order
      // to support appearance:base-select.
      for (HTMLOptionElement* option : GetOptionList()) {
        option->SetTextOnlyRendering(!UsesMenuList());
      }
    }
  }
  if (!InActiveDocument())
    return;
  SetForceReattachLayoutTree();
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kControl));
}

const ComputedStyle* HTMLSelectElement::OptionStyle() const {
  return select_type_->OptionStyle();
}

// Show the option list for this select element.
// https://html.spec.whatwg.org/multipage/input.html#dom-select-showpicker
void HTMLSelectElement::showPicker(ExceptionState& exception_state) {
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  // In cross-origin iframes it should throw a "SecurityError" DOMException
  if (frame) {
    if (!frame->IsSameOrigin()) {
      exception_state.ThrowSecurityError(
          "showPicker() called from cross-origin iframe.");
      return;
    }
  }

  if (IsDisabledFormControl()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "showPicker() cannot "
                                      "be used on immutable controls.");
    return;
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "showPicker() requires a user gesture.");
    return;
  }

  document.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*this) ||
      !GetLayoutBox()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "showPicker() requires the select is rendered.");
    return;
  }

  LocalFrame::ConsumeTransientUserActivation(frame);

  select_type_->ShowPicker();
}

bool HTMLSelectElement::IsValidCommand(HTMLElement& invoker,
                                       CommandEventType command) {
  bool parent_is_valid = HTMLElement::IsValidCommand(invoker, command);
  if (!RuntimeEnabledFeatures::HTMLInvokeActionsV2Enabled()) {
    return parent_is_valid;
  }
  return parent_is_valid || command == CommandEventType::kShowPicker;
}

bool HTMLSelectElement::HandleCommandInternal(HTMLElement& invoker,
                                              CommandEventType command) {
  CHECK(IsValidCommand(invoker, command));

  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }

  if (command != CommandEventType::kShowPicker) {
    return false;
  }

  // Step 1. If this is not mutable, then return.
  if (IsDisabledFormControl()) {
    return false;
  }

  // Step 2. If this's relevant settings object's origin is not same origin with
  // this's relevant settings object's top-level origin, [...], then return.
  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();
  if (frame && !frame->IsSameOrigin()) {
    String message = "Select cannot be invoked from cross-origin iframe.";
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return false;
  }

  // If this's relevant global object does not have transient
  // activation, then return.
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    String message = "Select cannot be invoked without a user gesture.";
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return false;
  }

  document.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*this) ||
      !GetLayoutBox()) {
    String message = "Select cannot be invoked when not being rendered.";
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return false;
  }

  // Step 3. ... show the picker, if applicable, for this.
  select_type_->ShowPicker();

  return true;
}

HTMLButtonElement* HTMLSelectElement::SlottedButton() const {
  return select_type_->SlottedButton();
}

HTMLElement* HTMLSelectElement::PopoverForAppearanceBase() const {
  return select_type_->PopoverForAppearanceBase();
}

// static
bool HTMLSelectElement::IsPopoverForAppearanceBase(const Element* element) {
  if (auto* root = DynamicTo<ShadowRoot>(element->parentNode())) {
    return IsA<HTMLSelectElement>(root->host()) &&
           element->FastHasAttribute(html_names::kPopoverAttr);
  }
  return false;
}

bool HTMLSelectElement::IsAppearanceBaseButton() const {
  return select_type_->IsAppearanceBaseButton();
}

bool HTMLSelectElement::IsAppearanceBasePicker() const {
  return select_type_->IsAppearanceBasePicker();
}

void HTMLSelectElement::SelectedOptionElementInserted(
    HTMLSelectedOptionElement* selectedoption) {
  descendant_selectedoptions_.insert(selectedoption);
  selectedoption->CloneContentsFromOptionElement(SelectedOption());
}

void HTMLSelectElement::SelectedOptionElementRemoved(
    HTMLSelectedOptionElement* selectedoption) {
  descendant_selectedoptions_.erase(selectedoption);
  selectedoption->CloneContentsFromOptionElement(nullptr);
}

FocusableState HTMLSelectElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsAppearanceBaseButton() && SlottedButton()) {
    // In appearance:base-select mode, the child button gets focus instead of
    // the select via delegatesfocus. We must return false here in order to make
    // the delegatesfocus focusing code find the child button.
    return FocusableState::kNotFocusable;
  }
  return HTMLFormControlElementWithState::SupportsFocus(update_behavior);
}

HTMLSelectElement::SelectAutofillPreviewElement*
HTMLSelectElement::GetAutofillPreviewElement() const {
  return select_type_->GetAutofillPreviewElement();
}

HTMLSelectElement::SelectAutofillPreviewElement::SelectAutofillPreviewElement(
    Document& document,
    HTMLSelectElement* select)
    : HTMLDivElement(document), select_(select) {
  CHECK(select_);
  SetHasCustomStyleCallbacks();
}

const ComputedStyle*
HTMLSelectElement::SelectAutofillPreviewElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  HTMLElement* button = select_->SlottedButton();
  if (!button) {
    button = select_;
  }
  if (!button || !button->GetComputedStyle()) {
    return HTMLDivElement::CustomStyleForLayoutObject(style_recalc_context);
  }

  const ComputedStyle& button_style = button->ComputedStyleRef();
  const ComputedStyle* original_style =
      OriginalStyleForLayoutObject(style_recalc_context);
  ComputedStyleBuilder style_builder(*original_style);
  if (button_style.HasAuthorBorderRadius()) {
    style_builder.SetBorderBottomLeftRadius(
        button_style.BorderBottomLeftRadius());
    style_builder.SetBorderBottomRightRadius(
        button_style.BorderBottomRightRadius());
    style_builder.SetBorderTopLeftRadius(button_style.BorderTopLeftRadius());
    style_builder.SetBorderTopRightRadius(button_style.BorderTopRightRadius());
  }
  if (button_style.HasAuthorBorder()) {
    style_builder.SetBorderColorFrom(button_style);

    style_builder.SetBorderBottomWidth(button_style.BorderBottomWidth());
    style_builder.SetBorderLeftWidth(button_style.BorderLeftWidth());
    style_builder.SetBorderRightWidth(button_style.BorderRightWidth());
    style_builder.SetBorderTopWidth(button_style.BorderTopWidth());

    style_builder.SetBorderBottomStyle(button_style.BorderBottomStyle());
    style_builder.SetBorderLeftStyle(button_style.BorderLeftStyle());
    style_builder.SetBorderRightStyle(button_style.BorderRightStyle());
    style_builder.SetBorderTopStyle(button_style.BorderTopStyle());
  }

  return style_builder.TakeStyle();
}

Node::InsertionNotificationRequest
HTMLSelectElement::SelectAutofillPreviewElement::InsertedInto(
    ContainerNode& container) {
  select_->IncrementImplicitlyAnchoredElementCount();
  return HTMLDivElement::InsertedInto(container);
}

void HTMLSelectElement::SelectAutofillPreviewElement::RemovedFrom(
    ContainerNode& container) {
  HTMLDivElement::RemovedFrom(container);
  select_->DecrementImplicitlyAnchoredElementCount();
}

void HTMLSelectElement::SelectAutofillPreviewElement::Trace(
    Visitor* visitor) const {
  visitor->Trace(select_);
  HTMLDivElement::Trace(visitor);
}

HTMLSelectedOptionElement* HTMLSelectElement::selectedOptionElement() const {
  CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
  return DynamicTo<HTMLSelectedOptionElement>(
      GetElementAttribute(html_names::kSelectedoptionelementAttr));
}

void HTMLSelectElement::setSelectedOptionElement(
    HTMLSelectedOptionElement* new_selectedoption) {
  CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
  auto* old_selectedoption = selectedOptionElement();
  if (new_selectedoption) {
    SetElementAttribute(html_names::kSelectedoptionelementAttr,
                        new_selectedoption);
  }

  if (old_selectedoption != new_selectedoption) {
    if (old_selectedoption) {
      // Clear out the contents of any <selectedoption> which we are removing
      // the association from.
      old_selectedoption->CloneContentsFromOptionElement(nullptr);
    }
    if (new_selectedoption) {
      new_selectedoption->CloneContentsFromOptionElement(SelectedOption());
    }
  }
}

HeapHashSet<Member<HTMLSelectedOptionElement>>
HTMLSelectElement::TargetSelectedOptions() const {
  HeapHashSet<Member<HTMLSelectedOptionElement>> selectedoptions =
      descendant_selectedoptions_;
  if (auto* attr_selectedoption = selectedOptionElement()) {
    selectedoptions.insert(attr_selectedoption);
  }
  return selectedoptions;
}

}  // namespace blink
