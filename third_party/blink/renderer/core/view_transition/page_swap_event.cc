// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/page_swap_event.h"

#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_page_swap_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_activation.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

V8NavigationType::Enum TypeToEnum(
    mojom::blink::NavigationTypeForNavigationApi type) {
  switch (type) {
    case mojom::blink::NavigationTypeForNavigationApi::kPush:
      return V8NavigationType::Enum::kPush;
    case mojom::blink::NavigationTypeForNavigationApi::kTraverse:
      return V8NavigationType::Enum::kTraverse;
    case mojom::blink::NavigationTypeForNavigationApi::kReplace:
      return V8NavigationType::Enum::kReplace;
    case mojom::blink::NavigationTypeForNavigationApi::kReload:
      return V8NavigationType::Enum::kReload;
  }
  NOTREACHED();
}

}  // namespace

PageSwapEvent::PageSwapEvent(
    Document& document,
    mojom::blink::PageSwapEventParamsPtr page_swap_event_params,
    DOMViewTransition* view_transition)
    : Event(event_type_names::kPageswap, Bubbles::kNo, Cancelable::kNo),
      dom_view_transition_(view_transition) {
  CHECK(RuntimeEnabledFeatures::PageSwapEventEnabled());
  CHECK(!view_transition ||
        RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  CHECK(!view_transition || page_swap_event_params);

  if (page_swap_event_params) {
    NavigationApi* navigation = document.domWindow()->navigation();

    // The current entry can be null at this point when navigating away from the
    // initial empty document.
    // See https://html.spec.whatwg.org/#navigation-current-entry.
    auto* from = navigation->currentEntry();

    NavigationHistoryEntry* entry = nullptr;
    switch (page_swap_event_params->navigation_type) {
      case mojom::blink::NavigationTypeForNavigationApi::kReload:
        entry = from;
        break;
      case mojom::blink::NavigationTypeForNavigationApi::kTraverse: {
        // This shouldn't be null but we can't assert because that may happen in
        // rare race conditions.
        Member<HistoryItem> destination_item =
            HistoryItem::Create(PageState::CreateFromEncodedData(
                page_swap_event_params->page_state));
        entry = navigation->GetExistingEntryFor(
            destination_item->GetNavigationApiKey(),
            destination_item->GetNavigationApiId());
      } break;
      case mojom::blink::NavigationTypeForNavigationApi::kPush:
      case mojom::blink::NavigationTypeForNavigationApi::kReplace:
        entry = MakeGarbageCollected<NavigationHistoryEntry>(
            document.domWindow(),
            /*key=*/WTF::CreateCanonicalUUIDString(),
            /*id=*/WTF::CreateCanonicalUUIDString(),
            /*url=*/page_swap_event_params->url,
            /*document_sequence_number=*/0,
            /*state=*/nullptr);
    }

    activation_ = MakeGarbageCollected<NavigationActivation>();
    activation_->Update(entry, from,
                        TypeToEnum(page_swap_event_params->navigation_type));
  }
}

PageSwapEvent::PageSwapEvent(const AtomicString& type,
                             const PageSwapEventInit* initializer)
    : Event(type, initializer),
      activation_(initializer ? initializer->activation() : nullptr),
      dom_view_transition_(initializer ? initializer->viewTransition()
                                       : nullptr) {}

PageSwapEvent::~PageSwapEvent() = default;

const AtomicString& PageSwapEvent::InterfaceName() const {
  return event_interface_names::kPageSwapEvent;
}

void PageSwapEvent::Trace(Visitor* visitor) const {
  visitor->Trace(activation_);
  visitor->Trace(dom_view_transition_);
  Event::Trace(visitor);
}

DOMViewTransition* PageSwapEvent::viewTransition() const {
  return dom_view_transition_.Get();
}

NavigationActivation* PageSwapEvent::activation() const {
  return activation_.Get();
}

}  // namespace blink
