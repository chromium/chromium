// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/dom_window.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/frame/window_post_message_options.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DOMWindow::DOMWindow(Frame& frame)
    : frame_(frame),
      window_proxy_manager_(frame.GetWindowProxyManager()),
      window_is_closing_(false) {}

DOMWindow::~DOMWindow() {
  // The frame must be disconnected before finalization.
  DCHECK(!frame_);
}

v8::Local<v8::Object> DOMWindow::Wrap(v8::Isolate* isolate,
                                      v8::Local<v8::Object> creation_context) {
  NOTREACHED();
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> DOMWindow::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  NOTREACHED();
  return v8::Local<v8::Object>();
}

const AtomicString& DOMWindow::InterfaceName() const {
  return EventTargetNames::DOMWindow;
}

const DOMWindow* DOMWindow::ToDOMWindow() const {
  return this;
}

bool DOMWindow::IsWindowOrWorkerGlobalScope() const {
  return true;
}

Location* DOMWindow::location() const {
  if (!location_)
    location_ = Location::Create(const_cast<DOMWindow*>(this));
  return location_.Get();
}

bool DOMWindow::closed() const {
  return window_is_closing_ || !GetFrame() || !GetFrame()->GetPage();
}

unsigned DOMWindow::length() const {
  return GetFrame() ? GetFrame()->Tree().ScopedChildCount() : 0;
}

DOMWindow* DOMWindow::self() const {
  if (!GetFrame())
    return nullptr;

  return GetFrame()->DomWindow();
}

DOMWindow* DOMWindow::opener() const {
  // FIXME: Use FrameTree to get opener as well, to simplify logic here.
  if (!GetFrame() || !GetFrame()->Client())
    return nullptr;

  Frame* opener = GetFrame()->Client()->Opener();
  return opener ? opener->DomWindow() : nullptr;
}

DOMWindow* DOMWindow::parent() const {
  if (!GetFrame())
    return nullptr;

  Frame* parent = GetFrame()->Tree().Parent();
  return parent ? parent->DomWindow() : GetFrame()->DomWindow();
}

DOMWindow* DOMWindow::top() const {
  if (!GetFrame())
    return nullptr;

  return GetFrame()->Tree().Top().DomWindow();
}

void DOMWindow::postMessage(LocalDOMWindow* incumbent_window,
                            const ScriptValue& message,
                            const String& target_origin,
                            Vector<ScriptValue>& transfer,
                            ExceptionState& exception_state) {
  WindowPostMessageOptions options;
  options.setTargetOrigin(target_origin);
  if (!transfer.IsEmpty())
    options.setTransfer(transfer);
  postMessage(incumbent_window, message, options, exception_state);
}

void DOMWindow::postMessage(LocalDOMWindow* incumbent_window,
                            const ScriptValue& message,
                            const WindowPostMessageOptions& options,
                            ExceptionState& exception_state) {
  UseCounter::Count(incumbent_window->GetFrame(),
                    WebFeature::kWindowPostMessage);

  // Since remote windows do not have a v8::Context, we cannot use
  // [CallWith=ScriptState], and there is no good way to get the v8::Isolate.
  // As a compromise, ask the isolate to the WindowProxyManager.
  v8::Isolate* isolate = window_proxy_manager_->GetIsolate();

  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(isolate, message, options,
                                                transferables, exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);
  DoPostMessage(std::move(serialized_message), transferables.message_ports,
                options, incumbent_window, exception_state);
}

DOMWindow* DOMWindow::AnonymousIndexedGetter(uint32_t index) const {
  if (!GetFrame())
    return nullptr;

  Frame* child = GetFrame()->Tree().ScopedChild(index);
  return child ? child->DomWindow() : nullptr;
}

bool DOMWindow::IsCurrentlyDisplayedInFrame() const {
  if (GetFrame())
    SECURITY_CHECK(GetFrame()->DomWindow() == this);
  return GetFrame() && GetFrame()->GetPage();
}

bool DOMWindow::IsInsecureScriptAccess(LocalDOMWindow& calling_window,
                                       const KURL& url) {
  if (!url.ProtocolIsJavaScript())
    return false;

  // If this DOMWindow isn't currently active in the Frame, then there's no
  // way we should allow the access.
  if (IsCurrentlyDisplayedInFrame()) {
    // FIXME: Is there some way to eliminate the need for a separate
    // "callingWindow == this" check?
    if (&calling_window == this)
      return false;

    // FIXME: The name canAccess seems to be a roundabout way to ask "can
    // execute script".  Can we name the SecurityOrigin function better to make
    // this more clear?
    if (calling_window.document()->GetSecurityOrigin()->CanAccess(
            GetFrame()->GetSecurityContext()->GetSecurityOrigin())) {
      return false;
    }
  }

  calling_window.PrintErrorMessage(
      CrossDomainAccessErrorMessage(&calling_window));
  return true;
}

// FIXME: Once we're throwing exceptions for cross-origin access violations, we
// will always sanitize the target frame details, so we can safely combine
// 'crossDomainAccessErrorMessage' with this method after considering exactly
// which details may be exposed to JavaScript.
//
// http://crbug.com/17325
String DOMWindow::SanitizedCrossDomainAccessErrorMessage(
    const LocalDOMWindow* calling_window) const {
  if (!calling_window || !calling_window->document() || !GetFrame())
    return String();

  const KURL& calling_window_url = calling_window->document()->Url();
  if (calling_window_url.IsNull())
    return String();

  const SecurityOrigin* active_origin =
      calling_window->document()->GetSecurityOrigin();
  String message = "Blocked a frame with origin \"" +
                   active_origin->ToString() +
                   "\" from accessing a cross-origin frame.";

  // FIXME: Evaluate which details from 'crossDomainAccessErrorMessage' may
  // safely be reported to JavaScript.

  return message;
}

String DOMWindow::CrossDomainAccessErrorMessage(
    const LocalDOMWindow* calling_window) const {
  if (!calling_window || !calling_window->document() || !GetFrame())
    return String();

  const KURL& calling_window_url = calling_window->document()->Url();
  if (calling_window_url.IsNull())
    return String();

  // FIXME: This message, and other console messages, have extra newlines.
  // Should remove them.
  const SecurityOrigin* active_origin =
      calling_window->document()->GetSecurityOrigin();
  const SecurityOrigin* target_origin =
      GetFrame()->GetSecurityContext()->GetSecurityOrigin();
  // It's possible for a remote frame to be same origin with respect to a
  // local frame, but it must still be treated as a disallowed cross-domain
  // access. See https://crbug.com/601629.
  DCHECK(GetFrame()->IsRemoteFrame() ||
         !active_origin->CanAccess(target_origin));

  String message = "Blocked a frame with origin \"" +
                   active_origin->ToString() +
                   "\" from accessing a frame with origin \"" +
                   target_origin->ToString() + "\". ";

  // Sandbox errors: Use the origin of the frames' location, rather than their
  // actual origin (since we know that at least one will be "null").
  KURL active_url = calling_window->document()->Url();
  // TODO(alexmos): RemoteFrames do not have a document, and their URLs
  // aren't replicated.  For now, construct the URL using the replicated
  // origin for RemoteFrames. If the target frame is remote and sandboxed,
  // there isn't anything else to show other than "null" for its origin.
  KURL target_url = IsLocalDOMWindow()
                        ? blink::ToLocalDOMWindow(this)->document()->Url()
                        : KURL(NullURL(), target_origin->ToString());
  if (GetFrame()->GetSecurityContext()->IsSandboxed(kSandboxOrigin) ||
      calling_window->document()->IsSandboxed(kSandboxOrigin)) {
    message = "Blocked a frame at \"" +
              SecurityOrigin::Create(active_url)->ToString() +
              "\" from accessing a frame at \"" +
              SecurityOrigin::Create(target_url)->ToString() + "\". ";
    if (GetFrame()->GetSecurityContext()->IsSandboxed(kSandboxOrigin) &&
        calling_window->document()->IsSandboxed(kSandboxOrigin))
      return "Sandbox access violation: " + message +
             " Both frames are sandboxed and lack the \"allow-same-origin\" "
             "flag.";
    if (GetFrame()->GetSecurityContext()->IsSandboxed(kSandboxOrigin))
      return "Sandbox access violation: " + message +
             " The frame being accessed is sandboxed and lacks the "
             "\"allow-same-origin\" flag.";
    return "Sandbox access violation: " + message +
           " The frame requesting access is sandboxed and lacks the "
           "\"allow-same-origin\" flag.";
  }

  // Protocol errors: Use the URL's protocol rather than the origin's protocol
  // so that we get a useful message for non-heirarchal URLs like 'data:'.
  if (target_origin->Protocol() != active_origin->Protocol())
    return message + " The frame requesting access has a protocol of \"" +
           active_url.Protocol() +
           "\", the frame being accessed has a protocol of \"" +
           target_url.Protocol() + "\". Protocols must match.\n";

  // 'document.domain' errors.
  if (target_origin->DomainWasSetInDOM() && active_origin->DomainWasSetInDOM())
    return message +
           "The frame requesting access set \"document.domain\" to \"" +
           active_origin->Domain() +
           "\", the frame being accessed set it to \"" +
           target_origin->Domain() +
           "\". Both must set \"document.domain\" to the same value to allow "
           "access.";
  if (active_origin->DomainWasSetInDOM())
    return message +
           "The frame requesting access set \"document.domain\" to \"" +
           active_origin->Domain() +
           "\", but the frame being accessed did not. Both must set "
           "\"document.domain\" to the same value to allow access.";
  if (target_origin->DomainWasSetInDOM())
    return message + "The frame being accessed set \"document.domain\" to \"" +
           target_origin->Domain() +
           "\", but the frame requesting access did not. Both must set "
           "\"document.domain\" to the same value to allow access.";

  // Default.
  return message + "Protocols, domains, and ports must match.";
}

void DOMWindow::close(LocalDOMWindow* incumbent_window) {
  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  Document* active_document = nullptr;
  if (incumbent_window) {
    DCHECK(IsMainThread());
    active_document = incumbent_window->document();
    if (!active_document)
      return;

    if (!active_document->GetFrame() ||
        !active_document->GetFrame()->CanNavigate(*GetFrame()))
      return;
  }

  Settings* settings = GetFrame()->GetSettings();
  bool allow_scripts_to_close_windows =
      settings && settings->GetAllowScriptsToCloseWindows();

  if (!page->OpenedByDOM() && GetFrame()->Client()->BackForwardLength() > 1 &&
      !allow_scripts_to_close_windows) {
    if (active_document) {
      active_document->domWindow()->GetFrameConsole()->AddMessage(
          ConsoleMessage::Create(
              kJSMessageSource, kWarningMessageLevel,
              "Scripts may close only the windows that were opened by it."));
    }
    return;
  }

  if (!GetFrame()->ShouldClose())
    return;

  ExecutionContext* execution_context = nullptr;
  if (IsLocalDOMWindow()) {
    execution_context = blink::ToLocalDOMWindow(this)->GetExecutionContext();
  }
  probe::breakableLocation(execution_context, "DOMWindow.close");

  page->CloseSoon();

  // So as to make window.closed return the expected result
  // after window.close(), separately record the to-be-closed
  // state of this window. Scripts may access window.closed
  // before the deferred close operation has gone ahead.
  window_is_closing_ = true;
}

void DOMWindow::focus(LocalDOMWindow* incumbent_window) {
  if (!GetFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  DCHECK(incumbent_window);
  ExecutionContext* incumbent_execution_context =
      incumbent_window->GetExecutionContext();

  bool allow_focus = incumbent_execution_context->IsWindowInteractionAllowed();
  if (allow_focus) {
    incumbent_execution_context->ConsumeWindowInteraction();
  } else {
    DCHECK(IsMainThread());
    allow_focus =
        opener() && (opener() != this) &&
        (To<Document>(incumbent_execution_context)->domWindow() == opener());
  }

  // If we're a top level window, bring the window to the front.
  if (GetFrame()->IsMainFrame() && allow_focus)
    page->GetChromeClient().Focus(incumbent_window->GetFrame());

  page->GetFocusController().FocusDocumentView(GetFrame(),
                                               true /* notifyEmbedder */);
}

InputDeviceCapabilitiesConstants* DOMWindow::GetInputDeviceCapabilities() {
  if (!input_capabilities_)
    input_capabilities_ = new InputDeviceCapabilitiesConstants;
  return input_capabilities_;
}

void DOMWindow::PostMessageForTesting(
    scoped_refptr<SerializedScriptValue> message,
    const MessagePortArray& ports,
    const String& target_origin,
    LocalDOMWindow* source,
    ExceptionState& exception_state) {
  WindowPostMessageOptions options;
  options.setTargetOrigin(target_origin);
  DoPostMessage(std::move(message), ports, options, source, exception_state);
}

void DOMWindow::DoPostMessage(scoped_refptr<SerializedScriptValue> message,
                              const MessagePortArray& ports,
                              const WindowPostMessageOptions& options,
                              LocalDOMWindow* source,
                              ExceptionState& exception_state) {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  Document* source_document = source->document();

  const String& target_origin = options.targetOrigin();

  // Compute the target origin.  We need to do this synchronously in order
  // to generate the SyntaxError exception correctly.
  scoped_refptr<const SecurityOrigin> target;
  if (target_origin == "/") {
    if (!source_document)
      return;
    target = source_document->GetSecurityOrigin();
  } else if (target_origin != "*") {
    target = SecurityOrigin::CreateFromString(target_origin);
    // It doesn't make sense target a postMessage at a unique origin
    // because there's no way to represent a unique origin in a string.
    if (target->IsOpaque()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "Invalid target origin '" +
                                            target_origin +
                                            "' in a call to 'postMessage'.");
      return;
    }
  }

  auto channels = MessagePort::DisentanglePorts(GetExecutionContext(), ports,
                                                exception_state);
  if (exception_state.HadException())
    return;

  // Capture the source of the message.  We need to do this synchronously
  // in order to capture the source of the message correctly.
  if (!source_document)
    return;

  const SecurityOrigin* security_origin = source_document->GetSecurityOrigin();

  String source_origin = security_origin->ToString();

  KURL target_url = IsLocalDOMWindow()
                        ? blink::ToLocalDOMWindow(this)->document()->Url()
                        : KURL(NullURL(), GetFrame()
                                              ->GetSecurityContext()
                                              ->GetSecurityOrigin()
                                              ->ToString());
  if (MixedContentChecker::IsMixedContent(source_document->GetSecurityOrigin(),
                                          target_url)) {
    UseCounter::Count(source->GetFrame(),
                      WebFeature::kPostMessageFromSecureToInsecure);
  } else if (MixedContentChecker::IsMixedContent(
                 GetFrame()->GetSecurityContext()->GetSecurityOrigin(),
                 source_document->Url())) {
    UseCounter::Count(source->GetFrame(),
                      WebFeature::kPostMessageFromInsecureToSecure);
    if (MixedContentChecker::IsMixedContent(
            GetFrame()->Tree().Top().GetSecurityContext()->GetSecurityOrigin(),
            source_document->Url())) {
      UseCounter::Count(source->GetFrame(),
                        WebFeature::kPostMessageFromInsecureToSecureToplevel);
    }
  }

  if (!source_document->GetContentSecurityPolicy()->AllowConnectToSource(
          target_url, RedirectStatus::kNoRedirect,
          SecurityViolationReportingPolicy::kSuppressReporting)) {
    UseCounter::Count(
        source->GetFrame(),
        WebFeature::kPostMessageOutgoingWouldBeBlockedByConnectSrc);
  }
  UserActivation* user_activation = nullptr;
  if (options.includeUserActivation())
    user_activation = UserActivation::CreateSnapshot(source);

  MessageEvent* event =
      MessageEvent::Create(std::move(channels), std::move(message),
                           source_origin, String(), source, user_activation);

  SchedulePostMessage(event, std::move(target), source_document);
}

void DOMWindow::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(window_proxy_manager_);
  visitor->Trace(input_capabilities_);
  visitor->Trace(location_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
