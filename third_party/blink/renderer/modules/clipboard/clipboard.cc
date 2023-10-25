// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include <utility>

#include "net/base/mime_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

// static
const char Clipboard::kSupplementName[] = "Clipboard";

Clipboard* Clipboard::clipboard(Navigator& navigator) {
  Clipboard* clipboard = Supplement<Navigator>::From<Clipboard>(navigator);
  if (!clipboard) {
    clipboard = MakeGarbageCollected<Clipboard>(navigator);
    ProvideTo(navigator, clipboard);
  }
  return clipboard;
}

Clipboard::Clipboard(Navigator& navigator) : Supplement<Navigator>(navigator) {}

ScriptPromise Clipboard::read(ScriptState* script_state,
                              ClipboardUnsanitizedFormats* formats) {
  return ClipboardPromise::CreateForRead(GetExecutionContext(), script_state,
                                         formats);
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

// static
String Clipboard::ParseWebCustomFormat(const String& format) {
  if (format.StartsWith(ui::kWebClipboardFormatPrefix)) {
    String web_custom_format_suffix = format.Substring(
        static_cast<unsigned>(std::strlen(ui::kWebClipboardFormatPrefix)));
    std::string web_top_level_mime_type;
    std::string web_mime_sub_type;
    if (net::ParseMimeTypeWithoutParameter(web_custom_format_suffix.Utf8(),
                                           &web_top_level_mime_type,
                                           &web_mime_sub_type)) {
      return String::Format("%s/%s", web_top_level_mime_type.c_str(),
                            web_mime_sub_type.c_str());
    }
  }
  return g_empty_string;
}

void Clipboard::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
