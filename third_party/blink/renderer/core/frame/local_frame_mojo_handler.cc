// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_mojo_handler.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom-blink.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/frame_serializer_delegate_impl.h"
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
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"
#include "third_party/blink/renderer/core/html/portal/portal_activate_event.h"
#include "third_party/blink/renderer/core/html/portal/portal_host.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

#if defined(OS_MAC)
#include "third_party/blink/renderer/core/editing/substring_util.h"
#include "third_party/blink/renderer/platform/fonts/mac/attributed_string_type_converter.h"
#include "ui/base/mojom/attributed_string.mojom-blink.h"
#endif

namespace blink {

namespace {

constexpr char kInvalidWorldID[] =
    "JavaScriptExecuteRequestInIsolatedWorld gets an invalid world id.";

#if defined(OS_MAC)
uint32_t GetCurrentCursorPositionInFrame(LocalFrame* local_frame) {
  blink::WebRange range =
      WebLocalFrameImpl::FromFrame(local_frame)->SelectionRange();
  return range.IsNull() ? 0U : static_cast<uint32_t>(range.StartOffset());
}
#endif

RemoteFrame* SourceFrameForOptionalToken(
    const absl::optional<RemoteFrameToken>& source_frame_token) {
  if (!source_frame_token)
    return nullptr;
  return RemoteFrame::FromFrameToken(source_frame_token.value());
}

class WebBundleGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
  STACK_ALLOCATED();

 public:
  WebBundleGenerationDelegate() = default;
  ~WebBundleGenerationDelegate() = default;

  WebBundleGenerationDelegate(const WebBundleGenerationDelegate&) = delete;
  WebBundleGenerationDelegate& operator=(const WebBundleGenerationDelegate&) =
      delete;

  bool ShouldSkipResource(const WebURL& url) override { return false; }
  bool UseBinaryEncoding() override { return false; }
  bool RemovePopupOverlay() override { return false; }
  bool UsePageProblemDetectors() override { return false; }
};

class ResourceSnapshotForWebBundleImpl
    : public data_decoder::mojom::blink::ResourceSnapshotForWebBundle {
 public:
  explicit ResourceSnapshotForWebBundleImpl(Deque<SerializedResource> resources)
      : resources_(std::move(resources)) {}
  ~ResourceSnapshotForWebBundleImpl() override = default;

  ResourceSnapshotForWebBundleImpl(const ResourceSnapshotForWebBundleImpl&) =
      delete;
  ResourceSnapshotForWebBundleImpl& operator=(
      const ResourceSnapshotForWebBundleImpl&) = delete;

  // data_decoder::mojom::blink::ResourceSnapshotForWebBundle:
  void GetResourceCount(GetResourceCountCallback callback) override {
    std::move(callback).Run(resources_.size());
  }
  void GetResourceInfo(uint64_t index,
                       GetResourceInfoCallback callback) override {
    if (index >= resources_.size()) {
      std::move(callback).Run(nullptr);
      return;
    }
    const auto& resource = resources_.at(SafeCast<WTF::wtf_size_t>(index));
    auto info = data_decoder::mojom::blink::SerializedResourceInfo::New();
    info->url = resource.url;
    info->mime_type = resource.mime_type;
    info->size = resource.data ? resource.data->size() : 0;
    std::move(callback).Run(std::move(info));
  }
  void GetResourceBody(uint64_t index,
                       GetResourceBodyCallback callback) override {
    if (index >= resources_.size()) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
    const auto& resource = resources_.at(SafeCast<WTF::wtf_size_t>(index));
    if (!resource.data) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
    std::move(callback).Run(
        mojo_base::BigBuffer(resource.data->CopyAs<std::vector<uint8_t>>()));
  }

 private:
  const Deque<SerializedResource> resources_;
};

v8::Local<v8::Context> MainWorldScriptContext(LocalFrame* local_frame) {
  ScriptState* script_state = ToScriptStateForMainWorld(local_frame);
  DCHECK(script_state);
  return script_state->GetContext();
}

base::Value GetJavaScriptExecutionResult(v8::Local<v8::Value> result,
                                         LocalFrame* local_frame,
                                         WebV8ValueConverter* converter) {
  if (!result.IsEmpty()) {
    v8::Local<v8::Context> context = MainWorldScriptContext(local_frame);
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
                                            base::Value arguments,
                                            WebV8ValueConverter* converter) {
  v8::Local<v8::Context> context = MainWorldScriptContext(local_frame);

  v8::Context::Scope context_scope(context);
  WTF::Vector<v8::Local<v8::Value>> args;
  for (auto const& argument : arguments.GetList()) {
    args.push_back(converter->ToV8Value(&argument, context));
  }

  v8::Local<v8::Value> object;
  v8::Local<v8::Value> method;
  if (!GetProperty(context, context->Global(), object_name).ToLocal(&object) ||
      !GetProperty(context, object, method_name).ToLocal(&method)) {
    return v8::MaybeLocal<v8::Value>();
  }

  return local_frame->DomWindow()
      ->GetScriptController()
      .EvaluateMethodInMainWorld(v8::Local<v8::Function>::Cast(method), object,
                                 static_cast<int>(args.size()), args.data());
}

// A wrapper class used as the callback for JavaScript executed
// in an isolated world.
class JavaScriptIsolatedWorldRequest
    : public GarbageCollected<JavaScriptIsolatedWorldRequest>,
      public WebScriptExecutionCallback {
  using JavaScriptExecuteRequestInIsolatedWorldCallback =
      mojom::blink::LocalFrame::JavaScriptExecuteRequestInIsolatedWorldCallback;

 public:
  JavaScriptIsolatedWorldRequest(
      LocalFrame* local_frame,
      bool wants_result,
      mojom::blink::LocalFrame::JavaScriptExecuteRequestInIsolatedWorldCallback
          callback);
  JavaScriptIsolatedWorldRequest(const JavaScriptIsolatedWorldRequest&) =
      delete;
  JavaScriptIsolatedWorldRequest& operator=(
      const JavaScriptIsolatedWorldRequest&) = delete;
  ~JavaScriptIsolatedWorldRequest() override;

  // WebScriptExecutionCallback:
  void Completed(const WebVector<v8::Local<v8::Value>>& result) override;

  void Trace(Visitor* visitor) const { visitor->Trace(local_frame_); }

 private:
  Member<LocalFrame> local_frame_;
  bool wants_result_;
  JavaScriptExecuteRequestInIsolatedWorldCallback callback_;
};

JavaScriptIsolatedWorldRequest::JavaScriptIsolatedWorldRequest(
    LocalFrame* local_frame,
    bool wants_result,
    JavaScriptExecuteRequestInIsolatedWorldCallback callback)
    : local_frame_(local_frame),
      wants_result_(wants_result),
      callback_(std::move(callback)) {}

JavaScriptIsolatedWorldRequest::~JavaScriptIsolatedWorldRequest() = default;

void JavaScriptIsolatedWorldRequest::Completed(
    const WebVector<v8::Local<v8::Value>>& result) {
  base::Value value;
  if (!result.empty() && !result.begin()->IsEmpty() && wants_result_) {
    // It's safe to always use the main world context when converting
    // here. V8ValueConverter shouldn't actually care about the context
    // scope, and it switches to v8::Object's creation context when
    // encountered. (from extensions/renderer/script_injection.cc)
    v8::Local<v8::Context> context = MainWorldScriptContext(local_frame_);
    v8::Context::Scope context_scope(context);
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();
    converter->SetDateAllowed(true);
    converter->SetRegExpAllowed(true);
    std::unique_ptr<base::Value> new_value =
        converter->FromV8Value(*result.begin(), context);
    if (new_value)
      value = base::Value::FromUniquePtrValue(std::move(new_value));
  }

  std::move(callback_).Run(std::move(value));
}

HitTestResult HitTestResultForRootFramePos(
    LocalFrame* main_frame,
    const PhysicalOffset& pos_in_root_frame) {
  DCHECK(main_frame->IsMainFrame());

  HitTestLocation location(
      main_frame->View()->ConvertFromRootFrame(pos_in_root_frame));
  HitTestResult result = main_frame->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

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
      local_frame_host_remote_.BindNewEndpointAndPassReceiver(
          frame.GetTaskRunner(TaskType::kInternalDefault)));

  auto* registry = frame.GetInterfaceRegistry();
  registry->AddAssociatedInterface(
      WTF::BindRepeating(&LocalFrameMojoHandler::BindToLocalFrameReceiver,
                         WrapWeakPersistent(this)));
  registry->AddInterface(
      WTF::BindRepeating(&LocalFrameMojoHandler::BindToHighPriorityReceiver,
                         WrapWeakPersistent(this)),
      frame.GetTaskRunner(TaskType::kInternalHighPriorityLocalFrame));
  registry->AddAssociatedInterface(WTF::BindRepeating(
      &LocalFrameMojoHandler::BindFullscreenVideoElementReceiver,
      WrapWeakPersistent(this)));
}

void LocalFrameMojoHandler::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(local_frame_host_remote_);
  visitor->Trace(local_frame_receiver_);
  visitor->Trace(main_frame_receiver_);
  visitor->Trace(high_priority_frame_receiver_);
  visitor->Trace(fullscreen_video_receiver_);
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
  high_priority_frame_receiver_.reset();
  // TODO(tkent): Should we reset other receivers?
}

void LocalFrameMojoHandler::ClosePageForTesting() {
  ClosePage(base::DoNothing());
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

void LocalFrameMojoHandler::BindToHighPriorityReceiver(
    mojo::PendingReceiver<mojom::blink::HighPriorityLocalFrame> receiver) {
  if (frame_->IsDetached())
    return;

  high_priority_frame_receiver_.Bind(
      std::move(receiver),
      frame_->GetTaskRunner(TaskType::kInternalHighPriorityLocalFrame));
  high_priority_frame_receiver_.SetFilter(
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
      properties->color_scheme);

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
  // Note that when use-zoom-for-dsf is enabled, WindowToViewportScalar will
  // be the true device scale factor, and PageZoomFactor will be the combination
  // of the device scale factor and the zoom percent of the page.
  blink::LocalFrame& local_frame_root = frame_->LocalFrameRoot();
  const float window_to_viewport_factor =
      page->GetChromeClient().WindowToViewportScalar(&local_frame_root, 1.0f);
  const float zoom_factor = local_frame_root.PageZoomFactor();
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

void LocalFrameMojoHandler::AddInspectorIssue(
    mojom::blink::InspectorIssueInfoPtr info) {
  if (auto* page = GetPage()) {
    page->GetInspectorIssueStorage().AddInspectorIssue(DomWindow(),
                                                       std::move(info));
  }
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
  DCHECK(!frame_->Tree().Parent());
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
  if (HasEditableStyle(*old_focused_element) ||
      old_focused_element->IsTextControl())
    frame_->Selection().Clear();
}

void LocalFrameMojoHandler::GetResourceSnapshotForWebBundle(
    mojo::PendingReceiver<
        data_decoder::mojom::blink::ResourceSnapshotForWebBundle> receiver) {
  Deque<SerializedResource> resources;

  HeapHashSet<WeakMember<const Element>> shadow_template_elements;
  WebBundleGenerationDelegate web_delegate;
  FrameSerializerDelegateImpl core_delegate(web_delegate,
                                            shadow_template_elements);
  FrameSerializer serializer(resources, core_delegate);
  serializer.SerializeFrame(*frame_);

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ResourceSnapshotForWebBundleImpl>(std::move(resources)),
      std::move(receiver));
}

void LocalFrameMojoHandler::CopyImageAt(const gfx::Point& window_point) {
  gfx::Point viewport_position =
      frame_->GetWidgetForLocalRoot()->DIPsToRoundedBlinkSpace(window_point);
  frame_->CopyImageAtViewportPoint(IntPoint(viewport_position));
}

void LocalFrameMojoHandler::SaveImageAt(const gfx::Point& window_point) {
  frame_->SaveImageAt(window_point);
}

void LocalFrameMojoHandler::ReportBlinkFeatureUsage(
    const Vector<mojom::blink::WebFeature>& features) {
  DCHECK(!features.IsEmpty());

  // Assimilate all features used/performed by the browser into UseCounter.
  auto* document = GetDocument();
  DCHECK(document);
  for (const auto& feature : features)
    document->CountUse(feature);
}

void LocalFrameMojoHandler::RenderFallbackContent() {
  frame_->RenderFallbackContent();
}

void LocalFrameMojoHandler::RenderFallbackContentWithResourceTiming(
    mojom::blink::ResourceTimingInfoPtr timing,
    const String& server_timing_value) {
  frame_->RenderFallbackContentWithResourceTiming(std::move(timing),
                                                  server_timing_value);
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
  IntPoint location(viewport_position);

  frame_->MediaPlayerActionAtViewportPoint(location, action->type,
                                           action->enable);
}

void LocalFrameMojoHandler::AdvanceFocusInFrame(
    mojom::blink::FocusType focus_type,
    const absl::optional<RemoteFrameToken>& source_frame_token) {
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

void LocalFrameMojoHandler::AdvanceFocusInForm(
    mojom::blink::FocusType focus_type) {
  auto* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  if (focused_frame != frame_)
    return;

  DCHECK(GetDocument());
  Element* element = GetDocument()->FocusedElement();
  if (!element)
    return;

  Element* next_element =
      GetPage()->GetFocusController().NextFocusableElementInForm(element,
                                                                 focus_type);
  if (!next_element)
    return;

  next_element->scrollIntoViewIfNeeded(true /*centerIfNeeded*/);
  next_element->focus();
}

void LocalFrameMojoHandler::ReportContentSecurityPolicyViolation(
    network::mojom::blink::CSPViolationPtr violation) {
  auto source_location = std::make_unique<SourceLocation>(
      violation->source_location->url, violation->source_location->line,
      violation->source_location->column, nullptr);

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
      ContentSecurityPolicy::ContentSecurityPolicyViolationType::kURLViolation,
      std::move(source_location), context_frame,
      violation->after_redirect ? RedirectStatus::kFollowedRedirect
                                : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

void LocalFrameMojoHandler::DidUpdateFramePolicy(
    const FramePolicy& frame_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  SECURITY_CHECK(IsA<RemoteFrameOwner>(frame_->Owner()));
  To<RemoteFrameOwner>(frame_->Owner())->SetFramePolicy(frame_policy);
}

void LocalFrameMojoHandler::OnScreensChange() {
  if (RuntimeEnabledFeatures::WindowPlacementEnabled(DomWindow())) {
    // Allow fullscreen requests shortly after user-generated screens changes.
    frame_->transient_allow_fullscreen_.Activate();
  }
}

void LocalFrameMojoHandler::PostMessageEvent(
    const absl::optional<RemoteFrameToken>& source_frame_token,
    const String& source_origin,
    const String& target_origin,
    BlinkTransferableMessage message) {
  frame_->PostMessageEvent(source_frame_token, source_origin, target_origin,
                           std::move(message));
}

void LocalFrameMojoHandler::JavaScriptMethodExecuteRequest(
    const String& object_name,
    const String& method_name,
    base::Value arguments,
    bool wants_result,
    JavaScriptMethodExecuteRequestCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptMethodExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();
  converter->SetDateAllowed(true);
  converter->SetRegExpAllowed(true);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result;
  if (!CallMethodOnFrame(frame_, object_name, method_name, std::move(arguments),
                         converter.get())
           .ToLocal(&result)) {
    std::move(callback).Run({});
    return;
  }

  if (wants_result) {
    std::move(callback).Run(
        GetJavaScriptExecutionResult(result, frame_, converter.get()));
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

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result =
      ClassicScript::CreateUnspecifiedScript(javascript)
          ->RunScriptAndReturnValue(DomWindow());

  if (wants_result) {
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();
    converter->SetDateAllowed(true);
    converter->SetRegExpAllowed(true);

    std::move(callback).Run(
        GetJavaScriptExecutionResult(result, frame_, converter.get()));
  } else {
    std::move(callback).Run({});
  }
}

void LocalFrameMojoHandler::JavaScriptExecuteRequestForTests(
    const String& javascript,
    bool wants_result,
    bool has_user_gesture,
    int32_t world_id,
    JavaScriptExecuteRequestForTestsCallback callback) {
  TRACE_EVENT_INSTANT0("test_tracing", "JavaScriptExecuteRequestForTests",
                       TRACE_EVENT_SCOPE_THREAD);

  // A bunch of tests expect to run code in the context of a user gesture, which
  // can grant additional privileges (e.g. the ability to create popups).
  if (has_user_gesture)
    NotifyUserActivation(mojom::blink::UserActivationNotificationType::kTest);

  v8::HandleScope handle_scope(V8PerIsolateData::MainThreadIsolate());
  v8::Local<v8::Value> result;
  if (world_id == DOMWrapperWorld::kMainWorldId) {
    result = ClassicScript::CreateUnspecifiedScript(javascript)
                 ->RunScriptAndReturnValue(DomWindow());
  } else {
    CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
    CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);
    // Note: An error event in an isolated world will never be dispatched to
    // a foreign world.
    result =
        ClassicScript::CreateUnspecifiedScript(
            javascript, SanitizeScriptErrors::kDoNotSanitize)
            ->RunScriptInIsolatedWorldAndReturnValue(DomWindow(), world_id);
  }

  if (wants_result) {
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();
    converter->SetDateAllowed(true);
    converter->SetRegExpAllowed(true);

    std::move(callback).Run(
        GetJavaScriptExecutionResult(result, frame_, converter.get()));
  } else {
    std::move(callback).Run({});
  }
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

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  scoped_refptr<DOMWrapperWorld> isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(ToIsolate(frame_), world_id);
  ScriptSourceCode source_code = ScriptSourceCode(javascript);
  HeapVector<ScriptSourceCode> sources;
  sources.Append(&source_code, 1);
  auto* executor = MakeGarbageCollected<PausableScriptExecutor>(
      DomWindow(), std::move(isolated_world), sources, false /* user_gesture */,
      MakeGarbageCollected<JavaScriptIsolatedWorldRequest>(
          frame_, wants_result, std::move(callback)));
  executor->Run();
}

#if defined(OS_MAC)
void LocalFrameMojoHandler::GetCharacterIndexAtPoint(const gfx::Point& point) {
  frame_->GetCharacterIndexAtPoint(point);
}

void LocalFrameMojoHandler::GetFirstRectForRange(const gfx::Range& range) {
  gfx::Rect rect;
  WebLocalFrameClient* client = WebLocalFrameImpl::FromFrame(frame_)->Client();
  if (!client)
    return;

  if (!client->GetCaretBoundsFromFocusedPlugin(rect)) {
    // When request range is invalid we will try to obtain it from current
    // frame selection. The fallback value will be 0.
    uint32_t start = range.IsValid() ? range.start()
                                     : GetCurrentCursorPositionInFrame(frame_);

    WebLocalFrameImpl::FromFrame(frame_)->FirstRectForCharacterRange(
        start, range.length(), rect);
  }

  frame_->GetTextInputHost().GotFirstRectForRange(rect);
}

void LocalFrameMojoHandler::GetStringForRange(
    const gfx::Range& range,
    GetStringForRangeCallback callback) {
  gfx::Point baseline_point;
  ui::mojom::blink::AttributedStringPtr attributed_string = nullptr;
  NSAttributedString* string = SubstringUtil::AttributedSubstringInRange(
      frame_, range.start(), range.length(), &baseline_point);
  if (string)
    attributed_string = ui::mojom::blink::AttributedString::From(string);

  std::move(callback).Run(std::move(attributed_string), baseline_point);
}
#endif

void LocalFrameMojoHandler::BindReportingObserver(
    mojo::PendingReceiver<mojom::blink::ReportingObserver> receiver) {
  ReportingContext::From(DomWindow())->Bind(std::move(receiver));
}

void LocalFrameMojoHandler::UpdateOpener(
    const absl::optional<blink::FrameToken>& opener_frame_token) {
  if (auto* web_frame = WebFrame::FromCoreFrame(frame_)) {
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
    source = std::make_unique<SourceLocation>(source_location->url,
                                              source_location->line,
                                              source_location->column, nullptr);
  }
  MixedContentChecker::MixedContentFound(
      frame_, main_resource_url, mixed_content_url, request_context,
      was_allowed, url_before_redirects, had_redirect, std::move(source));
}

void LocalFrameMojoHandler::ActivateForPrerendering(
    base::TimeTicks activation_start) {
  DCHECK(features::IsPrerender2Enabled());

  // https://jeremyroman.github.io/alternate-loading-modes/#prerendering-browsing-context-activate
  // Step 8.2. "Let inclusiveDescendants be successorBC extended with
  // successorBC's active document's list of the descendant browsing contexts."
  // Step 8.3. "For each bc of inclusiveDescendants, queue a global task on the
  // networking task source, given bc's active window, to perform the following
  // steps:"
  frame_->GetTaskRunner(TaskType::kNetworking)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&Document::ActivateForPrerendering,
                           WrapPersistent(GetDocument()), activation_start));
}

void LocalFrameMojoHandler::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver) {
  DCHECK(frame_->Client());
  frame_->Client()->BindDevToolsAgent(std::move(host), std::move(receiver));
}

#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)

void LocalFrameMojoHandler::HandleRendererDebugURL(const KURL& url) {
  DCHECK(IsRendererDebugURL(url));
  if (url.ProtocolIs("javascript")) {
    // JavaScript URLs should be sent to Blink for handling.
    frame_->LoadJavaScriptURL(url);
  } else {
    // This is a Chrome Debug URL. Handle it.
    HandleChromeDebugURL(url);
  }

  // The browser sets its status as loading before calling this IPC. Inform it
  // that the load stopped if needed, while leaving the debug URL visible in the
  // address bar.
  if (!frame_->IsLoading())
    frame_->Client()->DidStopLoading();
}

void LocalFrameMojoHandler::GetCanonicalUrlForSharing(
    GetCanonicalUrlForSharingCallback callback) {
  KURL canonical_url;
  HTMLLinkElement* link_element = GetDocument()->LinkCanonical();
  if (link_element)
    canonical_url = link_element->Href();
  std::move(callback).Run(canonical_url.IsNull()
                              ? absl::nullopt
                              : absl::make_optional(canonical_url));
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
  SECURITY_CHECK(frame_->IsMainFrame());

  // TODO(crbug.com/1161996): Remove this VLOG once the investigation is done.
  VLOG(1) << "LocalFrame::ClosePage() URL = " << frame_->GetDocument()->Url();

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
  frame_->Loader().DispatchUnloadEvent(nullptr, nullptr);

  std::move(completion_callback).Run();
}

void LocalFrameMojoHandler::PluginActionAt(
    const gfx::Point& location,
    mojom::blink::PluginActionType action) {
  SECURITY_CHECK(frame_->IsMainFrame());

  // TODO(bokan): Location is probably in viewport coordinates
  HitTestResult result =
      HitTestResultForRootFramePos(frame_, PhysicalOffset(IntPoint(location)));
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
  NOTREACHED();
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
    network::mojom::blink::CoopAccessReportType report_type,
    const FrameToken& accessed_window,
    mojo::PendingRemote<network::mojom::blink::CrossOriginOpenerPolicyReporter>
        reporter,
    bool endpoint_defined,
    const WTF::String& reported_window_url) {
  blink::Frame* accessed_frame = Frame::ResolveFrame(accessed_window);
  // The Frame might have been deleted during the cross-process communication.
  if (!accessed_frame)
    return;

  accessed_frame->DomWindow()->InstallCoopAccessMonitor(
      report_type, frame_, std::move(reporter), endpoint_defined,
      std::move(reported_window_url));
}

void LocalFrameMojoHandler::OnPortalActivated(
    const PortalToken& portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient> portal_client,
    BlinkTransferableMessage data,
    uint64_t trace_id,
    OnPortalActivatedCallback callback) {
  DCHECK(frame_->GetDocument());
  LocalDOMWindow* dom_window = frame_->DomWindow();
  PaintTiming::From(*frame_->GetDocument()).OnPortalActivate();

  TRACE_EVENT_WITH_FLOW0("navigation", "LocalFrame::OnPortalActivated",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);

  DOMWindowPortalHost::portalHost(*dom_window)->OnPortalActivated();
  frame_->GetPage()->SetInsidePortal(false);

  DCHECK(!data.locked_agent_cluster_id)
      << "portal activation is always cross-agent-cluster and should be "
         "diagnosed early";
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*dom_window, std::move(data.ports));

  PortalActivateEvent* event = PortalActivateEvent::Create(
      frame_, portal_token, std::move(portal), std::move(portal_client),
      std::move(data.message), ports, std::move(callback));

  ThreadDebugger* debugger = MainThreadDebugger::Instance();
  if (debugger)
    debugger->ExternalAsyncTaskStarted(data.sender_stack_trace_id);
  dom_window->DispatchEvent(*event);
  if (debugger)
    debugger->ExternalAsyncTaskFinished(data.sender_stack_trace_id);
  event->ExpireAdoptionLifetime();
}

void LocalFrameMojoHandler::ForwardMessageFromHost(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& source_origin) {
  PortalHost::From(*frame_->DomWindow())
      .ReceiveMessage(std::move(message), source_origin);
}

void LocalFrameMojoHandler::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  DCHECK(frame_->IsMainFrame());
  TRACE_EVENT2("renderer", "LocalFrame::UpdateBrowserControlsState",
               "Constraint", static_cast<int>(constraints), "Current",
               static_cast<int>(current));
  TRACE_EVENT_INSTANT1("renderer", "is_animated", TRACE_EVENT_SCOPE_THREAD,
                       "animated", animate);

  frame_->GetWidgetForLocalRoot()->UpdateBrowserControlsState(constraints,
                                                              current, animate);
}

void LocalFrameMojoHandler::DispatchBeforeUnload(
    bool is_reload,
    mojom::blink::LocalFrame::BeforeUnloadCallback callback) {
  BeforeUnload(is_reload, std::move(callback));
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

}  // namespace blink
