/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/location.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/fragment_directive.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/url/dom_url_utils_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

Location::Location(DOMWindow* dom_window)
    : dom_window_(dom_window),
      fragment_directive_(MakeGarbageCollected<FragmentDirective>()) {}

void Location::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_window_);
  visitor->Trace(fragment_directive_);
  ScriptWrappable::Trace(visitor);
}

inline const KURL& Location::Url() const {
  const KURL& url = GetDocument()->Url();
  if (!url.IsValid()) {
    // Use "about:blank" while the page is still loading (before we have a
    // frame).
    return BlankURL();
  }

  return url;
}

String Location::href() const {
  return Url().StrippedForUseAsHref();
}

String Location::protocol() const {
  return DOMURLUtilsReadOnly::protocol(Url());
}

String Location::host() const {
  return DOMURLUtilsReadOnly::host(Url());
}

String Location::hostname() const {
  return DOMURLUtilsReadOnly::hostname(Url());
}

String Location::port() const {
  return DOMURLUtilsReadOnly::port(Url());
}

String Location::pathname() const {
  return DOMURLUtilsReadOnly::pathname(Url());
}

String Location::search() const {
  return DOMURLUtilsReadOnly::search(Url());
}

String Location::origin() const {
  return DOMURLUtilsReadOnly::origin(Url());
}

FragmentDirective* Location::fragmentDirective() const {
  return fragment_directive_;
}

DOMStringList* Location::ancestorOrigins() const {
  auto* origins = MakeGarbageCollected<DOMStringList>();
  if (!IsAttached())
    return origins;
  for (Frame* frame = dom_window_->GetFrame()->Tree().Parent(); frame;
       frame = frame->Tree().Parent()) {
    origins->Append(
        frame->GetSecurityContext()->GetSecurityOrigin()->ToString());
  }
  return origins;
}

String Location::toString() const {
  return href();
}

String Location::hash() const {
  return DOMURLUtilsReadOnly::hash(Url());
}

void Location::setHref(v8::Isolate* isolate,
                       const String& url_string,
                       ExceptionState& exception_state) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);
  SetLocation(url_string, incumbent_window, entered_window, &exception_state);
}

void Location::setProtocol(v8::Isolate* isolate,
                           const String& protocol,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  if (!url.SetProtocol(protocol)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + protocol + "' is an invalid protocol.");
    return;
  }

  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setHost(v8::Isolate* isolate,
                       const String& host,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetHostAndPort(host);
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setHostname(v8::Isolate* isolate,
                           const String& hostname,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetHost(hostname);
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setPort(v8::Isolate* isolate,
                       const String& port,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetPort(port);
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setPathname(v8::Isolate* isolate,
                           const String& pathname,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetPath(pathname);
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setSearch(v8::Isolate* isolate,
                         const String& search,
                         ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetQuery(search);
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::setHash(v8::Isolate* isolate,
                       const String& hash,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  String old_fragment_identifier = url.FragmentIdentifier();
  String new_fragment_identifier = hash;
  if (hash[0] == '#')
    new_fragment_identifier = hash.Substring(1);
  url.SetFragmentIdentifier(new_fragment_identifier);
  // Note that by parsing the URL and *then* comparing fragments, we are
  // comparing fragments post-canonicalization, and so this handles the
  // cases where fragment identifiers are ignored or invalid.
  if (EqualIgnoringNullity(old_fragment_identifier, url.FragmentIdentifier()))
    return;
  SetLocation(url.GetString(), IncumbentDOMWindow(isolate),
              EnteredDOMWindow(isolate), &exception_state);
}

void Location::assign(v8::Isolate* isolate,
                      const String& url_string,
                      ExceptionState& exception_state) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);
  SetLocation(url_string, incumbent_window, entered_window, &exception_state);
}

void Location::replace(v8::Isolate* isolate,
                       const String& url_string,
                       ExceptionState& exception_state) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);
  SetLocation(url_string, incumbent_window, entered_window, &exception_state,
              SetLocationPolicy::kReplaceThisFrame);
}

void Location::reload() {
  if (!IsAttached())
    return;
  if (GetDocument()->Url().ProtocolIsJavaScript())
    return;
  // reload() is not cross-origin accessible, so |dom_window_| will always be
  // local.
  To<LocalDOMWindow>(dom_window_.Get())
      ->GetFrame()
      ->Reload(WebFrameLoadType::kReload);
}

void Location::SetLocation(const String& url,
                           LocalDOMWindow* current_window,
                           LocalDOMWindow* entered_window,
                           ExceptionState* exception_state,
                           SetLocationPolicy set_location_policy) {
  if (!IsAttached())
    return;

  if (!current_window->GetFrame())
    return;

  Document* entered_document = entered_window->document();
  if (!entered_document)
    return;

  KURL completed_url = entered_document->CompleteURL(url);
  if (completed_url.IsNull())
    return;

  if (!current_window->GetFrame()->CanNavigate(*dom_window_->GetFrame(),
                                               completed_url)) {
    if (exception_state) {
      exception_state->ThrowSecurityError(
          "The current window does not have permission to navigate the target "
          "frame to '" +
          url + "'.");
    }
    return;
  }
  if (exception_state && !completed_url.IsValid()) {
    exception_state->ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                       "'" + url + "' is not a valid URL.");
    return;
  }

  // Check the source browsing context's CSP to fulfill the CSP check
  // requirement of https://html.spec.whatwg.org/C/#navigate for javascript
  // URLs. Although the spec states we should perform this check on task
  // execution, there are concerns about the correctness of that statement,
  // see http://github.com/whatwg/html/issues/2591.
  Document* current_document = current_window->document();
  if (current_document && completed_url.ProtocolIsJavaScript()) {
    String script_source = DecodeURLEscapeSequences(
        completed_url.GetString(), DecodeURLMode::kUTF8OrIsomorphic);
    if (!current_document->GetContentSecurityPolicyForWorld()->AllowInline(
            ContentSecurityPolicy::InlineType::kNavigation,
            nullptr /* element */, script_source, String() /* nonce */,
            current_document->Url(), OrdinalNumber())) {
      return;
    }
  }

  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
  if (activity_logger) {
    Vector<String> argv;
    argv.push_back("LocalDOMWindow");
    argv.push_back("url");
    argv.push_back(entered_document->Url());
    argv.push_back(completed_url);
    activity_logger->LogEvent("blinkSetAttribute", argv.size(), argv.data());
  }

  FrameLoadRequest request(current_window->document(),
                           ResourceRequest(completed_url));
  request.SetClientRedirectReason(ClientNavigationReason::kFrameNavigation);
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
  if (set_location_policy == SetLocationPolicy::kReplaceThisFrame)
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

  current_window->GetFrame()->MaybeLogAdClickNavigation();
  dom_window_->GetFrame()->Navigate(request, frame_load_type);
}

Document* Location::GetDocument() const {
  return To<LocalDOMWindow>(dom_window_.Get())->document();
}

bool Location::IsAttached() const {
  return dom_window_->GetFrame();
}

}  // namespace blink
