// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include <utility>

#include "net/base/mime_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/ui_base_features.h"

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

ScriptPromise<IDLSequence<ClipboardItem>> Clipboard::read(
    ScriptState* script_state,
    ClipboardReadOptions* options,
    ExceptionState& exception_state) {
  return ClipboardPromise::CreateForRead(GetExecutionContext(), script_state,
                                         options, exception_state);
}

ScriptPromise<IDLString> Clipboard::readText(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return ClipboardPromise::CreateForReadText(GetExecutionContext(),
                                             script_state, exception_state);
}

void Clipboard::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);

  if (event_type != event_type_names::kClipboardchange) {
    return;
  }

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kClipboardChangeEventAddListener);

  Navigator& navigator = *GetSupplementable();
  LocalDOMWindow* window = navigator.DomWindow();
  if (!window) {
    return;
  }

  if (!clipboard_change_event_controller_) {
    clipboard_change_event_controller_ =
        MakeGarbageCollected<ClipboardChangeEventController>(navigator, this);
  }

  // Defer ClipboardHost bind until prerender activation; the interface is
  // kUnexpected for kSameOriginPrerendering and would kill the renderer
  // (crbug.com/500385607).
  Document* document = window->document();
  if (document && document->IsPrerendering()) {
    if (!register_with_dispatcher_pending_) {
      register_with_dispatcher_pending_ = true;
      document->AddPostPrerenderingActivationStep(
          BindOnce(&Clipboard::OnPrerenderActivatedRegisterController,
                   WrapWeakPersistent(this)));
    }
    return;
  }

  clipboard_change_event_controller_->RegisterWithDispatcher();
}

void Clipboard::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);

  if (event_type != event_type_names::kClipboardchange) {
    return;
  }

  // Don't unregister a controller that was never registered (pending defer).
  if (clipboard_change_event_controller_ &&
      !HasEventListeners(event_type_names::kClipboardchange) &&
      !register_with_dispatcher_pending_) {
    clipboard_change_event_controller_->UnregisterWithDispatcher();
  }
}

void Clipboard::OnPrerenderActivatedRegisterController() {
  register_with_dispatcher_pending_ = false;
  if (!clipboard_change_event_controller_) {
    return;
  }
  if (!HasEventListeners(event_type_names::kClipboardchange)) {
    return;
  }
  clipboard_change_event_controller_->RegisterWithDispatcher();
}

ScriptPromise<IDLUndefined> Clipboard::write(
    ScriptState* script_state,
    const HeapVector<Member<ClipboardItem>>& data,
    ExceptionState& exception_state) {
  return ClipboardPromise::CreateForWrite(GetExecutionContext(), script_state,
                                          std::move(data), exception_state);
}

ScriptPromise<IDLUndefined> Clipboard::writeText(
    ScriptState* script_state,
    const String& data,
    ExceptionState& exception_state) {
  return ClipboardPromise::CreateForWriteText(
      GetExecutionContext(), script_state, data, exception_state);
}

const AtomicString& Clipboard::InterfaceName() const {
  return event_target_names::kClipboard;
}

ExecutionContext* Clipboard::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

// static
String Clipboard::ParseWebCustomFormat(const String& format) {
  if (format.starts_with(ui::kWebClipboardFormatPrefix)) {
    String web_custom_format_suffix = format.substr(
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
  visitor->Trace(clipboard_change_event_controller_);
}

}  // namespace blink
