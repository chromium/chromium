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

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_restoration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/history_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

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
  v8::Local<v8::Object> v8_history =
      ToV8Traits<History>::ToV8(script_state, this)
          .As<v8::Object>();
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

void History::setScrollRestoration(const V8ScrollRestoration& value,
                                   ExceptionState& exception_state) {
  HistoryItem* item = GetHistoryItem();
  if (!item) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  mojom::blink::ScrollRestorationType scroll_restoration =
      value.AsEnum() == V8ScrollRestoration::Enum::kManual
          ? mojom::blink::ScrollRestorationType::kManual
          : mojom::blink::ScrollRestorationType::kAuto;
  if (scroll_restoration == ScrollRestorationInternal())
    return;

  item->SetScrollRestorationType(scroll_restoration);
  DomWindow()->GetFrame()->Client()->DidUpdateCurrentHistoryItem();
}

V8ScrollRestoration History::scrollRestoration(
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return V8ScrollRestoration(V8ScrollRestoration::Enum::kAuto);
  }
  return V8ScrollRestoration(
      ScrollRestorationInternal() ==
              mojom::blink::ScrollRestorationType::kManual
          ? V8ScrollRestoration::Enum::kManual
          : V8ScrollRestoration::Enum::kAuto);
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
  LocalDOMWindow* window = DomWindow();
  if (!window) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }
  LocalFrame* frame = window->GetFrame();
  DCHECK(frame);

  if (!frame->IsNavigationAllowed())
    return;

  DCHECK(IsMainThread());

  if (!frame->navigation_rate_limiter().CanProceed())
    return;

  // TODO(crbug.com/1262022): Remove this condition when Fenced Frames
  // transition to MPArch completely.
  if (frame->IsInFencedFrameTree())
    return;

  if (delta) {
    // Set up propagating the current task state to the navigation commit.
    std::optional<scheduler::TaskAttributionId> soft_navigation_task_id;
    if (script_state->World().IsMainWorld() && frame->IsOutermostMainFrame()) {
      if (auto* heuristics = SoftNavigationHeuristics::From(*window)) {
        soft_navigation_task_id =
            heuristics->AsyncSameDocumentNavigationStarted();
      }
    }
    DCHECK(frame->Client());
    if (frame->Client()->NavigateBackForward(delta, soft_navigation_task_id)) {
      if (Page* page = frame->GetPage())
        page->HistoryNavigationVirtualTimePauser().PauseVirtualTime();
    }
  } else {
    // We intentionally call reload() for the current frame if delta is zero.
    // Otherwise, navigation happens on the root frame.
    // This behavior is designed in the following spec.
    // https://html.spec.whatwg.org/C/#dom-history-go
    frame->Reload(WebFrameLoadType::kReload);
  }
}

void History::pushState(ScriptState* script_state,
                        const ScriptValue& data,
                        const String& title,
                        const String& url,
                        ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  WebFrameLoadType load_type = WebFrameLoadType::kStandard;
  if (LocalDOMWindow* window = DomWindow()) {
    DCHECK(window->GetFrame());
    if (window->GetFrame()->ShouldMaintainTrivialSessionHistory()) {
      window->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Use of history.pushState in a trivial session history context, "
              "which maintains only one session history entry, is treated as "
              "history.replaceState."),
          /* discard_duplicates */ true);
      load_type = WebFrameLoadType::kReplaceCurrentItem;
    }
  }

  scoped_refptr<SerializedScriptValue> serialized_data =
      SerializedScriptValue::Serialize(isolate, data.V8Value(),
                                       SerializedScriptValue::SerializeOptions(
                                           SerializedScriptValue::kForStorage),
                                       exception_state);
  if (exception_state.HadException())
    return;

  StateObjectAdded(std::move(serialized_data), title, url, load_type,
                   script_state, exception_state);
}

void History::replaceState(ScriptState* script_state,
                           const ScriptValue& data,
                           const String& title,
                           const String& url,
                           ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  scoped_refptr<SerializedScriptValue> serialized_data =
      SerializedScriptValue::Serialize(isolate, data.V8Value(),
                                       SerializedScriptValue::SerializeOptions(
                                           SerializedScriptValue::kForStorage),
                                       exception_state);
  if (exception_state.HadException())
    return;

  StateObjectAdded(std::move(serialized_data), title, url,
                   WebFrameLoadType::kReplaceCurrentItem, script_state,
                   exception_state);
}

KURL History::UrlForState(const String& url_string) {
  if (url_string.IsNull())
    return DomWindow()->Url();
  if (url_string.empty())
    return DomWindow()->BaseURL();

  return KURL(DomWindow()->BaseURL(), url_string);
}

void History::StateObjectAdded(scoped_refptr<SerializedScriptValue> data,
                               const String& /* title */,
                               const String& url_string,
                               WebFrameLoadType type,
                               ScriptState* script_state,
                               ExceptionState& exception_state) {
  LocalDOMWindow* window = DomWindow();
  if (!window) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  KURL full_url = UrlForState(url_string);
  bool can_change = CanChangeToUrlForHistoryApi(
      full_url, window->GetSecurityOrigin(), window->Url());

  if (window->GetSecurityOrigin()->IsGrantedUniversalAccess()) {
    // Log the case when 'pushState'/'replaceState' is allowed only because
    // of IsGrantedUniversalAccess ie there is no other condition which should
    // allow the change (!can_change).
    base::UmaHistogramBoolean(
        "Android.WebView.UniversalAccess.OriginUrlMismatchInHistoryUtil",
        !can_change);
    can_change = true;
  }

  if (!can_change) {
    // We can safely expose the URL to JavaScript, as a) no redirection takes
    // place: JavaScript already had this URL, b) JavaScript can only access a
    // same-origin History object.
    exception_state.ThrowSecurityError(
        "A history state object with URL '" + full_url.ElidedString() +
        "' cannot be created in a document with origin '" +
        window->GetSecurityOrigin()->ToString() + "' and URL '" +
        window->Url().ElidedString() + "'.");
    return;
  }

  if (!window->GetFrame()->navigation_rate_limiter().CanProceed()) {
    // TODO(769592): Get an API spec change so that we can throw an exception:
    //
    //  exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
    //                                    "Throttling history state changes to "
    //                                    "prevent the browser from hanging.");
    //
    // instead of merely warning.
    return;
  }

  auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
      full_url, NavigateEventType::kHistoryApi, type);
  params->state_object = data.get();
  if (window->navigation()->DispatchNavigateEvent(params) !=
      NavigationApi::DispatchResult::kContinue) {
    return;
  }

  window->document()->Loader()->RunURLAndHistoryUpdateSteps(
      full_url, nullptr, mojom::blink::SameDocumentNavigationType::kHistoryApi,
      std::move(data), type, FirePopstate::kNo);
}

}  // namespace blink
