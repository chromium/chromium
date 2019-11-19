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
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/frame/window_post_message_options.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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
  return event_target_names::kWindow;
}

const DOMWindow* DOMWindow::ToDOMWindow() const {
  return this;
}

bool DOMWindow::IsWindowOrWorkerGlobalScope() const {
  return true;
}

Location* DOMWindow::location() const {
  if (!location_)
    location_ = MakeGarbageCollected<Location>(const_cast<DOMWindow*>(this));
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

void DOMWindow::postMessage(v8::Isolate* isolate,
                            const ScriptValue& message,
                            const String& target_origin,
                            HeapVector<ScriptValue>& transfer,
                            ExceptionState& exception_state) {
  WindowPostMessageOptions* options = WindowPostMessageOptions::Create();
  options->setTargetOrigin(target_origin);
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(isolate, message, options, exception_state);
}

void DOMWindow::postMessage(v8::Isolate* isolate,
                            const ScriptValue& message,
                            const WindowPostMessageOptions* options,
                            ExceptionState& exception_state) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  UseCounter::Count(incumbent_window->document(),
                    WebFeature::kWindowPostMessage);

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

// FIXME: Once we're throwing exceptions for cross-origin access violations, we
// will always sanitize the target frame details, so we can safely combine
// 'crossDomainAccessErrorMessage' with this method after considering exactly
// which details may be exposed to JavaScript.
//
// http://crbug.com/17325
String DOMWindow::SanitizedCrossDomainAccessErrorMessage(
    const LocalDOMWindow* accessing_window,
    CrossDocumentAccessFeaturePolicy cross_document_access) const {
  if (!accessing_window || !accessing_window->document() || !GetFrame())
    return String();

  const KURL& accessing_window_url = accessing_window->document()->Url();
  if (accessing_window_url.IsNull())
    return String();

  const SecurityOrigin* active_origin =
      accessing_window->document()->GetSecurityOrigin();
  String message;
  if (cross_document_access == CrossDocumentAccessFeaturePolicy::kDisallowed) {
    message = "Blocked a restricted frame with origin \"" +
              active_origin->ToString() + "\" from accessing another frame.";
  } else {
    message = "Blocked a frame with origin \"" + active_origin->ToString() +
              "\" from accessing a cross-origin frame.";
  }

  // FIXME: Evaluate which details from 'crossDomainAccessErrorMessage' may
  // safely be reported to JavaScript.

  return message;
}

String DOMWindow::CrossDomainAccessErrorMessage(
    const LocalDOMWindow* accessing_window,
    CrossDocumentAccessFeaturePolicy cross_document_access) const {
  if (!accessing_window || !accessing_window->document() || !GetFrame())
    return String();

  const KURL& accessing_window_url = accessing_window->document()->Url();
  if (accessing_window_url.IsNull())
    return String();

  // FIXME: This message, and other console messages, have extra newlines.
  // Should remove them.
  const SecurityOrigin* active_origin =
      accessing_window->document()->GetSecurityOrigin();
  const SecurityOrigin* target_origin =
      GetFrame()->GetSecurityContext()->GetSecurityOrigin();
  auto* local_dom_window = DynamicTo<LocalDOMWindow>(this);
  // It's possible for a remote frame to be same origin with respect to a
  // local frame, but it must still be treated as a disallowed cross-domain
  // access. See https://crbug.com/601629.
  DCHECK(GetFrame()->IsRemoteFrame() ||
         !active_origin->CanAccess(target_origin) ||
         (local_dom_window && accessing_window->document()->GetAgent() !=
                                  local_dom_window->document()->GetAgent()));

  String message = "Blocked a frame with origin \"" +
                   active_origin->ToString() +
                   "\" from accessing a frame with origin \"" +
                   target_origin->ToString() + "\". ";

  // Sandbox errors: Use the origin of the frames' location, rather than their
  // actual origin (since we know that at least one will be "null").
  KURL active_url = accessing_window->document()->Url();
  // TODO(alexmos): RemoteFrames do not have a document, and their URLs
  // aren't replicated.  For now, construct the URL using the replicated
  // origin for RemoteFrames. If the target frame is remote and sandboxed,
  // there isn't anything else to show other than "null" for its origin.
  KURL target_url = local_dom_window
                        ? local_dom_window->document()->Url()
                        : KURL(NullURL(), target_origin->ToString());
  if (GetFrame()->GetSecurityContext()->IsSandboxed(WebSandboxFlags::kOrigin) ||
      accessing_window->document()->IsSandboxed(WebSandboxFlags::kOrigin)) {
    message = "Blocked a frame at \"" +
              SecurityOrigin::Create(active_url)->ToString() +
              "\" from accessing a frame at \"" +
              SecurityOrigin::Create(target_url)->ToString() + "\". ";
    if (GetFrame()->GetSecurityContext()->IsSandboxed(
            WebSandboxFlags::kOrigin) &&
        accessing_window->document()->IsSandboxed(WebSandboxFlags::kOrigin))
      return "Sandbox access violation: " + message +
             " Both frames are sandboxed and lack the \"allow-same-origin\" "
             "flag.";
    if (GetFrame()->GetSecurityContext()->IsSandboxed(WebSandboxFlags::kOrigin))
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
  if (cross_document_access == CrossDocumentAccessFeaturePolicy::kDisallowed)
    return message + "The document-access policy denied access.";

  // Default.
  return message + "Protocols, domains, and ports must match.";
}

void DOMWindow::close(v8::Isolate* isolate) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  Close(incumbent_window);
}

void DOMWindow::Close(LocalDOMWindow* incumbent_window) {
  DCHECK(incumbent_window);

  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  Document* active_document = incumbent_window->document();
  if (!(active_document && active_document->GetFrame() &&
        active_document->GetFrame()->CanNavigate(*GetFrame()))) {
    return;
  }

  Settings* settings = GetFrame()->GetSettings();
  bool allow_scripts_to_close_windows =
      settings && settings->GetAllowScriptsToCloseWindows();

  if (!page->OpenedByDOM() && GetFrame()->Client()->BackForwardLength() > 1 &&
      !allow_scripts_to_close_windows) {
    active_document->domWindow()->GetFrameConsole()->AddMessage(
        ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Scripts may close only the windows that were opened by it."));
    return;
  }

  if (!GetFrame()->ShouldClose())
    return;

  ExecutionContext* execution_context = nullptr;
  if (auto* local_dom_window = DynamicTo<LocalDOMWindow>(this)) {
    execution_context = local_dom_window->GetExecutionContext();
  }
  probe::BreakableLocation(execution_context, "DOMWindow.close");

  page->CloseSoon();

  // So as to make window.closed return the expected result
  // after window.close(), separately record the to-be-closed
  // state of this window. Scripts may access window.closed
  // before the deferred close operation has gone ahead.
  window_is_closing_ = true;
}

void DOMWindow::focus(v8::Isolate* isolate) {
  if (!GetFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  // HTML standard doesn't require to check the incumbent realm, but Blink
  // historically checks it for some reasons, maybe the same reason as |close|.
  // (|close| checks whether the incumbent realm is eligible to close the window
  // in order to prevent a (cross origin) window from abusing |close| to close
  // pages randomly or with a malicious intent.)
  // https://html.spec.whatwg.org/C/#dom-window-focus
  // https://html.spec.whatwg.org/C/#focusing-steps
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  ExecutionContext* incumbent_execution_context =
      incumbent_window->GetExecutionContext();

  // TODO(mustaq): Use of |allow_focus| and consuming the activation here seems
  // suspicious (https://crbug.com/959815).
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
  if (GetFrame()->IsMainFrame() && allow_focus) {
    page->GetChromeClient().Focus(incumbent_window->GetFrame());
  } else if (auto* local_frame = DynamicTo<LocalFrame>(GetFrame())) {
    // We are depending on user activation twice since IsFocusAllowed() will
    // check for activation. This should be addressed in
    // https://crbug.com/959815.
    if (local_frame->GetDocument() &&
        !local_frame->GetDocument()->IsFocusAllowed()) {
      return;
    }
  }

  page->GetFocusController().FocusDocumentView(GetFrame(),
                                               true /* notifyEmbedder */);
}

InputDeviceCapabilitiesConstants* DOMWindow::GetInputDeviceCapabilities() {
  if (!input_capabilities_) {
    input_capabilities_ =
        MakeGarbageCollected<InputDeviceCapabilitiesConstants>();
  }
  return input_capabilities_;
}

void DOMWindow::PostMessageForTesting(
    scoped_refptr<SerializedScriptValue> message,
    const MessagePortArray& ports,
    const String& target_origin,
    LocalDOMWindow* source,
    ExceptionState& exception_state) {
  WindowPostMessageOptions* options = WindowPostMessageOptions::Create();
  options->setTargetOrigin(target_origin);
  DoPostMessage(std::move(message), ports, options, source, exception_state);
}

void DOMWindow::DoPostMessage(scoped_refptr<SerializedScriptValue> message,
                              const MessagePortArray& ports,
                              const WindowPostMessageOptions* options,
                              LocalDOMWindow* source,
                              ExceptionState& exception_state) {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  Document* source_document = source->document();

  // Capture the source of the message.  We need to do this synchronously
  // in order to capture the source of the message correctly.
  if (!source_document)
    return;

  // Compute the target origin.  We need to do this synchronously in order
  // to generate the SyntaxError exception correctly.
  scoped_refptr<const SecurityOrigin> target =
      PostMessageHelper::GetTargetOrigin(options, *source_document,
                                         exception_state);
  if (exception_state.HadException())
    return;

  auto channels = MessagePort::DisentanglePorts(GetExecutionContext(), ports,
                                                exception_state);
  if (exception_state.HadException())
    return;

  const SecurityOrigin* security_origin = source_document->GetSecurityOrigin();

  String source_origin = security_origin->ToString();

  auto* local_dom_window = DynamicTo<LocalDOMWindow>(this);
  KURL target_url = local_dom_window
                        ? local_dom_window->document()->Url()
                        : KURL(NullURL(), GetFrame()
                                              ->GetSecurityContext()
                                              ->GetSecurityOrigin()
                                              ->ToString());
  if (MixedContentChecker::IsMixedContent(source_document->GetSecurityOrigin(),
                                          target_url)) {
    UseCounter::Count(source_document,
                      WebFeature::kPostMessageFromSecureToInsecure);
  } else if (MixedContentChecker::IsMixedContent(
                 GetFrame()->GetSecurityContext()->GetSecurityOrigin(),
                 source_document->Url())) {
    UseCounter::Count(source_document,
                      WebFeature::kPostMessageFromInsecureToSecure);
    if (MixedContentChecker::IsMixedContent(
            GetFrame()->Tree().Top().GetSecurityContext()->GetSecurityOrigin(),
            source_document->Url())) {
      UseCounter::Count(source_document,
                        WebFeature::kPostMessageFromInsecureToSecureToplevel);
    }
  }

  if (!source_document->GetContentSecurityPolicy()->AllowConnectToSource(
          target_url, RedirectStatus::kNoRedirect,
          SecurityViolationReportingPolicy::kSuppressReporting)) {
    UseCounter::Count(
        source_document,
        WebFeature::kPostMessageOutgoingWouldBeBlockedByConnectSrc);
  }
  UserActivation* user_activation = nullptr;
  if (options->includeUserActivation())
    user_activation = UserActivation::CreateSnapshot(source);

  LocalFrame* source_frame = source->GetFrame();

  bool allow_autoplay = false;
  if (RuntimeEnabledFeatures::ExperimentalAutoplayDynamicDelegationEnabled(
          GetExecutionContext()) &&
      LocalFrame::HasTransientUserActivation(source_frame) &&
      options->hasAllow()) {
    Vector<String> policy_entry_list;
    options->allow().Split(' ', policy_entry_list);
    allow_autoplay = policy_entry_list.Contains("autoplay");
  }

  MessageEvent* event = MessageEvent::Create(
      std::move(channels), std::move(message), source_origin, String(), source,
      user_activation, options->transferUserActivation(), allow_autoplay);

  // Transfer user activation state in the source's renderer when
  // |transferUserActivation| is true. We are making an expriment with
  // dynamic delegation of "autoplay" capability using this post message
  // approach to transfer user activation.
  // TODO(lanwei): we should execute the below code after the post task fires
  // (for both local and remote posting messages).
  bool should_transfer_user_activation =
      RuntimeEnabledFeatures::UserActivationPostMessageTransferEnabled() &&
      options->transferUserActivation();
  should_transfer_user_activation =
      should_transfer_user_activation || allow_autoplay;
  if (should_transfer_user_activation &&
      LocalFrame::HasTransientUserActivation(source_frame)) {
    GetFrame()->TransferUserActivationFrom(source_frame);

    // When the source and target frames are in the same process, we need to
    // update the user activation state in the browser process. For the cross
    // process case, it is handled in RemoteDOMWindow.
    if (IsLocalDOMWindow())
      GetFrame()->Client()->TransferUserActivationFrom(source->GetFrame());
  }

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
