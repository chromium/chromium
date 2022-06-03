// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_entry.h"

#include "third_party/blink/renderer/core/app_history/app_history.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink {

AppHistoryEntry::AppHistoryEntry(ExecutionContext* context, HistoryItem* item)
    : ExecutionContextClient(context), item_(item) {}

String AppHistoryEntry::key() const {
  return DomWindow() ? item_->GetAppHistoryKey() : String();
}

String AppHistoryEntry::id() const {
  return DomWindow() ? item_->GetAppHistoryId() : String();
}

int64_t AppHistoryEntry::index() {
  return DomWindow() ? AppHistory::appHistory(*DomWindow())->GetIndexFor(this)
                     : -1;
}

KURL AppHistoryEntry::url() {
  return DomWindow() ? item_->Url() : NullURL();
}

bool AppHistoryEntry::sameDocument() const {
  if (!DomWindow())
    return false;
  auto* current_item = DomWindow()->document()->Loader()->GetHistoryItem();
  return current_item->DocumentSequenceNumber() ==
         item_->DocumentSequenceNumber();
}

ScriptValue AppHistoryEntry::getState() const {
  SerializedScriptValue* state = item_->GetAppHistoryState();
  if (!DomWindow() || !state)
    return ScriptValue();
  v8::Isolate* isolate = DomWindow()->GetIsolate();
  return ScriptValue(isolate, state->Deserialize(isolate));
}

const AtomicString& AppHistoryEntry::InterfaceName() const {
  return event_target_names::kAppHistoryEntry;
}

void AppHistoryEntry::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(item_);
}

}  // namespace blink
