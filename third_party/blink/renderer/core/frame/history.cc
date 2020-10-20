/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/history.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

bool EqualIgnoringPathQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathStart()) ==
         StringView(b.GetString(), 0, b.PathStart());
}

bool EqualIgnoringQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathEnd()) ==
         StringView(b.GetString(), 0, b.PathEnd());
}

}  // namespace

History::History(LocalDOMWindow* window)
    : ExecutionContextClient(window), last_state_object_requested_(nullptr) {}

void History::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

unsigned History::length(ExceptionState& exception_state) const {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return 0;
  }
  return DomWindow()->GetFrame()->Client()->BackForwardLength();
}

ScriptValue History::state(ScriptState* script_state,
                           ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  static const V8PrivateProperty::SymbolKey kHistoryStatePrivateProperty;
  auto private_prop =
      V8PrivateProperty::GetSymbol(isolate, kHistoryStatePrivateProperty);
  v8::Local<v8::Object> v8_history = ToV8(this, script_state).As<v8::Object>();
  v8::Local<v8::Value> v8_state;

  // Returns the same V8 value unless the history gets updated.  This
  // implementation is mostly the same as the one of [CachedAttribute], but
  // it's placed in this function rather than in Blink-V8 bindings layer so
  // that PopStateEvent.state can also access the same V8 value.
  scoped_refptr<SerializedScriptValue> current_state = StateInternal();
  if (last_state_object_requested_ == current_state) {
    if (!private_prop.GetOrUndefined(v8_history).ToLocal(&v8_state))
      return ScriptValue::CreateNull(isolate);
    if (!v8_state->IsUndefined())
      return ScriptValue(isolate, v8_state);
  }

  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is "
        "not fully active");
    v8_state = v8::Null(isolate);
  } else if (!current_state) {
    v8_state = v8::Null(isolate);
  } else {
    ScriptState::EscapableScope target_context_scope(script_state);
    v8_state = target_context_scope.Escape(current_state->Deserialize(isolate));
  }

  last_state_object_requested_ = current_state;
  private_prop.Set(v8_history, v8_state);
  return ScriptValue(isolate, v8_state);
}

SerializedScriptValue* History::StateInternal() const {
  if (HistoryItem* history_item = GetHistoryItem())
    return history_item->StateObject();
  return nullptr;
}

void History::setScrollRestoration(const String& value,
                                   ExceptionState& exception_state) {
  DCHECK(value == "manual" || value == "auto");
  HistoryItem* item = GetHistoryItem();
  if (!item) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  mojom::blink::ScrollRestorationType scroll_restoration =
      value == "manual" ? mojom::blink::ScrollRestorationType::kManual
                        : mojom::blink::ScrollRestorationType::kAuto;
  if (scroll_restoration == ScrollRestorationInternal())
    return;

  item->SetScrollRestorationType(scroll_restoration);
  DomWindow()->GetFrame()->Client()->DidUpdateCurrentHistoryItem();
}

String History::scrollRestoration(ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return "auto";
  }
  return ScrollRestorationInternal() ==
                 mojom::blink::ScrollRestorationType::kManual
             ? "manual"
             : "auto";
}

mojom::blink::ScrollRestorationType History::ScrollRestorationInternal() const {
  if (HistoryItem* history_item = GetHistoryItem())
    return history_item->ScrollRestorationType();
  return mojom::blink::ScrollRestorationType::kAuto;
}

HistoryItem* History::GetHistoryItem() const {
  return DomWindow() ? DomWindow()->document()->Loader()->GetHistoryItem()
                     : nullptr;
}

bool History::IsSameAsCurrentState(SerializedScriptValue* state) const {
  return state == StateInternal();
}

void History::back(ScriptState* script_state, ExceptionState& exception_state) {
  go(script_state, -1, exception_state);
}

void History::forward(ScriptState* script_state,
                      ExceptionState& exception_state) {
  go(script_state, 1, exception_state);
}

void History::go(ScriptState* script_state,
                 int delta,
                 ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  DCHECK(IsMainThread());
  auto* active_window = LocalDOMWindow::From(script_state);
  if (!active_window)
    return;

  if (!active_window->GetFrame() ||
      !active_window->GetFrame()->CanNavigate(*GetFrame()) ||
      !active_window->GetFrame()->IsNavigationAllowed() ||
      !DomWindow()->GetFrame()->IsNavigationAllowed()) {
    return;
  }

  if (!DomWindow()->GetFrame()->navigation_rate_limiter().CanProceed())
    return;

  if (delta) {
    if (DomWindow()->GetFrame()->Client()->NavigateBackForward(delta)) {
      if (Page* page = DomWindow()->GetFrame()->GetPage())
        page->HistoryNavigationVirtualTimePauser().PauseVirtualTime();
    }
  } else {
    // We intentionally call reload() for the current frame if delta is zero.
    // Otherwise, navigation happens on the root frame.
    // This behavior is designed in the following spec.
    // https://html.spec.whatwg.org/C/#dom-history-go
    DomWindow()->GetFrame()->Reload(WebFrameLoadType::kReload);
  }
}

void History::pushState(v8::Isolate* isolate,
                        const ScriptValue& data,
                        const String& title,
                        const String& url,
                        ExceptionState& exception_state) {
  WebFrameLoadType load_type = WebFrameLoadType::kStandard;
  // Navigations in portal contexts do not create back/forward entries.
  if (DomWindow() && DomWindow()->GetFrame()->GetPage()->InsidePortal()) {
    DomWindow()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Use of history.pushState in a portal context "
            "is treated as history.replaceState."),
        /* discard_duplicates */ true);
    load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  scoped_refptr<SerializedScriptValue> serialized_data =
      SerializedScriptValue::Serialize(isolate, data.V8Value(),
                                       SerializedScriptValue::SerializeOptions(
                                           SerializedScriptValue::kForStorage),
                                       exception_state);
  if (exception_state.HadException())
    return;

  StateObjectAdded(std::move(serialized_data), title, url,
                   ScrollRestorationInternal(), load_type, exception_state);
}

void History::replaceState(v8::Isolate* isolate,
                           const ScriptValue& data,
                           const String& title,
                           const String& url,
                           ExceptionState& exception_state) {
  scoped_refptr<SerializedScriptValue> serialized_data =
      SerializedScriptValue::Serialize(isolate, data.V8Value(),
                                       SerializedScriptValue::SerializeOptions(
                                           SerializedScriptValue::kForStorage),
                                       exception_state);
  if (exception_state.HadException())
    return;

  StateObjectAdded(std::move(serialized_data), title, url,
                   ScrollRestorationInternal(),
                   WebFrameLoadType::kReplaceCurrentItem, exception_state);
}

KURL History::UrlForState(const String& url_string) {
  if (url_string.IsNull())
    return DomWindow()->Url();
  if (url_string.IsEmpty())
    return DomWindow()->BaseURL();

  return KURL(DomWindow()->BaseURL(), url_string);
}

bool History::CanChangeToUrl(const KURL& url,
                             const SecurityOrigin* document_origin,
                             const KURL& document_url) {
  if (!url.IsValid())
    return false;

  if (document_origin->IsGrantedUniversalAccess())
    return true;

  // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
  // 'pushState'/'replaceState' to modify the URL fragment: see
  // https://crbug.com/528681 for the compatibility concerns.
  if (document_origin->IsOpaque() || document_origin->IsLocal())
    return EqualIgnoringQueryAndFragment(url, document_url);

  if (!EqualIgnoringPathQueryAndFragment(url, document_url))
    return false;

  scoped_refptr<const SecurityOrigin> requested_origin =
      SecurityOrigin::Create(url);
  if (requested_origin->IsOpaque() ||
      !requested_origin->IsSameOriginWith(document_origin)) {
    return false;
  }

  return true;
}

void History::StateObjectAdded(
    scoped_refptr<SerializedScriptValue> data,
    const String& /* title */,
    const String& url_string,
    mojom::blink::ScrollRestorationType restoration_type,
    WebFrameLoadType type,
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  KURL full_url = UrlForState(url_string);
  if (!CanChangeToUrl(full_url, DomWindow()->GetSecurityOrigin(),
                      DomWindow()->Url())) {
    // We can safely expose the URL to JavaScript, as a) no redirection takes
    // place: JavaScript already had this URL, b) JavaScript can only access a
    // same-origin History object.
    exception_state.ThrowSecurityError(
        "A history state object with URL '" + full_url.ElidedString() +
        "' cannot be created in a document with origin '" +
        DomWindow()->GetSecurityOrigin()->ToString() + "' and URL '" +
        DomWindow()->Url().ElidedString() + "'.");
    return;
  }

  if (!DomWindow()->GetFrame()->navigation_rate_limiter().CanProceed()) {
    // TODO(769592): Get an API spec change so that we can throw an exception:
    //
    //  exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
    //                                    "Throttling history state changes to "
    //                                    "prevent the browser from hanging.");
    //
    // instead of merely warning.
    return;
  }

  DomWindow()->document()->Loader()->UpdateForSameDocumentNavigation(
      full_url, kSameDocumentNavigationHistoryApi, std::move(data),
      restoration_type, type, DomWindow()->document());
}

}  // namespace blink
