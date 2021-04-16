// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history.h"

#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
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

void AppHistory::InitializeForNavigation(
    HistoryItem& current,
    const WebVector<WebHistoryItem>& back_entries,
    const WebVector<WebHistoryItem>& forward_entries) {
  DCHECK(entries_.IsEmpty());

  // Construct |entries_|. Any back entries are inserted, then the current
  // entry, then any forward entries.
  entries_.ReserveCapacity(back_entries.size() + forward_entries.size() + 1);
  for (const auto& entry : back_entries) {
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), entry));
  }

  current_index_ = back_entries.size();
  entries_.emplace_back(
      MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), &current));

  for (const auto& entry : forward_entries) {
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), entry));
  }
}

void AppHistory::CloneFromPrevious(AppHistory& previous) {
  DCHECK(entries_.IsEmpty());
  entries_.ReserveCapacity(previous.entries_.size());
  for (size_t i = 0; i < previous.entries_.size(); i++) {
    // It's possible that |old_item| is indirectly holding a reference to
    // the old Document. Also, it has a bunch of state we don't need for a
    // non-current entry. Clone a subset of its state to a |new_item|.
    HistoryItem* old_item = previous.entries_[i]->GetItem();
    HistoryItem* new_item = MakeGarbageCollected<HistoryItem>();
    new_item->SetItemSequenceNumber(old_item->ItemSequenceNumber());
    new_item->SetDocumentSequenceNumber(old_item->DocumentSequenceNumber());
    new_item->SetURL(old_item->Url());
    entries_.emplace_back(
        MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), new_item));
  }
  current_index_ = previous.current_index_;
}

void AppHistory::UpdateForNavigation(HistoryItem& item, WebFrameLoadType type) {
  // A same-document navigation (e.g., a document.open()) in a newly created
  // iframe will try to operate on an empty |entries_|. appHistory considers
  // this a no-op.
  if (entries_.IsEmpty())
    return;

  if (type == WebFrameLoadType::kBackForward) {
    // If this is a same-document back/forward navigation, the new current_
    // should already be present in entries_.
    // We just need to update current_index_ to its index, so find it.
    size_t new_current_index = 0;
    for (; new_current_index < entries_.size(); new_current_index++) {
      if (entries_[new_current_index]->key() == item.GetAppHistoryKey())
        break;
    }
    DCHECK_LT(new_current_index, entries_.size());
    current_index_ = new_current_index;
    return;
  }

  if (type == WebFrameLoadType::kStandard) {
    // For a new back/forward entry, truncate any forward entries and prepare to
    // append.
    current_index_++;
    entries_.resize(current_index_ + 1);
  }

  // current_index_ is now correctly set (for type of
  // WebFrameLoadType::kReplaceCurrentItem/kReload/kReloadBypassingCache, it
  // didn't change). Create the new current entry.
  entries_[current_index_] =
      MakeGarbageCollected<AppHistoryEntry>(GetSupplementable(), &item);
}

AppHistoryEntry* AppHistory::current() const {
  // current_index_ is initialized to -1 and set >= 0 when entries_ is
  // populated. It will still be negative if the appHistory of an initial empty
  // document is accessed.
  return current_index_ >= 0 && GetSupplementable()->GetFrame()
             ? entries_[current_index_]
             : nullptr;
}

HeapVector<Member<AppHistoryEntry>> AppHistory::entries() {
  return GetSupplementable()->GetFrame()
             ? entries_
             : HeapVector<Member<AppHistoryEntry>>();
}

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
  return navigate_event->Fire(this, same_document);
}

const AtomicString& AppHistory::InterfaceName() const {
  return event_target_names::kAppHistory;
}

void AppHistory::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(entries_);
}

}  // namespace blink
