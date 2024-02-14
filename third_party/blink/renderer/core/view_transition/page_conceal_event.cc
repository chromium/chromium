// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/page_conceal_event.h"

#include "third_party/blink/public/common/page_state/page_state.h"
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

String TypeToString(mojom::blink::NavigationTypeForNavigationApi type) {
  static_assert(static_cast<int>(
                    mojom::blink::NavigationTypeForNavigationApi::kPush) == 0);
  static_assert(static_cast<int>(
                    mojom::blink::NavigationTypeForNavigationApi::kTraverse) ==
                1);
  static_assert(static_cast<int>(
                    mojom::blink::NavigationTypeForNavigationApi::kReplace) ==
                2);
  static_assert(
      static_cast<int>(mojom::blink::NavigationTypeForNavigationApi::kReload) ==
      3);

  DEFINE_STATIC_LOCAL(Vector<String>, names,
                      ({"push", "traverse", "replace", "reload"}));
  return names[static_cast<int>(type)];
}

}  // namespace

PageConcealEvent::PageConcealEvent(
    Document& document,
    mojom::blink::PageConcealEventParamsPtr page_conceal_event_params,
    DOMViewTransition* view_transition)
    : Event(event_type_names::kPageconceal, Bubbles::kNo, Cancelable::kNo),
      dom_view_transition_(view_transition) {
  CHECK(RuntimeEnabledFeatures::PageConcealEventEnabled());
  CHECK(!view_transition ||
        RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  CHECK(!view_transition || page_conceal_event_params);

  if (page_conceal_event_params) {
    NavigationApi* navigation = document.domWindow()->navigation();

    // The current entry could be null for the initial about:blank Document, a
    // detached window, or an opaque origin. We shouldn't be creating the
    // activation info for the first 2 cases:
    // 1. We don't fire `pageconceal` on the initial about:blank Document.
    // 2. We shouldn't be firing `pageconceal` for detached windows. The event
    //    only fires when navigating away from a Document and there shouldn't be
    //    navigations in a detached window, i.e., a disconnected iframe.
    // 3. The activation info is only provided for same-origin navigations. An
    //    opaque origin shouldn't be same origin with another opaque origin.
    auto* from = navigation->currentEntry();
    CHECK(from);

    NavigationHistoryEntry* entry = nullptr;
    switch (page_conceal_event_params->navigation_type) {
      case mojom::blink::NavigationTypeForNavigationApi::kReload:
        entry = from;
        break;
      case mojom::blink::NavigationTypeForNavigationApi::kTraverse: {
        // This shouldn't be null but we can't assert because that may happen in
        // rare race conditions.
        Member<HistoryItem> destination_item =
            HistoryItem::Create(PageState::CreateFromEncodedData(
                page_conceal_event_params->page_state));
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
            /*url=*/page_conceal_event_params->url,
            /*document_sequence_number=*/0,
            /*state=*/nullptr);
    }

    activation_ = MakeGarbageCollected<NavigationActivation>();
    activation_->Update(
        entry, from, TypeToString(page_conceal_event_params->navigation_type));
  }
}

PageConcealEvent::~PageConcealEvent() = default;

const AtomicString& PageConcealEvent::InterfaceName() const {
  return event_interface_names::kPageConcealEvent;
}

void PageConcealEvent::Trace(Visitor* visitor) const {
  visitor->Trace(activation_);
  visitor->Trace(dom_view_transition_);
  Event::Trace(visitor);
}

DOMViewTransition* PageConcealEvent::viewTransition() const {
  return dom_view_transition_.Get();
}

NavigationActivation* PageConcealEvent::activation() const {
  return activation_.Get();
}

}  // namespace blink
