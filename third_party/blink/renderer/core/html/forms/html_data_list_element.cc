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

void HTMLDataListElement::ShowPopoverInternal(Element* invoker,
                                              ExceptionState* exception_state) {
  HTMLElement::ShowPopoverInternal(invoker, exception_state);
  if (!RuntimeEnabledFeatures::CustomizableComboboxEnabled()) {
    return;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(invoker)) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kPopover);
    if (input->DataList() == this && input->IsAppearanceBase() &&
        IsAppearanceBase()) {
      for (Element* element_option : *options()) {
        HTMLOptionElement* option = To<HTMLOptionElement>(element_option);
        if (option->SupportsActiveOptionPseudo()) {
          CHECK(!active_option_);
          active_option_ = option;
          active_option_->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
          break;
        }
      }
    }
  }
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
  CHECK(active_option_);

  auto* option_list = options();
  unsigned active_option_index = 0;
  while (option_list->Item(active_option_index) != active_option_) {
    active_option_index++;
    if (active_option_index >= option_list->length()) {
      NOTREACHED() << " option is not in list: " << active_option_;
    }
  }

  unsigned index = active_option_index;
  while (true) {
    if (direction == Direction::kForwards) {
      index++;
      if (index == option_list->length()) {
        index = 0;
      }
    } else {
      if (index == 0) {
        index = option_list->length() - 1;
      } else {
        index--;
      }
    }
    if (index == active_option_index) {
      return;
    }

    HTMLOptionElement* next_option = option_list->Item(index);
    CHECK(next_option);
    if (next_option->SupportsActiveOptionPseudo()) {
      active_option_ = next_option;
      active_option_->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
      next_option->PseudoStateChanged(CSSSelector::kPseudoActiveOption);
      return;
    }
  }
}

}  // namespace blink
