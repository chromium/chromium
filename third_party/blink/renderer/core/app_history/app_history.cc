// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history.h"

#include "third_party/blink/renderer/core/app_history/app_history_navigate_event.h"
#include "third_party/blink/renderer/core/app_history/app_history_navigate_event_init.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"

namespace blink {

const char AppHistory::kSupplementName[] = "AppHistory";

AppHistory* AppHistory::appHistory(LocalDOMWindow& window) {
  if (!RuntimeEnabledFeatures::AppHistoryEnabled())
    return nullptr;
  auto* app_history = Supplement<LocalDOMWindow>::From<AppHistory>(window);
  if (!app_history) {
    app_history = MakeGarbageCollected<AppHistory>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, app_history);
  }
  return app_history;
}

AppHistory::AppHistory(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

bool AppHistory::DispatchNavigateEvent(const KURL& url,
                                       HTMLFormElement* form,
                                       bool same_document,
                                       WebFrameLoadType type,
                                       UserNavigationInvolvement involvement,
                                       SerializedScriptValue* state_object) {
  const KURL& current_url = GetSupplementable()->Url();

  auto* init = AppHistoryNavigateEventInit::Create();
  init->setCancelable(involvement != UserNavigationInvolvement::kBrowserUI ||
                      type != WebFrameLoadType::kBackForward);
  init->setCanRespond(
      CanChangeToUrlForHistoryApi(url, GetSupplementable()->GetSecurityOrigin(),
                                  current_url) &&
      (same_document || type != WebFrameLoadType::kBackForward));
  init->setHashChange(same_document && url != current_url &&
                      EqualIgnoringFragmentIdentifier(url, current_url));
  init->setUserInitiated(involvement != UserNavigationInvolvement::kNone);
  init->setFormData(form ? FormData::Create(form, ASSERT_NO_EXCEPTION)
                         : nullptr);
  auto* navigate_event = AppHistoryNavigateEvent::Create(
      GetSupplementable(), event_type_names::kNavigate, init);
  navigate_event->SetUrl(url);
  navigate_event->SetFrameLoadType(type);
  navigate_event->SetStateObject(state_object);

  DispatchEvent(*navigate_event);
  return !navigate_event->defaultPrevented();
}

const AtomicString& AppHistory::InterfaceName() const {
  return event_target_names::kAppHistory;
}

void AppHistory::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
