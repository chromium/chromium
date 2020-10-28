// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include <utility>
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_item_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

namespace blink {

// static
const char Clipboard::kSupplementName[] = "Clipboard";

Clipboard* Clipboard::clipboard(Navigator& navigator) {
  if (!navigator.DomWindow())
    return nullptr;

  Clipboard* clipboard = Supplement<Navigator>::From<Clipboard>(navigator);
  if (!clipboard) {
    clipboard = MakeGarbageCollected<Clipboard>(navigator);
    ProvideTo(navigator, clipboard);
  }
  return clipboard;
}

Clipboard::Clipboard(Navigator& navigator) : Supplement<Navigator>(navigator) {}

ScriptPromise Clipboard::read(ScriptState* script_state) {
  return read(script_state, ClipboardItemOptions::Create());
}

ScriptPromise Clipboard::read(ScriptState* script_state,
                              ClipboardItemOptions* options) {
  return ClipboardPromise::CreateForRead(GetExecutionContext(), script_state,
                                         options);
}

ScriptPromise Clipboard::readText(ScriptState* script_state) {
  return ClipboardPromise::CreateForReadText(GetExecutionContext(),
                                             script_state);
}

ScriptPromise Clipboard::write(ScriptState* script_state,
                               const HeapVector<Member<ClipboardItem>>& data) {
  return ClipboardPromise::CreateForWrite(GetExecutionContext(), script_state,
                                          std::move(data));
}

ScriptPromise Clipboard::writeText(ScriptState* script_state,
                                   const String& data) {
  return ClipboardPromise::CreateForWriteText(GetExecutionContext(),
                                              script_state, data);
}

const AtomicString& Clipboard::InterfaceName() const {
  return event_target_names::kClipboard;
}

ExecutionContext* Clipboard::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

void Clipboard::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
