// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

namespace blink {

NavigationHistoryEntry::NavigationHistoryEntry(ExecutionContext* context,
                                               HistoryItem* item)
    : ExecutionContextClient(context), item_(item) {}

String NavigationHistoryEntry::key() const {
  return DomWindow() ? item_->GetNavigationApiKey() : String();
}

String NavigationHistoryEntry::id() const {
  return DomWindow() ? item_->GetNavigationApiId() : String();
}

int64_t NavigationHistoryEntry::index() {
  return DomWindow()
             ? NavigationApi::navigation(*DomWindow())->GetIndexFor(this)
             : -1;
}

KURL NavigationHistoryEntry::url() {
  return DomWindow() && !item_->Url().IsEmpty() ? item_->Url() : NullURL();
}

bool NavigationHistoryEntry::sameDocument() const {
  if (!DomWindow())
    return false;
  HistoryItem* current_item =
      DomWindow()->document()->Loader()->GetHistoryItem();
  return current_item->DocumentSequenceNumber() ==
         item_->DocumentSequenceNumber();
}

ScriptValue NavigationHistoryEntry::getState() const {
  SerializedScriptValue* state = item_->GetNavigationApiState();
  if (!DomWindow() || !state)
    return ScriptValue();
  v8::Isolate* isolate = DomWindow()->GetIsolate();
  return ScriptValue(isolate, state->Deserialize(isolate));
}

const AtomicString& NavigationHistoryEntry::InterfaceName() const {
  return event_target_names::kNavigationHistoryEntry;
}

void NavigationHistoryEntry::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(item_);
}

}  // namespace blink
