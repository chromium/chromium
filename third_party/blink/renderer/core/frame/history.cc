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

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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

History::History(LocalFrame* frame)
    : DOMWindowClient(frame), last_state_object_requested_(nullptr) {}

void History::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

unsigned History::length(ExceptionState& exception_state) const {
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return 0;
  }
  return GetFrame()->Client()->BackForwardLength();
}

SerializedScriptValue* History::state(ExceptionState& exception_state) {
  if (!GetFrame()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return nullptr;
  }
  last_state_object_requested_ = StateInternal();
  return last_state_object_requested_.get();
}

SerializedScriptValue* History::StateInternal() const {
  if (!GetFrame())
    return nullptr;

  if (HistoryItem* history_item =
          GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem()) {
    return history_item->StateObject();
  }

  return nullptr;
}

void History::setScrollRestoration(const String& value,
                                   ExceptionState& exception_state) {
  DCHECK(value == "manual" || value == "auto");
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  HistoryScrollRestorationType scroll_restoration =
      value == "manual" ? kScrollRestorationManual : kScrollRestorationAuto;
  if (scroll_restoration == ScrollRestorationInternal())
    return;

  if (HistoryItem* history_item =
          GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem()) {
    history_item->SetScrollRestorationType(scroll_restoration);
    GetFrame()->Client()->DidUpdateCurrentHistoryItem();
  }
}

String History::scrollRestoration(ExceptionState& exception_state) {
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return "auto";
  }
  return ScrollRestorationInternal() == kScrollRestorationManual ? "manual"
                                                                 : "auto";
}

HistoryScrollRestorationType History::ScrollRestorationInternal() const {
  constexpr HistoryScrollRestorationType default_type = kScrollRestorationAuto;

  LocalFrame* frame = GetFrame();
  if (!frame)
    return default_type;

  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
  if (!document_loader)
    return default_type;

  HistoryItem* history_item = document_loader->GetHistoryItem();
  if (!history_item)
    return default_type;

  return history_item->ScrollRestorationType();
}

// TODO(crbug.com/394296): This is not the long-term fix to IPC flooding that we
// need. However, it does somewhat mitigate the immediate concern of |pushState|
// and |replaceState| DoS (assuming the renderer has not been compromised).
bool History::ShouldThrottleStateObjectChanges() {
  if (!GetFrame()->GetSettings()->GetShouldThrottlePushState())
    return false;

  // The aim is to enable 8 'frames' (history updates) per second, but we
  // express it as 80 frames per 10 seconds because some use cases (including
  // tests) do more than 8 updates in 1 second. But over time, applications
  // shooting for 8 FPS should work. If necessary to support legitimate
  // applications, we can increase this threshold somewhat.
  const int kStateUpdateLimit = 80;

  if (state_flood_guard.count > kStateUpdateLimit) {
    static constexpr auto kStateUpdateLimitResetInterval =
        TimeDelta::FromSeconds(10);
    const auto now = CurrentTimeTicks();
    if (now - state_flood_guard.last_updated > kStateUpdateLimitResetInterval) {
      state_flood_guard.count = 0;
      state_flood_guard.last_updated = now;
      return false;
    }
    return true;
  }

  state_flood_guard.count++;
  return false;
}

bool History::stateChanged() const {
  return last_state_object_requested_ != StateInternal();
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
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  DCHECK(IsMainThread());
  Document* active_document =
      To<Document>(ExecutionContext::From(script_state));
  if (!active_document)
    return;

  if (!active_document->GetFrame() ||
      !active_document->GetFrame()->CanNavigate(*GetFrame()) ||
      !active_document->GetFrame()->IsNavigationAllowed() ||
      !NavigationDisablerForBeforeUnload::IsNavigationAllowed()) {
    return;
  }

  if (delta) {
    GetFrame()->Client()->NavigateBackForward(delta);
  } else {
    // We intentionally call reload() for the current frame if delta is zero.
    // Otherwise, navigation happens on the root frame.
    // This behavior is designed in the following spec.
    // https://html.spec.whatwg.org/multipage/browsers.html#dom-history-go
    GetFrame()->Reload(WebFrameLoadType::kReload,
                       ClientRedirectPolicy::kClientRedirect);
  }
}

void History::pushState(scoped_refptr<SerializedScriptValue> data,
                        const String& title,
                        const String& url,
                        ExceptionState& exception_state) {
  StateObjectAdded(std::move(data), title, url, ScrollRestorationInternal(),
                   WebFrameLoadType::kStandard, exception_state);
}

KURL History::UrlForState(const String& url_string) {
  Document* document = GetFrame()->GetDocument();

  if (url_string.IsNull())
    return document->Url();
  if (url_string.IsEmpty())
    return document->BaseURL();

  return KURL(document->BaseURL(), url_string);
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
      !requested_origin->IsSameSchemeHostPort(document_origin)) {
    return false;
  }

  return true;
}

void History::StateObjectAdded(scoped_refptr<SerializedScriptValue> data,
                               const String& /* title */,
                               const String& url_string,
                               HistoryScrollRestorationType restoration_type,
                               WebFrameLoadType type,
                               ExceptionState& exception_state) {
  if (!GetFrame() || !GetFrame()->GetPage() ||
      !GetFrame()->Loader().GetDocumentLoader()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  KURL full_url = UrlForState(url_string);
  if (!CanChangeToUrl(full_url, GetFrame()->GetDocument()->GetSecurityOrigin(),
                      GetFrame()->GetDocument()->Url())) {
    // We can safely expose the URL to JavaScript, as a) no redirection takes
    // place: JavaScript already had this URL, b) JavaScript can only access a
    // same-origin History object.
    exception_state.ThrowSecurityError(
        "A history state object with URL '" + full_url.ElidedString() +
        "' cannot be created in a document with origin '" +
        GetFrame()->GetDocument()->GetSecurityOrigin()->ToString() +
        "' and URL '" + GetFrame()->GetDocument()->Url().ElidedString() + "'.");
    return;
  }

  if (ShouldThrottleStateObjectChanges()) {
    // TODO(769592): Get an API spec change so that we can throw an exception:
    //
    //  exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
    //                                    "Throttling history state changes to "
    //                                    "prevent the browser from hanging.");
    //
    // instead of merely warning.

    GetFrame()->Console().AddMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               "Throttling history state changes to prevent "
                               "the browser from hanging."));
    return;
  }

  GetFrame()->Loader().UpdateForSameDocumentNavigation(
      full_url, kSameDocumentNavigationHistoryApi, std::move(data),
      restoration_type, type, GetFrame()->GetDocument());
}

}  // namespace blink
