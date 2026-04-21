/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLDataListElement::HTMLDataListElement(Document& document)
    : HTMLElement(html_names::kDatalistTag, document) {
  UseCounter::Count(document, WebFeature::kDataListElement);
  document.IncrementDataListCount();
  if (RuntimeEnabledFeatures::CustomizableComboboxEnabled()) {
    EnsurePopoverData().setType(PopoverValueType::kAuto);
  }
}

HTMLDataListOptionsCollection* HTMLDataListElement::options() {
  return EnsureCachedCollection<HTMLDataListOptionsCollection>(
      kDataListOptions);
}

void HTMLDataListElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (!change.ByParser()) {
    if (auto* registry = GetTreeScope().GetIdTargetObserverRegistry()) {
      registry->NotifyObservers(GetIdAttribute());
    }
  }
}

void HTMLDataListElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
  if (auto* registry = GetTreeScope().GetIdTargetObserverRegistry()) {
    registry->NotifyObservers(GetIdAttribute());
  }
}

void HTMLDataListElement::OptionElementChildrenChanged() {
  if (auto* registry = GetTreeScope().GetIdTargetObserverRegistry()) {
    registry->NotifyObservers(GetIdAttribute());
  }
}

bool HTMLDataListElement::SupportsBaseAppearanceInternal(
    BaseAppearanceValue value) const {
  if (!RuntimeEnabledFeatures::CustomizableComboboxEnabled()) {
    return false;
  }
  return value == BaseAppearanceValue::kBase;
}

void HTMLDataListElement::DidMoveToNewDocument(Document& old_doc) {
  HTMLElement::DidMoveToNewDocument(old_doc);
  old_doc.DecrementDataListCount();
  GetDocument().IncrementDataListCount();
}

void HTMLDataListElement::Prefinalize() {
  GetDocument().DecrementDataListCount();
}

PopoverHideResult HTMLDataListElement::HidePopoverInternal(
    Element* invoker,
    HidePopoverFocusBehavior focus_behavior,
    HidePopoverTransitionBehavior event_firing,
    ExceptionState* exception_state) {
  PopoverHideResult result = HTMLElement::HidePopoverInternal(
      invoker, focus_behavior, event_firing, exception_state);

  if (RuntimeEnabledFeatures::CustomizableComboboxEnabled() &&
      result != PopoverHideResult::kForcedOpenByInspector && active_option_) {
    active_option_->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
    active_option_ = nullptr;
  }

  return result;
}

void HTMLDataListElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
  visitor->Trace(active_option_);
}

void HTMLDataListElement::MoveActiveOption(Direction direction) {
  CHECK(RuntimeEnabledFeatures::CustomizableComboboxEnabled());
  CHECK(IsAppearanceBase());

  auto* option_list = options();
  const unsigned length = option_list->length();
  if (length == 0) {
    return;
  }

  auto update_active_option = [&](HTMLOptionElement* new_active_option) {
    HTMLOptionElement* old_active_option = active_option_;
    active_option_ = new_active_option;
    if (old_active_option) {
      old_active_option->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
    }
    new_active_option->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
    new_active_option->scrollIntoViewIfNeeded(/*center_if_needed=*/false);
  };

  unsigned active_option_index = length;
  if (active_option_) {
    for (unsigned i = 0; i < length; ++i) {
      if (option_list->Item(i) == active_option_) {
        active_option_index = i;
        break;
      }
    }
  }

  // If there is no active_option_ or active_option_ is no longer in
  // option_list, then just make the first one in the list become the active
  // option.
  if (active_option_index == length) {
    for (unsigned i = 0; i < length; ++i) {
      auto* option = To<HTMLOptionElement>(option_list->Item(i));
      if (option && option->SupportsActiveOptionPseudo()) {
        update_active_option(option);
        return;
      }
    }
    return;
  }

  unsigned index = active_option_index;
  for (unsigned count = 0; count < length; ++count) {
    if (direction == Direction::kForwards) {
      index = (index + 1) % length;
    } else {
      index = (index == 0) ? length - 1 : index - 1;
    }

    if (index == active_option_index) {
      return;
    }

    auto* next_option = To<HTMLOptionElement>(option_list->Item(index));
    if (next_option && next_option->SupportsActiveOptionPseudo()) {
      update_active_option(next_option);
      return;
    }
  }
}

HTMLInputElement* HTMLDataListElement::ComboboxInput() {
  if (!RuntimeEnabledFeatures::CustomizableComboboxEnabled()) {
    return nullptr;
  }

  if (PopoverData* popover_data = GetPopoverData()) {
    if (auto* input = DynamicTo<HTMLInputElement>(popover_data->invoker())) {
      if (input->DataList() == this && IsAppearanceBase() &&
          input->IsAppearanceBase()) {
        return input;
      }
    }
  }

  return nullptr;
}

}  // namespace blink
