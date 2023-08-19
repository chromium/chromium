// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

namespace blink {

NavigationHistoryEntry::NavigationHistoryEntry(
    LocalDOMWindow* window,
    const String& key,
    const String& id,
    const KURL& url,
    int64_t document_sequence_number,
    scoped_refptr<SerializedScriptValue> state)
    : ExecutionContextClient(window),
      key_(key),
      id_(id),
      url_(url),
      document_sequence_number_(document_sequence_number),
      state_(state) {}

NavigationHistoryEntry* NavigationHistoryEntry::Clone(LocalDOMWindow* window) {
  return MakeGarbageCollected<NavigationHistoryEntry>(
      window, key_, id_, url_, document_sequence_number_, state_.get());
}

String NavigationHistoryEntry::key() const {
  return DomWindow() ? key_ : String();
}

String NavigationHistoryEntry::id() const {
  return DomWindow() ? id_ : String();
}

int64_t NavigationHistoryEntry::index() {
  return DomWindow() ? DomWindow()->navigation()->GetIndexFor(this) : -1;
}

KURL NavigationHistoryEntry::url() {
  return DomWindow() && !url_.IsEmpty() ? url_ : NullURL();
}

bool NavigationHistoryEntry::sameDocument() const {
  if (!DomWindow())
    return false;
  HistoryItem* current_item =
      DomWindow()->document()->Loader()->GetHistoryItem();
  return current_item->DocumentSequenceNumber() == document_sequence_number_;
}

ScriptValue NavigationHistoryEntry::getState() const {
  if (!DomWindow() || !state_)
    return ScriptValue();
  v8::Isolate* isolate = DomWindow()->GetIsolate();
  return ScriptValue(isolate, state_->Deserialize(isolate));
}

void NavigationHistoryEntry::SetAndSaveState(
    scoped_refptr<SerializedScriptValue> state) {
  CHECK_EQ(this, DomWindow()->navigation()->currentEntry());
  state_ = state;
  DomWindow()->document()->Loader()->GetHistoryItem()->SetNavigationApiState(
      state_.get());
  // Force the new state object to be synced to the browser process immediately.
  // The state object needs to be available as soon as possible in case a
  // new navigation commits soon, so that browser has the best chance of having
  // the up-to-date state object when constructing the arrays of non-current
  // NavigationHistoryEntries.
  DomWindow()->GetFrame()->Client()->NotifyCurrentHistoryItemChanged();
}

const AtomicString& NavigationHistoryEntry::InterfaceName() const {
  return event_target_names::kNavigationHistoryEntry;
}

void NavigationHistoryEntry::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
