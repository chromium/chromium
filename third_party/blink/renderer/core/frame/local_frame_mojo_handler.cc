// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_mojo_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/savable_resources.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/view_transition/page_swap_event.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "third_party/blink/renderer/core/editing/substring_util.h"
#include "third_party/blink/renderer/platform/fonts/mac/attributed_string_type_converter.h"
#include "ui/base/mojom/attributed_string.mojom-blink.h"
#endif

namespace blink {

namespace {

constexpr char kInvalidWorldID[] =
    "JavaScriptExecuteRequestInIsolatedWorld gets an invalid world id.";

#if BUILDFLAG(IS_MAC)
size_t GetCurrentCursorPositionInFrame(LocalFrame* local_frame) {
  blink::WebRange range =
      WebLocalFrameImpl::FromFrame(local_frame)->SelectionRange();
  return range.IsNull() ? size_t{0} : static_cast<size_t>(range.StartOffset());
}
#endif

RemoteFrame* SourceFrameForOptionalToken(
    const std::optional<RemoteFrameToken>& source_frame_token) {
  if (!source_frame_token)
    return nullptr;
  return RemoteFrame::FromFrameToken(source_frame_token.value());
}

v8::Local<v8::Context> MainWorldScriptContext(LocalFrame* local_frame) {
  ScriptState* script_state = ToScriptStateForMainWorld(local_frame);
  DCHECK(script_state);
  return script_state->GetContext();
}

base::Value GetJavaScriptExecutionResult(v8::Local<v8::Value> result,
                                         v8::Local<v8::Context> context,
                                         WebV8ValueConverter* converter) {
  if (!result.IsEmpty()) {
    v8::Context::Scope context_scope(context);
    std::unique_ptr<base::Value> new_value =
        converter->FromV8Value(result, context);
    if (new_value)
      return std::move(*new_value);
  }
  return base::Value();
}

v8::MaybeLocal<v8::Value> GetProperty(v8::Local<v8::Context> context,
                                      v8::Local<v8::Value> object,
                                      const String& name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> name_str = V8String(isolate, name);
  v8::Local<v8::Object> object_obj;
  if (!object->ToObject(context).ToLocal(&object_obj)) {
    return v8::MaybeLocal<v8::Value>();
  }
  return object_obj->Get(context, name_str);
}

v8::MaybeLocal<v8::Value> CallMethodOnFrame(LocalFrame* local_frame,
                                            const String& object_name,
                                            const String& method_name,
                                            base::Value::List arguments,
                                            WebV8ValueConverter* converter) {
  v8::Local<v8::Context> context = MainWorldScriptContext(local_frame);

  v8::Context::Scope context_scope(context);
  v8::LocalVector<v8::Value> args(context->GetIsolate());
  for (const auto& argument : arguments) {
    args.push_back(converter->ToV8Value(argument, context));
  }

  v8::Local<v8::Value> object;
  v8::Local<v8::Value> method;
  if (!GetProperty(context, context->Global(), object_name).ToLocal(&object) ||
      !GetProperty(context, object, method_name).ToLocal(&method) ||
      !method->IsFunction()) {
    return v8::MaybeLocal<v8::Value>();
  }
  CHECK(method->IsFunction());

  return local_frame->DomWindow()
      ->GetScriptController()
      .EvaluateMethodInMainWorld(v8::Local<v8::Function>::Cast(method), object,
                                 static_cast<int>(args.size()), args.data());
}

HitTestResult HitTestResultForRootFramePos(
    LocalFrame* frame,
    const PhysicalOffset& pos_in_root_frame) {
  HitTestLocation location(
      frame->View()->ConvertFromRootFrame(pos_in_root_frame));
  HitTestResult result = frame->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInUAShadowRoot();
  return result;
}

void ParseOpenGraphProperty(const HTMLMetaElement& element,
                            const Document& document,
                            mojom::blink::OpenGraphMetadata* metadata) {
  if (element.Property() == "og:image" && !metadata->image)
    metadata->image = document.CompleteURL(element.Content());

  // Non-OpenGraph, non-standard thing that some sites use the same way:
  // using <meta itemprop="image" content="$url">, which means the same thing
  // as <meta property="og:image" content="$url".
  if (element.Itemprop() == "image" && !metadata->image)
    metadata->image = document.CompleteURL(element.Content());
}

// Convert the error to a string so it can be sent back to the test.
//
// We try to use .stack property so that the error message contains a stack
// trace, but otherwise fallback to .toString().
v8::Local<v8::String> ErrorToString(ScriptState* script_state,
                                    v8::Local<v8::Value> error) {
  if (!error.IsEmpty()) {
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Value> value =
        v8::TryCatch::StackTrace(context, error).FromMaybe(error);
    v8::Local<v8::String> value_string;
    if (value->ToString(context).ToLocal(&value_string))
      return value_string;
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  return v8::String::NewFromUtf8Literal(isolate, "Unknown Failure");
}

class JavaScriptExecuteRequestForTestsHandler
    : public GarbageCollected<JavaScriptExecuteRequestForTestsHandler> {
 public:
  class PromiseCallback : public ScriptFunction::Callable {
   public:
    PromiseCallback(JavaScriptExecuteRequestForTestsHandler& handler,
                    mojom::blink::JavaScriptExecutionResultType type)
        : handler_(handler), type_(type) {}

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
      DCHECK(script_state);
      if (type_ == mojom::blink::JavaScriptExecutionResultType::kSuccess)
        handler_->SendSuccess(script_state, value.V8Value());
      else
        handler_->SendException(script_state, value.V8Value());
      return {};
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(handler_);
      ScriptFunction::Callable::Trace(visitor);
    }

   private:
    Member<JavaScriptExecuteRequestForTestsHandler> handler_;
    const mojom::blink::JavaScriptExecutionResultType type_;
  };

  explicit JavaScriptExecuteRequestForTestsHandler(
      LocalFrameMojoHandler::JavaScriptExecuteRequestForTestsCallback callback)
      : callback_(std::move(callback)) {}

  ~JavaScriptExecuteRequestForTestsHandler() {
    if (callback_) {
      std::move(callback_).Run(
          mojom::blink::JavaScriptExecutionResultType::kException,
          base::Value(
              "JavaScriptExecuteRequestForTestsHandler was destroyed without "
              "running the callback. This is usually caused by Promise "
              "resolution functions getting destroyed without being called."));
    }
  }

  ScriptFunction* CreateResolveCallback(ScriptState* script_state,
                                        LocalFrame* frame) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<PromiseCallback>(
            *this, mojom::blink::JavaScriptExecutionResultType::kSuccess));
  }

  ScriptFunction* CreateRejectCallback(ScriptState* script_state,
                                       LocalFrame* frame) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<PromiseCallback>(
            *this, mojom::blink::JavaScriptExecutionResultType::kException));
  }

  void SendSuccess(ScriptState* script_state, v8::Local<v8::Value> value) {
    SendResponse(script_state,
                 mojom::blink::JavaScriptExecutionResultType::kSuccess, value);
  }

  void SendException(ScriptState* script_state, v8::Local<v8::Value> error) {
    SendResponse(script_state,
                 mojom::blink::JavaScriptExecutionResultType::kException,
                 ErrorToString(script_state, error));
  }

  void Trace(Visitor* visitor) const {}

 private:
  void SendResponse(ScriptState* script_state,
                    mojom::blink::JavaScriptExecutionResultType type,
                    v8::Local<v8::Value> value) {
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();
    converter->SetDateAllowed(true);
    converter->SetRegExpAllowed(true);

    CHECK(callback_) << "Promise resolved twice";
    std::move(callback_).Run(
        type, GetJavaScriptExecutionResult(value, script_state->GetContext(),
                                           converter.get()));
  }

  LocalFrameMojoHandler::JavaScriptExecuteRequestForTestsCallback callback_;
};

}  // namespace

ActiveURLMessageFilter::~ActiveURLMessageFilter() {
  if (debug_url_set_) {
    Platform::Current()->SetActiveURL(WebURL(), WebString());
  }
}

bool ActiveURLMessageFilter::WillDispatch(mojo::Message* message) {
  // We expect local_frame_ always to be set because this MessageFilter
  // is owned by the LocalFrame. We do not want to introduce a Persistent
  // reference so we don't cause a cycle. If you hit this CHECK then you
  // likely didn't reset your mojo receiver in Detach.
  CHECK(local_frame_);
  debug_url_set_ = true;
  Platform::Current()->SetActiveURL(local_frame_->GetDocument()->Url(),
                                    local_frame_->Top()
                                        ->GetSecurityContext()
                                        ->GetSecurityOrigin()
                                        ->ToString());
  return true;
}

void ActiveURLMessageFilter::DidDispatchOrReject(mojo::Message* message,
                                                 bool accepted) {
  Platform::Current()->SetActiveURL(WebURL(), WebString());
  debug_url_set_ = false;
}

LocalFrameMojoHandler::LocalFrameMojoHandler(blink::LocalFrame& frame)
    : frame_(frame) {
  frame.GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      back_forward_cache_controller_host_remote_.BindNewEndpointAndPassReceiver(
          frame.GetTaskRunner(TaskType::kInternalDefault)));
#if BUILDFLAG(IS_MAC)
  // It should be bound before accessing TextInputHost which is the interface to
  // respond to GetCharacterIndexAtPoint.
  frame.GetBrowserInterfaceBroker().GetInterface(
      text_input_host_.BindNewPipeAndPassReceiver(
          frame.GetTaskRunner(TaskType::kInternalDefault)));
#endif

  frame.GetBrowserInterfaceBroker().GetInterface(
      non_associated_local_frame_host_remote_.BindNewPipeAndPassReceiver(
          frame.GetTaskRunner(TaskType::kInternalHighPriorityLocalFrame)));

  frame.GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      local_frame_host_remote_.BindNewEndpointAndPassReceiver(
          frame.GetTaskRunner(TaskType::kInternalDefault)));

  auto* registry = frame.GetInterfaceRegistry();
  registry->AddAssociatedInterface(
      WTF::BindRepeating(&LocalFrameMojoHandler::BindToLocalFrameReceiver,
                         WrapWeakPersistent(this)));
  registry->AddAssociatedInterface(WTF::BindRepeating(
      &LocalFrameMojoHandler::BindFullscreenVideoElementReceiver,
      WrapWeakPersistent(this)));
}

void LocalFrameMojoHandler::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(back_forward_cache_controller_host_remote_);
#if BUILDFLAG(IS_MAC)
  visitor->Trace(text_input_host_);
#endif
  visitor->Trace(reporting_service_);
  visitor->Trace(device_posture_provider_service_);
  visitor->Trace(local_frame_host_remote_);
  visitor->Trace(non_associated_local_frame_host_remote_);
  visitor->Trace(local_frame_receiver_);
  visitor->Trace(main_frame_receiver_);
  visitor->Trace(fullscreen_video_receiver_);
  visitor->Trace(device_posture_receiver_);
}

void LocalFrameMojoHandler::WasAttachedAsLocalMainFrame() {
  frame_->GetInterfaceRegistry()->AddAssociatedInterface(
      WTF::BindRepeating(&LocalFrameMojoHandler::BindToMainFrameReceiver,
                         WrapWeakPersistent(this)));
}

void LocalFrameMojoHandler::DidDetachFrame() {
  // We reset receivers explicitly because HeapMojoReceiver does not
  // automatically reset on context destruction.
  local_frame_receiver_.reset();
  main_frame_receiver_.reset();
  // TODO(tkent): Should we reset other receivers?
}

void LocalFrameMojoHandler::ClosePageForTesting() {
  ClosePage(base::DoNothing());
}

mojom::blink::BackForwardCacheControllerHost&
LocalFrameMojoHandler::BackForwardCacheControllerHostRemote() {
  return *back_forward_cache_controller_host_remote_.get();
}

#if BUILDFLAG(IS_MAC)
mojom::blink::TextInputHost& LocalFrameMojoHandler::TextInputHost() {
  DCHECK(text_input_host_.is_bound());
  return *text_input_host_.get();
}

void LocalFrameMojoHandler::ResetTextInputHostForTesting() {
  text_input_host_.reset();
}

void LocalFrameMojoHandler::RebindTextInputHostForTesting() {
  frame_->GetBrowserInterfaceBroker().GetInterface(
      text_input_host_.BindNewPipeAndPassReceiver(
          frame_->GetTaskRunner(TaskType::kInternalDefault)));
}
#endif

mojom::blink::ReportingServiceProxy* LocalFrameMojoHandler::ReportingService() {
  if (!reporting_service_.is_bound()) {
    frame_->GetBrowserInterfaceBroker().GetInterface(
        reporting_service_.BindNewPipeAndPassReceiver(
            frame_->GetTaskRunner(TaskType::kInternalDefault)));
  }
  return reporting_service_.get();
}

mojom::blink::DevicePostureProvider*
LocalFrameMojoHandler::DevicePostureProvider() {
  if (!frame_->IsLocalRoot()) {
    return frame_->LocalFrameRoot().GetDevicePostureProvider();
  }

  DCHECK(frame_->IsLocalRoot());
  if (!device_posture_provider_service_.is_bound()) {
    auto task_runner = frame_->GetTaskRunner(TaskType::kInternalDefault);
    frame_->GetBrowserInterfaceBroker().GetInterface(
        device_posture_provider_service_.BindNewPipeAndPassReceiver(
            task_runner));
  }
  return device_posture_provider_service_.get();
}

mojom::blink::DevicePostureType LocalFrameMojoHandler::GetDevicePosture() {
  if (!frame_->IsLocalRoot()) {
    return frame_->LocalFrameRoot().GetDevicePosture();
  }

  DCHECK(frame_->IsLocalRoot());
  if (device_posture_receiver_.is_bound()) {
    return current_device_posture_;
  }

  auto task_runner = frame_->GetTaskRunner(TaskType::kInternalDefault);
  DevicePostureProvider()->AddListenerAndGetCurrentPosture(
      device_posture_receiver_.BindNewPipeAndPassRemote(task_runner),
      WTF::BindOnce(&LocalFrameMojoHandler::OnPostureChanged,
                    WrapPersistent(this)));
  return current_device_posture_;
}

void LocalFrameMojoHandler::OverrideDevicePostureForEmulation(
    mojom::blink::DevicePostureType device_posture_param) {
  DevicePostureProvider()->OverrideDevicePostureForEmulation(
      device_posture_param);
}

void LocalFrameMojoHandler::DisableDevicePostureOverrideForEmulation() {
  DevicePostureProvider()->DisableDevicePostureOverrideForEmulation();
}

Page* LocalFrameMojoHandler::GetPage() const {
  return frame_->GetPage();
}

LocalDOMWindow* LocalFrameMojoHandler::DomWindow() const {
  return frame_->DomWindow();
}

Document* LocalFrameMojoHandler::GetDocument() const {
  return frame_->GetDocument();
}

void LocalFrameMojoHandler::BindToLocalFrameReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::LocalFrame> receiver) {
  if (frame_->IsDetached())
    return;

  local_frame_receiver_.Bind(std::move(receiver),
                             frame_->GetTaskRunner(TaskType::kInternalDefault));
  local_frame_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoHandler::BindToMainFrameReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrame> receiver) {
  if (frame_->IsDetached())
    return;

  main_frame_receiver_.Bind(std::move(receiver),
                            frame_->GetTaskRunner(TaskType::kInternalDefault));
  main_frame_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoHandler::BindFullscreenVideoElementReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::FullscreenVideoElementHandler>
        receiver) {
  if (frame_->IsDetached())
    return;

  fullscreen_video_receiver_.Bind(
      std::move(receiver), frame_->GetTaskRunner(TaskType::kInternalDefault));
  fullscreen_video_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoHandler::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  SurroundingText surrounding_text(frame_, max_length);

  // |surrounding_text| might not be correctly initialized, for example if
  // |frame_->SelectionRange().IsNull()|, in other words, if there was no
  // selection.
  if (surrounding_text.IsEmpty()) {
    // Don't use WTF::String's default constructor so that we make sure that we
    // always send a valid empty string over the wire instead of a null pointer.
    std::move(callback).Run(g_empty_string, 0, 0);
    return;
  }

  std::move(callback).Run(surrounding_text.TextContent(),
                          surrounding_text.StartOffsetInTextContent(),
                          surrounding_text.EndOffsetInTextContent());
}

void LocalFrameMojoHandler::SendInterventionReport(const String& id,
                                                   const String& message) {
  Intervention::GenerateReport(frame_, id, message);
}

void LocalFrameMojoHandler::SetFrameOwnerProperties(
    mojom::blink::FrameOwnerPropertiesPtr properties) {
  GetDocument()->WillChangeFrameOwnerProperties(
      properties->margin_width, properties->margin_height,
      properties->scrollbar_mode, properties->is_display_none,
      properties->color_scheme, properties->preferred_color_scheme);

  frame_->ApplyFrameOwnerProperties(std::move(properties));
}

void LocalFrameMojoHandler::NotifyUserActivation(
    mojom::blink::UserActivationNotificationType notification_type) {
  frame_->NotifyUserActivation(notification_type);
}

void LocalFrameMojoHandler::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  Page* page = GetPage();
  if (!page)
    return;

  // The rect passed to us from content is in DIP, relative to the main frame.
  // This doesn't take the page's zoom factor into account so we must scale by
  // the inverse of the page zoom in order to get correct client coordinates.
  // WindowToViewportScalar is the device scale factor while LayoutZoomFactor is
  // the combination of the device scale factor and the zoom factor of the
  // page.
  blink::LocalFrame& local_frame_root = frame_->LocalFrameRoot();
  const float window_to_viewport_factor =
      page->GetChromeClient().WindowToViewportScalar(&local_frame_root, 1.0f);
  const float zoom_factor = local_frame_root.LayoutZoomFactor();
  const float scale_factor = zoom_factor / window_to_viewport_factor;
  gfx::Rect scaled_rect(keyboard_rect.x() / scale_factor,
                        keyboard_rect.y() / scale_factor,
                        keyboard_rect.width() / scale_factor,
                        keyboard_rect.height() / scale_factor);

  frame_->NotifyVirtualKeyboardOverlayRectObservers(scaled_rect);
}

void LocalFrameMojoHandler::AddMessageToConsole(
    mojom::blink::ConsoleMessageLevel level,
    const WTF::String& message,
    bool discard_duplicates) {
  GetDocument()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther, level, message),
      discard_duplicates);
}

void LocalFrameMojoHandler::SwapInImmediately() {
  frame_->SwapIn();
  // Normally, this happens as part of committing a cross-Document navigation.
  // However, there is no navigation being committed here. Instead, the browser
  // navigation code is optimistically early-swapping in this frame to replace a
  // crashed subframe after starting a navigation.
  //
  // While the provisional frame has a unique opaque origin, the Blink bindings
  // code still expects the WindowProxy to be initialized for the access check
  // failed callbacks.
  DomWindow()->GetScriptController().UpdateDocument();
}

void LocalFrameMojoHandler::CheckCompleted() {
  frame_->CheckCompleted();
}

void LocalFrameMojoHandler::StopLoading() {
  frame_->Loader().StopAllLoaders(/*abort_client=*/true);

  // The stopLoading handler may run script, which may cause this frame to be
  // detached/deleted. If that happens, return immediately.
  if (!frame_->IsAttached())
    return;

  // Notify RenderFrame observers.
  WebLocalFrameClient* client = frame_->Client()->GetWebFrame()->Client();
  if (client)
    client->OnStopLoading();
}

void LocalFrameMojoHandler::Collapse(bool collapsed) {
  FrameOwner* owner = frame_->Owner();
  To<HTMLFrameOwnerElement>(owner)->SetCollapsed(collapsed);
}

void LocalFrameMojoHandler::EnableViewSourceMode() {
  DCHECK(frame_->IsOutermostMainFrame());
  frame_->SetInViewSourceMode(true);
}

void LocalFrameMojoHandler::Focus() {
  frame_->FocusImpl();
}

void LocalFrameMojoHandler::ClearFocusedElement() {
  Document* document = GetDocument();
  Element* old_focused_element = document->FocusedElement();
  document->ClearFocusedElement();
  if (!old_focused_element)
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  document->UpdateStyleAndLayoutTree();
  if (IsEditable(*old_focused_element) ||
      old_focused_element->IsTextControl()) {
    frame_->Selection().Clear();
  }
}

void LocalFrameMojoHandler::CopyImageAt(const gfx::Point& window_point) {
  gfx::Point viewport_position =
      frame_->GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  frame_->CopyImageAtViewportPoint(viewport_position);
}

void LocalFrameMojoHandler::SaveImageAt(const gfx::Point& window_point) {
  frame_->SaveImageAt(window_point);
}

void LocalFrameMojoHandler::ReportBlinkFeatureUsage(
    const Vector<mojom::blink::WebFeature>& features) {
  DCHECK(!features.empty());

  // Assimilate all features used/performed by the browser into UseCounter.
  auto* document = GetDocument();
  DCHECK(document);
  for (const auto& feature : features)
    document->CountUse(feature);
}

void LocalFrameMojoHandler::RenderFallbackContent() {
  frame_->RenderFallbackContent();
}

void LocalFrameMojoHandler::BeforeUnload(bool is_reload,
                                         BeforeUnloadCallback callback) {
  base::TimeTicks before_unload_start_time = base::TimeTicks::Now();

  // This will execute the BeforeUnload event in this frame and all of its
  // local descendant frames, including children of remote frames.  The browser
  // process will send separate IPCs to dispatch beforeunload in any
  // out-of-process child frames.
  bool proceed = frame_->Loader().ShouldClose(is_reload);

  DCHECK(!callback.is_null());
  base::TimeTicks before_unload_end_time = base::TimeTicks::Now();
  std::move(callback).Run(proceed, before_unload_start_time,
                          before_unload_end_time);
}

void LocalFrameMojoHandler::MediaPlayerActionAt(
    const gfx::Point& window_point,
    blink::mojom::blink::MediaPlayerActionPtr action) {
  gfx::Point viewport_position =
      frame_->GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  frame_->MediaPlayerActionAtViewportPoint(viewport_position, action->type,
                                           action->enable);
}

void LocalFrameMojoHandler::RequestVideoFrameAtWithBoundsHint(
    const gfx::Point& window_point,
    const gfx::Size& max_size,
    int max_area,
    RequestVideoFrameAtWithBoundsHintCallback callback) {
  gfx::Point viewport_position =
      frame_->GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  frame_->RequestVideoFrameAtWithBoundsHint(viewport_position, max_size,
                                            max_area, std::move(callback));
}

void LocalFrameMojoHandler::AdvanceFocusInFrame(
    mojom::blink::FocusType focus_type,
    const std::optional<RemoteFrameToken>& source_frame_token) {
  RemoteFrame* source_frame =
      source_frame_token ? SourceFrameForOptionalToken(*source_frame_token)
                         : nullptr;
  if (!source_frame) {
    SetInitialFocus(focus_type == mojom::blink::FocusType::kBackward);
    return;
  }

  GetPage()->GetFocusController().AdvanceFocusAcrossFrames(
      focus_type, source_frame, frame_);
}

void LocalFrameMojoHandler::AdvanceFocusForIME(
    mojom::blink::FocusType focus_type) {
  auto* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  if (focused_frame != frame_)
    return;

  DCHECK(GetDocument());
  Element* element = GetDocument()->FocusedElement();
  if (!element)
    return;

  Element* next_element =
      GetPage()->GetFocusController().NextFocusableElementForImeAndAutofill(
          element, focus_type);
  if (!next_element)
    return;

  next_element->scrollIntoViewIfNeeded(true /*centerIfNeeded*/);
  next_element->Focus(FocusParams(FocusTrigger::kUserGesture));
}

void LocalFrameMojoHandler::ReportContentSecurityPolicyViolation(
    network::mojom::blink::CSPViolationPtr violation) {
  auto source_location = std::make_unique<SourceLocation>(
      violation->source_location->url, String(),
      violation->source_location->line, violation->source_location->column,
      nullptr);

  frame_->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kSecurity,
      mojom::blink::ConsoleMessageLevel::kError, violation->console_message,
      source_location->Clone()));

  auto directive_type =
      ContentSecurityPolicy::GetDirectiveType(violation->effective_directive);
  blink::LocalFrame* context_frame =
      directive_type == network::mojom::blink::CSPDirectiveName::FrameAncestors
          ? frame_
          : nullptr;

  DomWindow()->GetContentSecurityPolicy()->ReportViolation(
      violation->directive, directive_type, violation->console_message,
      violation->blocked_url, violation->report_endpoints,
      violation->use_reporting_api, violation->header, violation->type,
      ContentSecurityPolicyViolationType::kURLViolation,
      std::move(source_location), context_frame, nullptr /* Element */);
}

void LocalFrameMojoHandler::DidUpdateFramePolicy(
    const FramePolicy& frame_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  SECURITY_CHECK(IsA<RemoteFrameOwner>(frame_->Owner()));
  To<RemoteFrameOwner>(frame_->Owner())->SetFramePolicy(frame_policy);
}

void LocalFrameMojoHandler::OnFrameVisibilityChanged(
    mojom::blink::FrameVisibility visibility) {
  if (frame_->Client() && frame_->Client()->GetWebFrame() &&
      frame_->Client()->GetWebFrame()->Client()) {
    frame_->Client()->GetWebFrame()->Client()->OnFrameVisibilityChanged(
        visibility);
  }
}

void LocalFrameMojoHandler::OnPostureChanged(
    mojom::blink::DevicePostureType posture) {
  if (!RuntimeEnabledFeatures::DevicePostureEnabled(
          GetDocument()->GetExecutionContext())) {
    return;
  }
  current_device_posture_ = posture;
  // A change of the device posture requires re-evaluation of media queries
  // for the local frame subtree (the device posture affect the
  // "device-posture" feature).
  frame_->MediaQueryAffectingValueChangedForLocalSubtree(
      MediaValueChange::kOther);
}

void LocalFrameMojoHandler::PostMessageEvent(
    const std::optional<RemoteFrameToken>& source_frame_token,
    const String& source_origin,
    const String& target_origin,
    BlinkTransferableMessage message) {
  frame_->PostMessageEvent(source_frame_token, source_origin, target_origin,
                           std::move(message));
}

void LocalFrameMojoHandler::JavaScriptMethodExecuteRequest(
    const String& object_name,
    const String& method_name,
    base::Value::List arguments,
    bool wants_result,
    JavaScriptMethodExecuteRequestCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptMethodExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();
  converter->SetDateAllowed(true);
  converter->SetRegExpAllowed(true);

  v8::HandleScope handle_scope(ToIsolate(frame_));
  v8::Local<v8::Value> result;
  if (!CallMethodOnFrame(frame_, object_name, method_name, std::move(arguments),
                         converter.get())
           .ToLocal(&result)) {
    std::move(callback).Run({});
  } else if (wants_result) {
    v8::Local<v8::Context> context = MainWorldScriptContext(frame_);
    std::move(callback).Run(
        GetJavaScriptExecutionResult(result, context, converter.get()));
  } else {
    std::move(callback).Run({});
  }
}

void LocalFrameMojoHandler::JavaScriptExecuteRequest(
    const String& javascript,
    bool wants_result,
    JavaScriptExecuteRequestCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  v8::HandleScope handle_scope(ToIsolate(frame_));
  v8::Local<v8::Value> result =
      ClassicScript::CreateUnspecifiedScript(javascript)
          ->RunScriptAndReturnValue(DomWindow())
          .GetSuccessValueOrEmpty();

  if (wants_result) {
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();
    converter->SetDateAllowed(true);
    converter->SetRegExpAllowed(true);

    v8::Local<v8::Context> context = MainWorldScriptContext(frame_);
    std::move(callback).Run(
        GetJavaScriptExecutionResult(result, context, converter.get()));
  } else {
    std::move(callback).Run({});
  }
}

void LocalFrameMojoHandler::JavaScriptExecuteRequestForTests(
    const String& javascript,
    bool has_user_gesture,
    bool resolve_promises,
    bool honor_js_content_settings,
    int32_t world_id,
    JavaScriptExecuteRequestForTestsCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptExecuteRequestForTests",
                       TRACE_EVENT_SCOPE_THREAD);

  // A bunch of tests expect to run code in the context of a user gesture, which
  // can grant additional privileges (e.g. the ability to create popups).
  if (has_user_gesture)
    NotifyUserActivation(mojom::blink::UserActivationNotificationType::kTest);

  v8::Isolate* isolate = ToIsolate(frame_);
  ScriptState* script_state =
      (world_id == DOMWrapperWorld::kMainWorldId)
          ? ToScriptStateForMainWorld(frame_)
          : ToScriptState(frame_, *DOMWrapperWorld::EnsureIsolatedWorld(
                                      isolate, world_id));
  ScriptState::Scope script_state_scope(script_state);

  // `kDoNotSanitize` is used because this is only for tests and some tests
  // need `kDoNotSanitize` for dynamic imports.
  ClassicScript* script = ClassicScript::CreateUnspecifiedScript(
      javascript, ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors::kDoNotSanitize);

  const auto policy =
      honor_js_content_settings
          ? ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled
          : ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled;
  ScriptEvaluationResult result =
      script->RunScriptOnScriptStateAndReturnValue(script_state, policy);

  auto* handler = MakeGarbageCollected<JavaScriptExecuteRequestForTestsHandler>(
      std::move(callback));
  v8::Local<v8::Value> error;
  switch (result.GetResultType()) {
    case ScriptEvaluationResult::ResultType::kSuccess: {
      v8::Local<v8::Value> value = result.GetSuccessValue();
      if (resolve_promises && !value.IsEmpty() && value->IsPromise()) {
        auto promise = ScriptPromise<IDLAny>::FromV8Promise(
            script_state->GetIsolate(), value.As<v8::Promise>());
        promise.Then(handler->CreateResolveCallback(script_state, frame_),
                     handler->CreateRejectCallback(script_state, frame_));
      } else {
        handler->SendSuccess(script_state, value);
      }
      return;
    }

    case ScriptEvaluationResult::ResultType::kException:
      error = result.GetExceptionForClassicForTesting();
      break;

    case ScriptEvaluationResult::ResultType::kAborted:
      error = v8::String::NewFromUtf8Literal(isolate, "Script aborted");
      break;

    case ScriptEvaluationResult::ResultType::kNotRun:
      error = v8::String::NewFromUtf8Literal(isolate, "Script not run");
      break;
  }
  DCHECK_NE(result.GetResultType(),
            ScriptEvaluationResult::ResultType::kSuccess);
  handler->SendException(script_state, error);
}

void LocalFrameMojoHandler::JavaScriptExecuteRequestInIsolatedWorld(
    const String& javascript,
    bool wants_result,
    int32_t world_id,
    JavaScriptExecuteRequestInIsolatedWorldCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing",
                       "JavaScriptExecuteRequestInIsolatedWorld",
                       TRACE_EVENT_SCOPE_THREAD);

  if (world_id <= DOMWrapperWorld::kMainWorldId ||
      world_id > DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit) {
    // Returns if the world_id is not valid. world_id is passed as a plain int
    // over IPC and needs to be verified here, in the IPC endpoint.
    std::move(callback).Run(base::Value());
    mojo::ReportBadMessage(kInvalidWorldID);
    return;
  }

  WebScriptSource web_script_source(javascript);
  frame_->RequestExecuteScript(
      world_id, base::span_from_ref(web_script_source),
      mojom::blink::UserActivationOption::kDoNotActivate,
      mojom::blink::EvaluationTiming::kSynchronous,
      mojom::blink::LoadEventBlockingOption::kDoNotBlock,
      WTF::BindOnce(
          [](JavaScriptExecuteRequestInIsolatedWorldCallback callback,
             std::optional<base::Value> value, base::TimeTicks start_time) {
            std::move(callback).Run(value ? std::move(*value) : base::Value());
          },
          std::move(callback)),
      BackForwardCacheAware::kAllow,
      wants_result
          ? mojom::blink::WantResultOption::kWantResultDateAndRegExpAllowed
          : mojom::blink::WantResultOption::kNoResult,
      mojom::blink::PromiseResultOption::kDoNotWait);
}

#if BUILDFLAG(IS_MAC)
void LocalFrameMojoHandler::GetCharacterIndexAtPoint(const gfx::Point& point) {
  frame_->GetCharacterIndexAtPoint(point);
}

void LocalFrameMojoHandler::GetFirstRectForRange(const gfx::Range& range) {
  gfx::Rect rect;
  WebLocalFrameClient* client = WebLocalFrameImpl::FromFrame(frame_)->Client();
  if (!client)
    return;

  WebPluginContainerImpl* plugin_container = frame_->GetWebPluginContainer();
  if (plugin_container) {
    // Pepper-free PDF will reach here.
    rect = plugin_container->Plugin()->GetPluginCaretBounds();
  } else {
    // TODO(crbug.com/40511450): Remove `pepper_has_caret` once PPAPI is gone.
    bool pepper_has_caret = client->GetCaretBoundsFromFocusedPlugin(rect);
    if (!pepper_has_caret) {
      // When request range is invalid we will try to obtain it from current
      // frame selection. The fallback value will be 0.
      size_t start = range.IsValid()
                           ? range.start()
                           : GetCurrentCursorPositionInFrame(frame_);

      WebLocalFrameImpl::FromFrame(frame_)->FirstRectForCharacterRange(
          base::checked_cast<uint32_t>(start),
          base::checked_cast<uint32_t>(range.length()), rect);
    }
  }

  TextInputHost().GotFirstRectForRange(rect);
}

void LocalFrameMojoHandler::GetStringForRange(
    const gfx::Range& range,
    GetStringForRangeCallback callback) {
  gfx::Point baseline_point;
  ui::mojom::blink::AttributedStringPtr attributed_string = nullptr;
  base::apple::ScopedCFTypeRef<CFAttributedStringRef> string =
      SubstringUtil::AttributedSubstringInRange(
          frame_, base::checked_cast<WTF::wtf_size_t>(range.start()),
          base::checked_cast<WTF::wtf_size_t>(range.length()), baseline_point);
  if (string) {
    attributed_string = ui::mojom::blink::AttributedString::From(string.get());
  }

  std::move(callback).Run(std::move(attributed_string), baseline_point);
}
#endif

void LocalFrameMojoHandler::BindReportingObserver(
    mojo::PendingReceiver<mojom::blink::ReportingObserver> receiver) {
  ReportingContext::From(DomWindow())->Bind(std::move(receiver));
}

void LocalFrameMojoHandler::UpdateOpener(
    const std::optional<blink::FrameToken>& opener_frame_token) {
  if (WebFrame::FromCoreFrame(frame_)) {
    Frame* opener_frame = nullptr;
    if (opener_frame_token)
      opener_frame = Frame::ResolveFrame(opener_frame_token.value());
    frame_->SetOpenerDoNotNotify(opener_frame);
  }
}

void LocalFrameMojoHandler::GetSavableResourceLinks(
    GetSavableResourceLinksCallback callback) {
  Vector<KURL> resources_list;
  Vector<mojom::blink::SavableSubframePtr> subframes;
  SavableResources::Result result(&resources_list, &subframes);

  if (!SavableResources::GetSavableResourceLinksForFrame(frame_, &result)) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto referrer = mojom::blink::Referrer::New(GetDocument()->Url(),
                                              DomWindow()->GetReferrerPolicy());

  auto reply = mojom::blink::GetSavableResourceLinksReply::New();
  reply->resources_list = std::move(resources_list);
  reply->referrer = std::move(referrer);
  reply->subframes = std::move(subframes);

  std::move(callback).Run(std::move(reply));
}

void LocalFrameMojoHandler::MixedContentFound(
    const KURL& main_resource_url,
    const KURL& mixed_content_url,
    mojom::blink::RequestContextType request_context,
    bool was_allowed,
    const KURL& url_before_redirects,
    bool had_redirect,
    network::mojom::blink::SourceLocationPtr source_location) {
  std::unique_ptr<SourceLocation> source;
  if (source_location) {
    source = std::make_unique<SourceLocation>(source_location->url, String(),
                                              source_location->line,
                                              source_location->column, nullptr);
  }
  MixedContentChecker::MixedContentFound(
      frame_, main_resource_url, mixed_content_url, request_context,
      was_allowed, url_before_redirects, had_redirect, std::move(source));
}

void LocalFrameMojoHandler::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver) {
  DCHECK(frame_->Client());
  frame_->Client()->BindDevToolsAgent(std::move(host), std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID)
void LocalFrameMojoHandler::ExtractSmartClipData(
    const gfx::Rect& rect,
    ExtractSmartClipDataCallback callback) {
  String clip_text;
  String clip_html;
  gfx::Rect clip_rect;
  frame_->ExtractSmartClipDataInternal(rect, clip_text, clip_html, clip_rect);
  std::move(callback).Run(clip_text.IsNull() ? g_empty_string : clip_text,
                          clip_html.IsNull() ? g_empty_string : clip_html,
                          clip_rect);
}
#endif  // BUILDFLAG(IS_ANDROID)

void LocalFrameMojoHandler::HandleRendererDebugURL(const KURL& url) {
  DCHECK(IsRendererDebugURL(GURL(url)));
  if (url.ProtocolIs("javascript")) {
    // JavaScript URLs should be sent to Blink for handling.
    frame_->LoadJavaScriptURL(url);
  } else {
    // This is a Chrome Debug URL. Handle it.
    HandleChromeDebugURL(GURL(url));
  }

  // The browser sets its status as loading before calling this IPC. Inform it
  // that the load stopped if needed, while leaving the debug URL visible in the
  // address bar.
  if (!frame_->IsLoading())
    frame_->Client()->DidStopLoading();
}

void LocalFrameMojoHandler::GetCanonicalUrlForSharing(
    GetCanonicalUrlForSharingCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  base::TimeTicks start_time = base::TimeTicks::Now();
#endif
  KURL canon_url;
  HTMLLinkElement* link_element = GetDocument()->LinkCanonical();
  if (link_element) {
    canon_url = link_element->Href();
    KURL doc_url = GetDocument()->Url();
    // When sharing links to pages, the fragment identifier often serves to mark a specific place
    // within the page that the user wishes to point the recipient to. Canonical URLs generally
    // don't and can't contain this state, so try to match user expectations a little more closely
    // here by splicing the fragment identifier (if there is one) into the shared URL.
    if (doc_url.HasFragmentIdentifier() && !canon_url.HasFragmentIdentifier()) {
      canon_url.SetFragmentIdentifier(doc_url.FragmentIdentifier().ToString());
    }
  }
  std::move(callback).Run(canon_url.IsNull() ? std::nullopt
                                             : std::make_optional(canon_url));
#if BUILDFLAG(IS_ANDROID)
  base::UmaHistogramMicrosecondsTimes("Blink.Frame.GetCanonicalUrlRendererTime",
                                      base::TimeTicks::Now() - start_time);
#endif
}

void LocalFrameMojoHandler::GetOpenGraphMetadata(
    GetOpenGraphMetadataCallback callback) {
  auto metadata = mojom::blink::OpenGraphMetadata::New();
  if (auto* document_element = frame_->GetDocument()->documentElement()) {
    for (const auto& child :
         Traversal<HTMLMetaElement>::DescendantsOf(*document_element)) {
      // If there are multiple OpenGraph tags for the same property, we always
      // take the value from the first one - this is the specified behavior in
      // the OpenGraph spec:
      //   The first tag (from top to bottom) is given preference during
      //   conflicts
      ParseOpenGraphProperty(child, *frame_->GetDocument(), metadata.get());
    }
  }
  std::move(callback).Run(std::move(metadata));
}

void LocalFrameMojoHandler::SetNavigationApiHistoryEntriesForRestore(
    mojom::blink::NavigationApiHistoryEntryArraysPtr entry_arrays,
    mojom::blink::NavigationApiEntryRestoreReason restore_reason) {
  frame_->DomWindow()->navigation()->SetEntriesForRestore(entry_arrays,
                                                          restore_reason);
}

void LocalFrameMojoHandler::NotifyNavigationApiOfDisposedEntries(
    const WTF::Vector<WTF::String>& keys) {
  frame_->DomWindow()->navigation()->DisposeEntriesForSessionHistoryRemoval(
      keys);
}

void LocalFrameMojoHandler::DispatchNavigateEventForCrossDocumentTraversal(
    const KURL& url,
    const std::string& page_state,
    bool is_browser_initiated) {
  auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
      url, NavigateEventType::kCrossDocument, WebFrameLoadType::kBackForward);
  params->involvement = is_browser_initiated
                            ? UserNavigationInvolvement::kBrowserUI
                            : UserNavigationInvolvement::kNone;
  params->destination_item =
      WebHistoryItem(PageState::CreateFromEncodedData(page_state));
  auto result =
      frame_->DomWindow()->navigation()->DispatchNavigateEvent(params);
  CHECK_EQ(result, NavigationApi::DispatchResult::kContinue);
}

void LocalFrameMojoHandler::TraverseCancelled(
    const String& navigation_api_key,
    mojom::blink::TraverseCancelledReason reason) {
  frame_->DomWindow()->navigation()->TraverseCancelled(navigation_api_key,
                                                       reason);
}

void LocalFrameMojoHandler::AnimateDoubleTapZoom(const gfx::Point& point,
                                                 const gfx::Rect& rect) {
  frame_->GetPage()->GetChromeClient().AnimateDoubleTapZoom(point, rect);
}

void LocalFrameMojoHandler::SetScaleFactor(float scale_factor) {
  frame_->SetScaleFactor(scale_factor);
}

void LocalFrameMojoHandler::ClosePage(
    mojom::blink::LocalMainFrame::ClosePageCallback completion_callback) {
  SECURITY_CHECK(frame_->IsOutermostMainFrame());

  // There are two ways to close a page:
  //
  // 1/ Via webview()->Close() that currently sets the WebView's delegate_ to
  // NULL, and prevent any JavaScript dialogs in the onunload handler from
  // appearing.
  //
  // 2/ Calling the FrameLoader's CloseURL method directly.
  //
  // TODO(creis): Having a single way to close that can run onunload is also
  // useful for fixing http://b/issue?id=753080.

  SubframeLoadingDisabler disabler(frame_->GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // when unloading itself.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      frame_->GetDocument());
  frame_->Loader().DispatchUnloadEventAndFillOldDocumentInfoIfNeeded(
      false /* need_unload_info_for_new_document */);

  std::move(completion_callback).Run();
}

void LocalFrameMojoHandler::GetFullPageSize(
    mojom::blink::LocalMainFrame::GetFullPageSizeCallback callback) {
  // LayoutZoomFactor takes CSS pixels to device/physical pixels. It includes
  // both browser ctrl+/- zoom as well as the device scale factor for screen
  // density. Note: we don't account for pinch-zoom, even though it scales a
  // CSS pixel, since "device pixels" coming from Blink are also unscaled by
  // pinch-zoom.
  float css_to_physical = frame_->LayoutZoomFactor();
  float physical_to_css = 1.f / css_to_physical;
  gfx::Size full_page_size =
      frame_->View()->GetScrollableArea()->ContentsSize();

  // `content_size` is in physical pixels. Normlisation is needed to convert it
  // to CSS pixels. Details: https://crbug.com/1181313
  gfx::Size css_full_page_size =
      gfx::ScaleToFlooredSize(full_page_size, physical_to_css);
  std::move(callback).Run(
      gfx::Size(css_full_page_size.width(), css_full_page_size.height()));
}

void LocalFrameMojoHandler::PluginActionAt(
    const gfx::Point& location,
    mojom::blink::PluginActionType action) {
  // TODO(bokan): Location is probably in viewport coordinates
  HitTestResult result =
      HitTestResultForRootFramePos(frame_, PhysicalOffset(location));
  Node* node = result.InnerNode();
  if (!IsA<HTMLObjectElement>(*node) && !IsA<HTMLEmbedElement>(*node))
    return;

  auto* embedded = DynamicTo<LayoutEmbeddedContent>(node->GetLayoutObject());
  if (!embedded)
    return;

  WebPluginContainerImpl* plugin_view = embedded->Plugin();
  if (!plugin_view)
    return;

  switch (action) {
    case mojom::blink::PluginActionType::kRotate90Clockwise:
      plugin_view->Plugin()->RotateView(WebPlugin::RotationType::k90Clockwise);
      return;
    case mojom::blink::PluginActionType::kRotate90Counterclockwise:
      plugin_view->Plugin()->RotateView(
          WebPlugin::RotationType::k90Counterclockwise);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void LocalFrameMojoHandler::SetInitialFocus(bool reverse) {
  frame_->SetInitialFocus(reverse);
}

void LocalFrameMojoHandler::EnablePreferredSizeChangedMode() {
  frame_->GetPage()->GetChromeClient().EnablePreferredSizeChangedMode();
}

void LocalFrameMojoHandler::ZoomToFindInPageRect(
    const gfx::Rect& rect_in_root_frame) {
  frame_->GetPage()->GetChromeClient().ZoomToFindInPageRect(rect_in_root_frame);
}

void LocalFrameMojoHandler::InstallCoopAccessMonitor(
    const FrameToken& accessed_window,
    network::mojom::blink::CrossOriginOpenerPolicyReporterParamsPtr
        coop_reporter_params,
    bool is_in_same_virtual_coop_related_group) {
  blink::Frame* accessed_frame = Frame::ResolveFrame(accessed_window);
  // The Frame might have been deleted during the cross-process communication.
  if (!accessed_frame)
    return;

  accessed_frame->DomWindow()->InstallCoopAccessMonitor(
      frame_, std::move(coop_reporter_params),
      is_in_same_virtual_coop_related_group);
}

void LocalFrameMojoHandler::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate,
    const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info) {
  DCHECK(frame_->IsOutermostMainFrame());
  TRACE_EVENT2("renderer", "LocalFrame::UpdateBrowserControlsState",
               "Constraint", static_cast<int>(constraints), "Current",
               static_cast<int>(current));
  TRACE_EVENT_INSTANT1("renderer", "is_animated", TRACE_EVENT_SCOPE_THREAD,
                       "animated", animate);

  frame_->GetWidgetForLocalRoot()->UpdateBrowserControlsState(
      constraints, current, animate, offset_tags_info);
}

void LocalFrameMojoHandler::Discard() {
  frame_->Discard();
}

void LocalFrameMojoHandler::SetV8CompileHints(
    base::ReadOnlySharedMemoryRegion data) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kConsumeCompileHints));
  Page* page = GetPage();
  if (page == nullptr) {
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = data.Map();
  if (!mapping.IsValid()) {
    return;
  }
  const int64_t* memory = mapping.GetMemoryAs<int64_t>();
  if (memory == nullptr) {
    return;
  }

  page->GetV8CrowdsourcedCompileHintsConsumer().SetData(memory,
                                                        mapping.size() / 8);
}

void LocalFrameMojoHandler::SnapshotDocumentForViewTransition(
    const blink::ViewTransitionToken& transition_token,
    mojom::blink::PageSwapEventParamsPtr params,
    SnapshotDocumentForViewTransitionCallback callback) {
  ViewTransitionSupplement::SnapshotDocumentForNavigation(
      *frame_->GetDocument(), transition_token, std::move(params),
      std::move(callback));
}

void LocalFrameMojoHandler::NotifyViewTransitionAbortedToOldDocument() {
  if (auto* transition =
          ViewTransitionUtils::GetOutgoingCrossDocumentTransition(
              *frame_->GetDocument())) {
    transition->SkipTransition();
  }
}

void LocalFrameMojoHandler::DispatchPageSwap(
    mojom::blink::PageSwapEventParamsPtr params) {
  auto* page_swap_event = MakeGarbageCollected<PageSwapEvent>(
      *frame_->GetDocument(), std::move(params), nullptr);
  frame_->GetDocument()->domWindow()->DispatchEvent(*page_swap_event);
}

void LocalFrameMojoHandler::AddResourceTimingEntryForFailedSubframeNavigation(
    const FrameToken& subframe_token,
    const KURL& initial_url,
    base::TimeTicks start_time,
    base::TimeTicks redirect_time,
    base::TimeTicks request_start,
    base::TimeTicks response_start,
    uint32_t response_code,
    const WTF::String& mime_type,
    network::mojom::blink::LoadTimingInfoPtr load_timing_info,
    net::HttpConnectionInfo connection_info,
    const WTF::String& alpn_negotiated_protocol,
    bool is_secure_transport,
    bool is_validated,
    const WTF::String& normalized_server_timing,
    const network::URLLoaderCompletionStatus& completion_status) {
  Frame* subframe = Frame::ResolveFrame(subframe_token);
  if (!subframe || !subframe->Owner()) {
    return;
  }

  ResourceResponse response;
  response.SetAlpnNegotiatedProtocol(AtomicString(alpn_negotiated_protocol));
  response.SetConnectionInfo(connection_info);
  response.SetConnectionReused(load_timing_info->socket_reused);
  response.SetTimingAllowPassed(true);
  response.SetIsValidated(is_validated);
  response.SetDecodedBodyLength(completion_status.decoded_body_length);
  response.SetEncodedBodyLength(completion_status.encoded_body_length);
  response.SetEncodedDataLength(completion_status.encoded_data_length);
  response.SetHttpStatusCode(response_code);
  if (!normalized_server_timing.empty()) {
    response.SetHttpHeaderField(http_names::kServerTiming,
                                AtomicString(normalized_server_timing));
  }

  mojom::blink::ResourceTimingInfoPtr info =
      CreateResourceTimingInfo(start_time, initial_url, &response);
  info->response_end = completion_status.completion_time;
  info->last_redirect_end_time = redirect_time;
  info->is_secure_transport = is_secure_transport;
  info->timing = std::move(load_timing_info);
  subframe->Owner()->AddResourceTiming(std::move(info));
}

void LocalFrameMojoHandler::RequestFullscreenVideoElement() {
  // Find the first video element of the frame.
  for (auto* child = frame_->GetDocument()->documentElement(); child;
       child = Traversal<HTMLElement>::Next(*child)) {
    if (IsA<HTMLVideoElement>(child)) {
      // This is always initiated from browser side (which should require the
      // user interacting with ui) which suffices for a user gesture even though
      // there will have been no input to the frame at this point.
      frame_->NotifyUserActivation(
          mojom::blink::UserActivationNotificationType::kInteraction);

      Fullscreen::RequestFullscreen(*child);
      return;
    }
  }
}

void LocalFrameMojoHandler::UpdatePrerenderURL(
    const KURL& matched_url,
    UpdatePrerenderURLCallback callback) {
  CHECK(SecurityOrigin::Create(matched_url)
            ->IsSameOriginWith(
                &*GetDocument()->GetExecutionContext()->GetSecurityOrigin()));
  auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
      matched_url, NavigateEventType::kPrerenderNoVarySearchActivation,
      WebFrameLoadType::kReplaceCurrentItem);
  params->is_browser_initiated = true;

  // TODO(crbug.com/41494389): Add test for how the navigation API can intercept
  // this update.
  if (frame_->DomWindow()->navigation()->DispatchNavigateEvent(params) !=
      NavigationApi::DispatchResult::kContinue) {
    std::move(callback).Run();
    return;
  }

  GetDocument()->Loader()->RunURLAndHistoryUpdateSteps(
      matched_url, nullptr,
      mojom::blink::SameDocumentNavigationType::
          kPrerenderNoVarySearchActivation,
      /*data=*/nullptr, WebFrameLoadType::kReplaceCurrentItem,
      FirePopstate::kNo,
      /*is_browser_initiated=*/true);
  std::move(callback).Run();
}

}  // namespace blink
