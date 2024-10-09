// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/frame/dom_window.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window_post_message_options.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/coop_access_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

String CoopReportOnlyErrorMessage(const String& property_name) {
  String call;
  if (property_name == "named") {
    call = "window[\"name\"]";
  } else if (property_name == "indexed") {
    call = "window[i]";
  } else {
    call = "window." + property_name;
  }
  return "Cross-Origin-Opener-Policy policy would block the " + call + " call.";
}

}  // namespace

DOMWindow::DOMWindow(Frame& frame)
    : frame_(frame),
      window_proxy_manager_(frame.GetWindowProxyManager()),
      window_is_closing_(false) {}

DOMWindow::~DOMWindow() {
  // The frame must be disconnected before finalization.
  DCHECK(!frame_);
}

v8::Local<v8::Value> DOMWindow::Wrap(ScriptState* script_state) {
  // TODO(yukishiino): Get understanding of why it's possible to initialize
  // the context after the frame is detached.  And then, remove the following
  // lines.  See also https://crbug.com/712638 .
  Frame* frame = GetFrame();
  if (!frame)
    return v8::Null(script_state->GetIsolate());

  // TODO(yukishiino): We'd like to return a global proxy instead of undefined
  // regardless of whether it's detached or not, in order to conform to spec.
  //
  // Getting the proxy also results in initializing it and eventually yields in
  // `SetupWindowPrototypeChain()` calls for the window proxy.
  v8::MaybeLocal<v8::Object> proxy =
      frame->GetWindowProxy(script_state->World())->GlobalProxyIfNotDetached();
  if (proxy.IsEmpty()) {
    // Return Undefined instead of an empty to avoid crashes further along the
    // way, as `Wrap()` is expected to return a non-empty value.
    return v8::Undefined(script_state->GetIsolate());
  } else {
    return proxy.ToLocalChecked();
  }
}

v8::Local<v8::Object> DOMWindow::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  NOTREACHED();
}

v8::Local<v8::Object> DOMWindow::AssociateWithWrapper(
    v8::Isolate* isolate,
    DOMWrapperWorld* world,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  // Using the world directly avoids fetching it from a potentially
  // half-initialized context.
  if (world->DomDataStore().Set</*entered_context=*/false>(
          isolate, this, wrapper_type_info, wrapper)) {
    V8DOMWrapper::SetNativeInfo(isolate, wrapper, this);
    DCHECK(V8DOMWrapper::HasInternalFieldsSet(isolate, wrapper));
  }
  return wrapper;
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
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessLocation,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
      mojom::blink::WindowProxyAccessType::kLocation);
  if (!location_)
    location_ = MakeGarbageCollected<Location>(const_cast<DOMWindow*>(this));
  return location_.Get();
}

bool DOMWindow::closed() const {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessClosed,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClosed,
      mojom::blink::WindowProxyAccessType::kClosed);
  return window_is_closing_ || !GetFrame() || !GetFrame()->GetPage();
}

unsigned DOMWindow::length() const {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessLength,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLength,
      mojom::blink::WindowProxyAccessType::kLength);
  return GetFrame() ? GetFrame()->Tree().ScopedChildCount() : 0;
}

DOMWindow* DOMWindow::self() const {
  if (!GetFrame())
    return nullptr;

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessSelf,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageSelf,
      mojom::blink::WindowProxyAccessType::kSelf);

  return GetFrame()->DomWindow();
}

DOMWindow* DOMWindow::window() const {
  if (!GetFrame())
    return nullptr;

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessWindow,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageWindow,
      mojom::blink::WindowProxyAccessType::kWindow);

  return GetFrame()->DomWindow();
}

DOMWindow* DOMWindow::frames() const {
  if (!GetFrame())
    return nullptr;

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessFrames,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFrames,
      mojom::blink::WindowProxyAccessType::kFrames);

  return GetFrame()->DomWindow();
}

ScriptValue DOMWindow::openerForBindings(v8::Isolate* isolate) const {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessOpener,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener,
      mojom::blink::WindowProxyAccessType::kOpener);
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  return ScriptValue(isolate, ToV8Traits<IDLNullable<DOMWindow>>::ToV8(
                                  script_state, opener()));
}

DOMWindow* DOMWindow::opener() const {
  // FIXME: Use FrameTree to get opener as well, to simplify logic here.
  if (!GetFrame() || !GetFrame()->Client())
    return nullptr;

  Frame* opener = GetFrame()->Opener();
  return opener ? opener->DomWindow() : nullptr;
}

void DOMWindow::setOpenerForBindings(v8::Isolate* isolate,
                                     ScriptValue opener,
                                     ExceptionState& exception_state) {
  ReportCoopAccess("opener");
  if (!GetFrame()) {
    return;
  }

  // https://html.spec.whatwg.org/C/#dom-opener
  // 7.1.2.1. Navigating related browsing contexts in the DOM
  // The opener attribute's setter must run these steps:
  // step 1. If the given value is null and this Window object's browsing
  //     context is non-null, then set this Window object's browsing context's
  //     disowned to true.
  //
  // Opener can be shadowed if it is in the same domain.
  // Have a special handling of null value to behave
  // like Firefox. See bug http://b/1224887 & http://b/791706.
  if (opener.IsNull()) {
    To<LocalFrame>(GetFrame())->SetOpener(nullptr);
  }

  // step 2. If the given value is non-null, then return
  //     ? OrdinaryDefineOwnProperty(this Window object, "opener",
  //     { [[Value]]: the given value, [[Writable]]: true,
  //       [[Enumerable]]: true, [[Configurable]]: true }).
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> this_wrapper =
      ToV8Traits<DOMWindow>::ToV8(ScriptState::From(isolate, context), this)
          .As<v8::Object>();
  v8::PropertyDescriptor desc(opener.V8Value(), /*writable=*/true);
  desc.set_enumerable(true);
  desc.set_configurable(true);
  bool result = false;
  if (!this_wrapper
           ->DefineProperty(context, V8AtomicString(isolate, "opener"), desc)
           .To(&result)) {
    return;
  }
  if (!result) {
    exception_state.ThrowTypeError("Cannot redefine the property.");
  }
}

DOMWindow* DOMWindow::parent() const {
  if (!GetFrame())
    return nullptr;

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessParent,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageParent,
      mojom::blink::WindowProxyAccessType::kParent);

  Frame* parent = GetFrame()->Tree().Parent();
  return parent ? parent->DomWindow() : GetFrame()->DomWindow();
}

DOMWindow* DOMWindow::top() const {
  if (!GetFrame())
    return nullptr;

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessTop,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageTop,
      mojom::blink::WindowProxyAccessType::kTop);

  return GetFrame()->Tree().Top().DomWindow();
}

void DOMWindow::postMessage(v8::Isolate* isolate,
                            const ScriptValue& message,
                            const String& target_origin,
                            HeapVector<ScriptValue>& transfer,
                            ExceptionState& exception_state) {
  WindowPostMessageOptions* options = WindowPostMessageOptions::Create();
  options->setTargetOrigin(target_origin);
  if (!transfer.empty())
    options->setTransfer(transfer);
  postMessage(isolate, message, options, exception_state);
}

void DOMWindow::postMessage(v8::Isolate* isolate,
                            const ScriptValue& message,
                            const WindowPostMessageOptions* options,
                            ExceptionState& exception_state) {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessPostMessage,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPagePostMessage,
      mojom::blink::WindowProxyAccessType::kPostMessage);
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

DOMWindow* DOMWindow::AnonymousIndexedGetter(uint32_t index) {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessIndexedGetter,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter,
      mojom::blink::WindowProxyAccessType::kAnonymousIndexedGetter);
  ReportCoopAccess("indexed");

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
    CrossDocumentAccessPolicy cross_document_access) const {
  if (!accessing_window || !GetFrame())
    return String();

  const KURL& accessing_window_url = accessing_window->Url();
  if (accessing_window_url.IsNull())
    return String();

  const SecurityOrigin* active_origin = accessing_window->GetSecurityOrigin();
  String message;
  if (cross_document_access == CrossDocumentAccessPolicy::kDisallowed) {
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
    CrossDocumentAccessPolicy cross_document_access) const {
  if (!accessing_window || !GetFrame())
    return String();

  const KURL& accessing_window_url = accessing_window->Url();
  if (accessing_window_url.IsNull())
    return String();

  const SecurityOrigin* active_origin = accessing_window->GetSecurityOrigin();
  const SecurityOrigin* target_origin =
      GetFrame()->GetSecurityContext()->GetSecurityOrigin();
  auto* local_dom_window = DynamicTo<LocalDOMWindow>(this);
  // It's possible for a remote frame to be same origin with respect to a
  // local frame, but it must still be treated as a disallowed cross-domain
  // access. See https://crbug.com/601629.
  DCHECK(GetFrame()->IsRemoteFrame() ||
         !active_origin->CanAccess(target_origin) ||
         (local_dom_window &&
          accessing_window->GetAgent() != local_dom_window->GetAgent()));

  String message = "Blocked a frame with origin \"" +
                   active_origin->ToString() +
                   "\" from accessing a frame with origin \"" +
                   target_origin->ToString() + "\". ";

  // Sandbox errors: Use the origin of the frames' location, rather than their
  // actual origin (since we know that at least one will be "null").
  KURL active_url = accessing_window->Url();
  // TODO(alexmos): RemoteFrames do not have a document, and their URLs
  // aren't replicated.  For now, construct the URL using the replicated
  // origin for RemoteFrames. If the target frame is remote and sandboxed,
  // there isn't anything else to show other than "null" for its origin.
  KURL target_url = local_dom_window
                        ? local_dom_window->Url()
                        : KURL(NullURL(), target_origin->ToString());
  using SandboxFlags = network::mojom::blink::WebSandboxFlags;
  if (GetFrame()->GetSecurityContext()->IsSandboxed(SandboxFlags::kOrigin) ||
      accessing_window->IsSandboxed(SandboxFlags::kOrigin)) {
    message = "Blocked a frame at \"" +
              SecurityOrigin::Create(active_url)->ToString() +
              "\" from accessing a frame at \"" +
              SecurityOrigin::Create(target_url)->ToString() + "\". ";

    if (GetFrame()->GetSecurityContext()->IsSandboxed(SandboxFlags::kOrigin) &&
        accessing_window->IsSandboxed(SandboxFlags::kOrigin)) {
      return "Sandbox access violation: " + message +
             " Both frames are sandboxed and lack the \"allow-same-origin\" "
             "flag.";
    }

    if (GetFrame()->GetSecurityContext()->IsSandboxed(SandboxFlags::kOrigin)) {
      return "Sandbox access violation: " + message +
             " The frame being accessed is sandboxed and lacks the "
             "\"allow-same-origin\" flag.";
    }

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
           target_url.Protocol() + "\". Protocols must match.";

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
  if (cross_document_access == CrossDocumentAccessPolicy::kDisallowed)
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

  if (!GetFrame() || !GetFrame()->IsOutermostMainFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  Document* active_document = incumbent_window->document();
  if (!(active_document && active_document->GetFrame() &&
        active_document->GetFrame()->CanNavigate(*GetFrame()))) {
    return;
  }

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessClose,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClose,
      mojom::blink::WindowProxyAccessType::kClose);

  Settings* settings = GetFrame()->GetSettings();
  bool allow_scripts_to_close_windows =
      settings && settings->GetAllowScriptsToCloseWindows();

  if (!page->OpenedByDOM() && !allow_scripts_to_close_windows) {
    if (GetFrame()->Client()->BackForwardLength() > 1) {
      active_document->domWindow()->GetFrameConsole()->AddMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Scripts may close only the windows that were opened by them."));
      return;
    } else {
      // https://html.spec.whatwg.org/multipage/nav-history-apis.html#script-closable
      // allows a window to be closed if its history length is 1, even if it was
      // not opened by script.
      UseCounter::Count(active_document,
                        WebFeature::kWindowCloseHistoryLengthOne);
    }
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
  Frame* frame = GetFrame();
  if (!frame)
    return;

  Page* page = frame->GetPage();
  if (!page)
    return;

  bool allow_focus_without_user_activation =
      frame->AllowFocusWithoutUserActivation();

  if (!allow_focus_without_user_activation &&
      !frame->HasTransientUserActivation()) {
    // Disallow script focus that crosses a fenced frame boundary on a
    // frame that doesn't have transient user activation. Note: all calls to
    // DOMWindow::focus come from JavaScript calls in the web platform
    return;
  }

  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessFocus,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFocus,
      mojom::blink::WindowProxyAccessType::kFocus);

  // HTML standard doesn't require to check the incumbent realm, but Blink
  // historically checks it for some reasons, maybe the same reason as |close|.
  // (|close| checks whether the incumbent realm is eligible to close the window
  // in order to prevent a (cross origin) window from abusing |close| to close
  // pages randomly or with a malicious intent.)
  // https://html.spec.whatwg.org/C/#dom-window-focus
  // https://html.spec.whatwg.org/C/#focusing-steps
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  LocalFrame* originating_frame = incumbent_window->GetFrame();

  // TODO(mustaq): Use of |allow_focus| and consuming the activation here seems
  // suspicious (https://crbug.com/959815).
  bool allow_focus = incumbent_window->IsWindowInteractionAllowed();
  bool is_focused_from_pip_window = false;
  if (allow_focus) {
    incumbent_window->ConsumeWindowInteraction();
  } else {
    DCHECK(IsMainThread());

    // Allow focus if the request is coming from our opener window.
    allow_focus = opener() && opener() != this && incumbent_window == opener();

    // Also allow focus from a user activation on a document picture-in-picture
    // window opened by this window. In this case, we determine the originating
    // frame to be the picture-in-picture window regardless of whether or not
    // it's also the incumbent frame. `frame` will also always be an outermost
    // main frame in this case since only outermost main frames can open a
    // document picture-in-picture window.
    auto* local_dom_window = DynamicTo<LocalDOMWindow>(this);
    if (local_dom_window) {
      Document* document = local_dom_window->document();
      LocalDOMWindow* pip_window =
          document
              ? PictureInPictureController::GetDocumentPictureInPictureWindow(
                    *document)
              : nullptr;
      if (pip_window &&
          LocalFrame::HasTransientUserActivation(pip_window->GetFrame())) {
        allow_focus = true;
        is_focused_from_pip_window = true;
        originating_frame = pip_window->GetFrame();
      }
    }
  }

  // If we're a top level window, bring the window to the front.
  if (frame->IsOutermostMainFrame() && allow_focus) {
    frame->FocusPage(originating_frame);
  } else if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
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

  // TODO(crbug.com/1458985) Remove the IsInFencedFrameTree condition once
  // fenced frames are enabled by default.
  if (!allow_focus_without_user_activation && frame->IsInFencedFrameTree()) {
    // Fenced frames should consume user activation when attempting to pull
    // focus across a fenced boundary into itself.
    LocalFrame::ConsumeTransientUserActivation(DynamicTo<LocalFrame>(frame));
  }

  // When the focus comes from the document picture-in-picture frame, we consume
  // a user gesture from the picture-in-picture frame.
  if (is_focused_from_pip_window) {
    LocalFrame::ConsumeTransientUserActivation(originating_frame);
  }
}

void DOMWindow::blur() {
  RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessBlur,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageBlur,
      mojom::blink::WindowProxyAccessType::kBlur);
}

InputDeviceCapabilitiesConstants* DOMWindow::GetInputDeviceCapabilities() {
  if (!input_capabilities_) {
    input_capabilities_ =
        MakeGarbageCollected<InputDeviceCapabilitiesConstants>();
  }
  return input_capabilities_.Get();
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

void DOMWindow::InstallCoopAccessMonitor(
    LocalFrame* accessing_frame,
    network::mojom::blink::CrossOriginOpenerPolicyReporterParamsPtr
        coop_reporter_params,
    bool is_in_same_virtual_coop_related_group) {
  ExecutionContext* execution_context =
      accessing_frame->DomWindow()->GetExecutionContext();
  CoopAccessMonitor* monitor =
      MakeGarbageCollected<CoopAccessMonitor>(execution_context);

  DCHECK(accessing_frame->IsMainFrame());
  DCHECK(!accessing_frame->IsInFencedFrameTree());
  monitor->report_type = coop_reporter_params->report_type;
  monitor->accessing_main_frame = accessing_frame->GetLocalFrameToken();
  monitor->endpoint_defined = coop_reporter_params->endpoint_defined;
  monitor->reported_window_url =
      std::move(coop_reporter_params->reported_window_url);
  monitor->is_in_same_virtual_coop_related_group =
      is_in_same_virtual_coop_related_group;

  // `task_runner` is used for handling disconnect, and it uses
  // `TaskType::kInternalDefault` to match the main frame receiver.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kInternalDefault);
  monitor->reporter.Bind(std::move(coop_reporter_params->reporter),
                         std::move(task_runner));
  // CoopAccessMonitor are cleared when their reporter are gone. This avoids
  // accumulation. However it would have been interesting continuing reporting
  // accesses past this point, at least for the ReportingObserver and Devtool.
  // TODO(arthursonzogni): Consider observing |accessing_main_frame| deletion
  // instead.
  monitor->reporter.set_disconnect_handler(
      WTF::BindOnce(&DOMWindow::DisconnectCoopAccessMonitor,
                    WrapWeakPersistent(this), monitor->accessing_main_frame));

  // As long as RenderDocument isn't shipped, it can exist a CoopAccessMonitor
  // for the same |accessing_main_frame|, because it might now host a different
  // Document. Same is true for |this| DOMWindow, it might refer to a window
  // hosting a different document.
  // The new documents will still be part of a different virtual browsing
  // context group, however the new COOPAccessMonitor might now contain updated
  // URLs.
  //
  // There are up to 2 CoopAccessMonitor for the same access, because it can be
  // reported to the accessing and the accessed window at the same time.
  for (Member<CoopAccessMonitor>& old : coop_access_monitor_) {
    if (old->accessing_main_frame == monitor->accessing_main_frame &&
        network::IsAccessFromCoopPage(old->report_type) ==
            network::IsAccessFromCoopPage(monitor->report_type)) {
      // Eagerly reset the connection to prevent the disconnect handler from
      // running, which could remove this new entry.
      old->reporter.reset();
      old = monitor;
      return;
    }
  }
  coop_access_monitor_.push_back(monitor);
  // Any attempts to access |this| window from |accessing_main_frame| will now
  // trigger reports (network, ReportingObserver, Devtool).
}

// Check if the accessing context would be able to access this window if COOP
// was enforced. If this isn't a report is sent.
void DOMWindow::ReportCoopAccess(const char* property_name) {
  if (coop_access_monitor_.empty())  // Fast early return. Very likely true.
    return;

  v8::Isolate* isolate = window_proxy_manager_->GetIsolate();
  LocalDOMWindow* accessing_window = IncumbentDOMWindow(isolate);
  LocalFrame* accessing_frame = accessing_window->GetFrame();

  // A frame might be destroyed, but its context can still be able to execute
  // some code. Those accesses are ignored. See https://crbug.com/1108256.
  if (!accessing_frame)
    return;

  // Iframes are allowed to trigger reports, only when they are same-origin with
  // their top-level document.
  if (accessing_frame->IsCrossOriginToOutermostMainFrame())
    return;

  // We returned early if accessing_frame->IsCrossOriginToOutermostMainFrame()
  // was true. This means we are not in a fenced frame and that the nearest main
  // frame is same-origin. This generally implies accessing_frame->Tree().Top()
  // to be a LocalFrame. On rare occasions same-origin frames in a page might
  // not share a process. This block speculatively returns early to avoid
  // crashing.
  // TODO(https://crbug.com/1183571): Check if crashes are still happening and
  // remove this block.
  if (!accessing_frame->Tree().Top().IsLocalFrame()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  LocalFrame& accessing_main_frame =
      To<LocalFrame>(accessing_frame->Tree().Top());
  const LocalFrameToken accessing_main_frame_token =
      accessing_main_frame.GetLocalFrameToken();

  auto it = coop_access_monitor_.begin();
  while (it != coop_access_monitor_.end()) {
    if ((*it)->accessing_main_frame != accessing_main_frame_token) {
      ++it;
      continue;
    }

    String property_name_as_string = property_name;
    if ((*it)->is_in_same_virtual_coop_related_group &&
        (property_name_as_string == "postMessage" ||
         property_name_as_string == "closed")) {
      ++it;
      continue;
    }

    // TODO(arthursonzogni): Send the blocked-window-url.

    auto location = CaptureSourceLocation(
        ExecutionContext::From(isolate->GetCurrentContext()));
    // TODO(crbug.com/349583610): Update to use SourceLocation typemap.
    auto source_location = network::mojom::blink::SourceLocation::New(
        location->Url() ? location->Url() : "", location->LineNumber(),
        location->ColumnNumber());

    accessing_window->GetFrameConsole()->AddMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            CoopReportOnlyErrorMessage(property_name), location->Clone()));

    CoopAccessMonitor* monitor = *it;

    // If the reporting document hasn't specified any network report
    // endpoint(s), then it is likely not interested in receiving
    // ReportingObserver's reports.
    //
    // TODO(arthursonzogni): Reconsider this decision later, developers might be
    // interested.
    if (monitor->endpoint_defined) {
      if (monitor->reporter.is_bound()) {
        monitor->reporter->QueueAccessReport(
            monitor->report_type, property_name, std::move(source_location),
            std::move(monitor->reported_window_url));
      }
      // Send a coop-access-violation report.
      if (network::IsAccessFromCoopPage(monitor->report_type)) {
        ReportingContext::From(accessing_main_frame.DomWindow())
            ->QueueReport(MakeGarbageCollected<Report>(
                ReportType::kCoopAccessViolation,
                accessing_main_frame.GetDocument()->Url().GetString(),
                MakeGarbageCollected<CoopAccessViolationReportBody>(
                    std::move(location), monitor->report_type,
                    String(property_name), monitor->reported_window_url)));
      }
    }

    // CoopAccessMonitor are used once and destroyed. This avoids sending
    // multiple reports for the same access.
    (*it)->reporter.reset();
    it = coop_access_monitor_.erase(it);
  }
}

void DOMWindow::DoPostMessage(scoped_refptr<SerializedScriptValue> message,
                              const MessagePortArray& ports,
                              const WindowPostMessageOptions* options,
                              LocalDOMWindow* source,
                              ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "DOMWindow::DoPostMessage");
  auto* source_frame = source->GetFrame();
  bool unload_event_in_progress =
      source_frame && source_frame->GetDocument() &&
      source_frame->GetDocument()->UnloadEventInProgress();
  if (!unload_event_in_progress && source_frame && source_frame->GetPage() &&
      source_frame->GetPage()->DispatchedPagehideAndStillHidden()) {
  }
  if (!IsCurrentlyDisplayedInFrame())
    return;

  // Compute the target origin.  We need to do this synchronously in order
  // to generate the SyntaxError exception correctly.
  scoped_refptr<const SecurityOrigin> target =
      PostMessageHelper::GetTargetOrigin(options, *source, exception_state);
  if (exception_state.HadException())
    return;
  if (!target) {
    UseCounter::Count(source, WebFeature::kUnspecifiedTargetOriginPostMessage);
  }

  auto channels = MessagePort::DisentanglePorts(GetExecutionContext(), ports,
                                                exception_state);
  if (exception_state.HadException())
    return;

  const SecurityOrigin* target_security_origin =
      GetFrame()->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* source_security_origin = source->GetSecurityOrigin();
  bool is_source_secure = source_security_origin->IsPotentiallyTrustworthy();
  bool is_target_secure = target_security_origin->IsPotentiallyTrustworthy();
  if (is_target_secure) {
    if (is_source_secure) {
      UseCounter::Count(source, WebFeature::kPostMessageFromSecureToSecure);
    } else {
      UseCounter::Count(source, WebFeature::kPostMessageFromInsecureToSecure);
      if (!GetFrame()
               ->Tree()
               .Top()
               .GetSecurityContext()
               ->GetSecurityOrigin()
               ->IsPotentiallyTrustworthy()) {
        UseCounter::Count(source,
                          WebFeature::kPostMessageFromInsecureToSecureToplevel);
      }
    }
  } else {
    if (is_source_secure) {
      UseCounter::Count(source, WebFeature::kPostMessageFromSecureToInsecure);
    } else {
      UseCounter::Count(source, WebFeature::kPostMessageFromInsecureToInsecure);
    }
  }

  if (source->GetFrame() &&
      source->GetFrame()->Tree().Top() != GetFrame()->Tree().Top()) {
    if ((!target_security_origin->RegistrableDomain() &&
         target_security_origin->Host() == source_security_origin->Host()) ||
        (target_security_origin->RegistrableDomain() &&
         target_security_origin->RegistrableDomain() ==
             source_security_origin->RegistrableDomain())) {
      if (target_security_origin->Protocol() ==
          source_security_origin->Protocol()) {
        UseCounter::Count(source, WebFeature::kSchemefulSameSitePostMessage);
      } else {
        UseCounter::Count(source, WebFeature::kSchemelesslySameSitePostMessage);
        if (is_source_secure && !is_target_secure) {
          UseCounter::Count(
              source,
              WebFeature::kSchemelesslySameSitePostMessageSecureToInsecure);
        } else if (!is_source_secure && is_target_secure) {
          UseCounter::Count(
              source,
              WebFeature::kSchemelesslySameSitePostMessageInsecureToSecure);
        }
      }
    } else {
      UseCounter::Count(source, WebFeature::kCrossSitePostMessage);
    }
  }
  auto* local_dom_window = DynamicTo<LocalDOMWindow>(this);
  KURL target_url = local_dom_window
                        ? local_dom_window->Url()
                        : KURL(NullURL(), target_security_origin->ToString());
  if (!source->GetContentSecurityPolicy()->AllowConnectToSource(
          target_url, target_url, RedirectStatus::kNoRedirect,
          ReportingDisposition::kSuppressReporting)) {
    UseCounter::Count(
        source, WebFeature::kPostMessageOutgoingWouldBeBlockedByConnectSrc);
  }
  UserActivation* user_activation = nullptr;
  if (options->includeUserActivation())
    user_activation = UserActivation::CreateSnapshot(source);

  // Capability Delegation permits a script to delegate its ability to call a
  // restricted API to another browsing context it trusts. User activation is
  // currently consumed when a supported capability is specified, to prevent
  // potentially abusive repeated delegation attempts.
  // https://wicg.github.io/capability-delegation/spec.html
  // TODO(mustaq): Explore use cases for delegating multiple capabilities.
  mojom::blink::DelegatedCapability delegated_capability =
      mojom::blink::DelegatedCapability::kNone;
  if (options->hasDelegate()) {
    Vector<String> capability_list;
    options->delegate().Split(' ', capability_list);
    if (capability_list.Contains("payment")) {
      delegated_capability = mojom::blink::DelegatedCapability::kPaymentRequest;
    } else if (capability_list.Contains("fullscreen")) {
      delegated_capability =
          mojom::blink::DelegatedCapability::kFullscreenRequest;
    } else if (capability_list.Contains("display-capture")) {
      delegated_capability =
          mojom::blink::DelegatedCapability::kDisplayCaptureRequest;
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "Delegation of \'" + options->delegate() + "\' is not supported.");
      return;
    }

    // TODO(mustaq): Add checks for allowed-to-use policy as proposed here:
    // https://wicg.github.io/capability-delegation/spec.html#monkey-patch-to-html-initiating-delegation

    if (!target) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Delegation to target origin '*' is not allowed.");
      return;
    }

    if (!LocalFrame::HasTransientUserActivation(source_frame)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Delegation is not allowed without transient user activation.");
      return;
    }

    LocalFrame::ConsumeTransientUserActivation(source_frame);
  }

  PostedMessage* posted_message = MakeGarbageCollected<PostedMessage>();
  posted_message->source_origin = source->GetSecurityOrigin();
  posted_message->target_origin = std::move(target);
  posted_message->data = std::move(message);
  posted_message->channels = std::move(channels);
  posted_message->source = source;
  posted_message->user_activation = user_activation;
  posted_message->delegated_capability = delegated_capability;
  SchedulePostMessage(posted_message);
}

void DOMWindow::RecordWindowProxyAccessMetrics(
    WebFeature property_access,
    WebFeature property_access_from_other_page,
    mojom::blink::WindowProxyAccessType access_type) const {
  if (!GetFrame())
    return;

  v8::Isolate* isolate = window_proxy_manager_->GetIsolate();
  if (!isolate)
    return;

  LocalDOMWindow* accessing_window = CurrentDOMWindow(isolate);
  if (!accessing_window)
    return;

  LocalFrame* accessing_frame = accessing_window->GetFrame();
  if (!accessing_frame)
    return;

  // We don't log instances of a frame accessing itself. This would cause
  // unacceptable lag (via mojom) and rate-limiting on the UKM.
  if (GetFrame() != accessing_frame) {
    // This sends a message to the browser process to record metrics. As of
    // 2024, these metrics are heavily downsampled in the browser process,
    // through the UKM downsampling mechanism. Perform the downsampling here, to
    // save on the IPC cost. The sampling ratio is based on observed
    // browser-side downsampling rates.
    if (!base::FeatureList::IsEnabled(
            features::kSubSampleWindowProxyUsageMetrics) ||
        metrics_sub_sampler_.ShouldSample(0.0001)) {
      accessing_frame->GetLocalFrameHostRemote().RecordWindowProxyUsageMetrics(
          GetFrame()->GetFrameToken(), access_type);
    }
  }

  // Note that SecurityOrigin can be null in unit tests.
  if (!GetFrame()->GetSecurityContext()->GetSecurityOrigin() ||
      !accessing_frame->GetSecurityContext()->GetSecurityOrigin() ||
      accessing_frame->GetSecurityContext()
          ->GetSecurityOrigin()
          ->IsSameOriginWith(
              GetFrame()->GetSecurityContext()->GetSecurityOrigin())) {
    return;
  }
  UseCounter::Count(accessing_window->document(), property_access);

  if (accessing_frame->GetPage() != GetFrame()->GetPage()) {
    UseCounter::Count(accessing_window, property_access_from_other_page);
  }
}

std::optional<DOMWindow::ProxyAccessBlockedReason>
DOMWindow::GetProxyAccessBlockedReason(v8::Isolate* isolate) const {
  if (!GetFrame()) {
    // Proxy is disconnected so we cannot take any action anyway.
    return std::nullopt;
  }

  LocalDOMWindow* accessing_window = CurrentDOMWindow(isolate);
  CHECK(accessing_window);

  LocalFrame* accessing_frame = accessing_window->GetFrame();
  if (!accessing_frame) {
    // Context is disconnected so we cannot take any action anyway.
    return std::nullopt;
  }

  // Returns an exception message if this window proxy or the window accessing
  // are not in the same page and one is in a partitioned popin. We check this
  // case first as it overlaps with the COOP:RP case below.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (GetFrame()->GetPage() != accessing_frame->GetPage() &&
      (accessing_frame->GetPage()->IsPartitionedPopin() ||
       GetFrame()->GetPage()->IsPartitionedPopin())) {
    return DOMWindow::ProxyAccessBlockedReason::kPartitionedPopins;
  }

  // Returns an exception message if the two windows are in the same
  // CoopRelatedGroup but not in the same BrowsingInstance as this means COOP:
  // restrict-properties is blocking access between the contexts.
  // TODO(https://crbug.com/1464618): Is there actually any scenario where
  // cross browsing context group was allowed before COOP: restrict-properties?
  // Verify that we need to have this check.
  if (accessing_frame->GetPage()->CoopRelatedGroupToken() ==
          GetFrame()->GetPage()->CoopRelatedGroupToken() &&
      accessing_frame->GetPage()->BrowsingContextGroupToken() !=
          GetFrame()->GetPage()->BrowsingContextGroupToken()) {
    return DOMWindow::ProxyAccessBlockedReason::kCoopRp;
  }

  // Our fallback allows access.
  return std::nullopt;
}

// static
String DOMWindow::GetProxyAccessBlockedExceptionMessage(
    DOMWindow::ProxyAccessBlockedReason reason) {
  switch (reason) {
    case ProxyAccessBlockedReason::kCoopRp:
      return "Cross-Origin-Opener-Policy: 'restrict-properties' blocked the "
             "access.";
    case ProxyAccessBlockedReason::kPartitionedPopins:
      return "Partitioned Popin blocked the access.";
  }
}

void DOMWindow::PostedMessage::Trace(Visitor* visitor) const {
  visitor->Trace(source);
  visitor->Trace(user_activation);
}

BlinkTransferableMessage
DOMWindow::PostedMessage::ToBlinkTransferableMessage() && {
  BlinkTransferableMessage result;

  result.message = std::move(data);
  result.sender_agent_cluster_id = source->GetAgentClusterID();
  result.locked_to_sender_agent_cluster =
      result.message->IsLockedToAgentCluster();

  result.ports = std::move(channels);

  if (user_activation) {
    result.user_activation = mojom::blink::UserActivationSnapshot::New(
        user_activation->hasBeenActive(), user_activation->isActive());
  }

  result.delegated_capability = delegated_capability;

  return result;
}

void DOMWindow::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(window_proxy_manager_);
  visitor->Trace(input_capabilities_);
  visitor->Trace(location_);
  visitor->Trace(coop_access_monitor_);
  EventTarget::Trace(visitor);
}

void DOMWindow::DisconnectCoopAccessMonitor(
    const LocalFrameToken& accessing_main_frame) {
  auto it = coop_access_monitor_.begin();
  while (it != coop_access_monitor_.end()) {
    if ((*it)->accessing_main_frame == accessing_main_frame) {
      it = coop_access_monitor_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace blink
