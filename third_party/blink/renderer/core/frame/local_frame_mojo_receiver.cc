// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_mojo_receiver.h"

#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"
#include "third_party/blink/renderer/core/html/portal/portal_activate_event.h"
#include "third_party/blink/renderer/core/html/portal/portal_host.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

namespace {

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

LocalFrameMojoReceiver::LocalFrameMojoReceiver(LocalFrame& frame)
    : frame_(frame) {
  auto* registry = frame.GetInterfaceRegistry();
  registry->AddInterface(
      WTF::BindRepeating(&LocalFrameMojoReceiver::BindToHighPriorityReceiver,
                         WrapWeakPersistent(this)),
      frame.GetTaskRunner(TaskType::kInternalHighPriorityLocalFrame));
  registry->AddAssociatedInterface(WTF::BindRepeating(
      &LocalFrameMojoReceiver::BindFullscreenVideoElementReceiver,
      WrapWeakPersistent(this)));
}

void LocalFrameMojoReceiver::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(main_frame_receiver_);
  visitor->Trace(high_priority_frame_receiver_);
  visitor->Trace(fullscreen_video_receiver_);
}

void LocalFrameMojoReceiver::WasAttachedAsLocalMainFrame() {
  frame_->GetInterfaceRegistry()->AddAssociatedInterface(
      WTF::BindRepeating(&LocalFrameMojoReceiver::BindToMainFrameReceiver,
                         WrapWeakPersistent(this)));
}

void LocalFrameMojoReceiver::DidDetachFrame() {
  // We reset receivers explicitly because HeapMojoReceiver does not
  // automatically reset on context destruction.
  main_frame_receiver_.reset();
  high_priority_frame_receiver_.reset();
  // TODO(tkent): Should we reset other receivers?
}

void LocalFrameMojoReceiver::ClosePageForTesting() {
  ClosePage(base::DoNothing());
}

void LocalFrameMojoReceiver::BindToMainFrameReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrame> receiver) {
  if (frame_->IsDetached())
    return;

  main_frame_receiver_.Bind(std::move(receiver),
                            frame_->GetTaskRunner(TaskType::kInternalDefault));
  main_frame_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoReceiver::BindToHighPriorityReceiver(
    mojo::PendingReceiver<mojom::blink::HighPriorityLocalFrame> receiver) {
  if (frame_->IsDetached())
    return;

  high_priority_frame_receiver_.Bind(
      std::move(receiver),
      frame_->GetTaskRunner(TaskType::kInternalHighPriorityLocalFrame));
  high_priority_frame_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoReceiver::BindFullscreenVideoElementReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::FullscreenVideoElementHandler>
        receiver) {
  if (frame_->IsDetached())
    return;

  fullscreen_video_receiver_.Bind(
      std::move(receiver), frame_->GetTaskRunner(TaskType::kInternalDefault));
  fullscreen_video_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(frame_));
}

void LocalFrameMojoReceiver::AnimateDoubleTapZoom(const gfx::Point& point,
                                                  const gfx::Rect& rect) {
  frame_->GetPage()->GetChromeClient().AnimateDoubleTapZoom(point, rect);
}

void LocalFrameMojoReceiver::SetScaleFactor(float scale_factor) {
  frame_->SetScaleFactor(scale_factor);
}

void LocalFrameMojoReceiver::ClosePage(
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

void LocalFrameMojoReceiver::PluginActionAt(
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

void LocalFrameMojoReceiver::SetInitialFocus(bool reverse) {
  frame_->SetInitialFocus(reverse);
}

void LocalFrameMojoReceiver::EnablePreferredSizeChangedMode() {
  frame_->GetPage()->GetChromeClient().EnablePreferredSizeChangedMode();
}

void LocalFrameMojoReceiver::ZoomToFindInPageRect(
    const gfx::Rect& rect_in_root_frame) {
  frame_->GetPage()->GetChromeClient().ZoomToFindInPageRect(rect_in_root_frame);
}

void LocalFrameMojoReceiver::InstallCoopAccessMonitor(
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

void LocalFrameMojoReceiver::OnPortalActivated(
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

void LocalFrameMojoReceiver::ForwardMessageFromHost(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& source_origin) {
  PortalHost::From(*frame_->DomWindow())
      .ReceiveMessage(std::move(message), source_origin);
}

void LocalFrameMojoReceiver::UpdateBrowserControlsState(
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

void LocalFrameMojoReceiver::DispatchBeforeUnload(
    bool is_reload,
    mojom::blink::LocalFrame::BeforeUnloadCallback callback) {
  frame_->BeforeUnload(is_reload, std::move(callback));
}

void LocalFrameMojoReceiver::RequestFullscreenVideoElement() {
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
