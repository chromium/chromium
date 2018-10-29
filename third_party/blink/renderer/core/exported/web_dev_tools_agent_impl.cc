/*
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"

#include <v8-inspector.h>
#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_io_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_memory_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/inspector_testing_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_worker_agent.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

bool IsMainFrame(WebLocalFrameImpl* frame) {
  // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even
  // though |frame| is meant to be main frame.  See http://crbug.com/526162.
  return frame->ViewImpl() && !frame->Parent();
}

}  // namespace

class ClientMessageLoopAdapter : public MainThreadDebugger::ClientMessageLoop {
 public:
  ~ClientMessageLoopAdapter() override { instance_ = nullptr; }

  static void EnsureMainThreadDebuggerCreated() {
    if (instance_)
      return;
    std::unique_ptr<ClientMessageLoopAdapter> instance(
        new ClientMessageLoopAdapter(
            Platform::Current()->CreateNestedMessageLoopRunner()));
    instance_ = instance.get();
    MainThreadDebugger::Instance()->SetClientMessageLoop(std::move(instance));
  }

  static void ContinueProgram() {
    // Release render thread if necessary.
    if (instance_)
      instance_->QuitNow();
  }

 private:
  ClientMessageLoopAdapter(
      std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop)
      : running_for_debug_break_(false),
        message_loop_(std::move(message_loop)) {
    DCHECK(message_loop_.get());
  }

  void Run(LocalFrame* frame) override {
    if (running_for_debug_break_)
      return;

    running_for_debug_break_ = true;
    RunLoop(WebLocalFrameImpl::FromFrame(frame));
  }

  void RunLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent = frame->DevToolsAgentImpl();
    agent->FlushProtocolNotifications();

    // 1. Disable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(true);
    for (auto* const view : WebViewImpl::AllInstances())
      view->GetChromeClient().NotifyPopupOpeningObservers();

    // 2. Disable active objects
    WebView::WillEnterModalLoop();

    // 3. Process messages until quitNow is called.
    message_loop_->Run();
  }

  void QuitNow() override {
    if (running_for_debug_break_) {
      running_for_debug_break_ = false;
      // Undo steps (3), (2) and (1) from above.
      // NOTE: This code used to be above right after the |mesasge_loop_->Run()|
      // code, but it is moved here to support browser-side navigation.
      message_loop_->QuitNow();
      WebView::DidExitModalLoop();
      WebFrameWidgetBase::SetIgnoreInputEvents(false);
    }
  }

  void RunIfWaitingForDebugger(LocalFrame* frame) override {
    WebDevToolsAgentImpl* agent =
        WebLocalFrameImpl::FromFrame(frame)->DevToolsAgentImpl();
    if (agent && agent->worker_client_)
      agent->worker_client_->ResumeStartup();
  }

  bool running_for_debug_break_;
  std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop_;

  static ClientMessageLoopAdapter* instance_;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::instance_ = nullptr;

InspectorSession* WebDevToolsAgentImpl::AttachSession(
    InspectorSession::Client* session_client,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state) {
  if (!sessions_.size())
    Platform::Current()->CurrentThread()->AddTaskObserver(this);

  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated();
  MainThreadDebugger* main_thread_debugger = MainThreadDebugger::Instance();
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  InspectedFrames* inspected_frames = inspected_frames_.Get();

  bool should_reattach = !reattach_session_state.is_null();

  InspectorSession* inspector_session = new InspectorSession(
      session_client, probe_sink_.Get(), inspected_frames, 0,
      main_thread_debugger->GetV8Inspector(),
      main_thread_debugger->ContextGroupId(inspected_frames->Root()),
      std::move(reattach_session_state));

  InspectorDOMAgent* dom_agent = new InspectorDOMAgent(
      isolate, inspected_frames, inspector_session->V8Session());
  inspector_session->Append(dom_agent);

  InspectorLayerTreeAgent* layer_tree_agent =
      InspectorLayerTreeAgent::Create(inspected_frames, this);
  inspector_session->Append(layer_tree_agent);

  InspectorNetworkAgent* network_agent = new InspectorNetworkAgent(
      inspected_frames, nullptr, inspector_session->V8Session());
  inspector_session->Append(network_agent);

  InspectorCSSAgent* css_agent = InspectorCSSAgent::Create(
      dom_agent, inspected_frames, network_agent,
      resource_content_loader_.Get(), resource_container_.Get());
  inspector_session->Append(css_agent);

  InspectorDOMDebuggerAgent* dom_debugger_agent = new InspectorDOMDebuggerAgent(
      isolate, dom_agent, inspector_session->V8Session());
  inspector_session->Append(dom_debugger_agent);

  inspector_session->Append(
      InspectorDOMSnapshotAgent::Create(inspected_frames, dom_debugger_agent));

  inspector_session->Append(new InspectorAnimationAgent(
      inspected_frames, css_agent, inspector_session->V8Session()));

  inspector_session->Append(InspectorMemoryAgent::Create(inspected_frames));

  inspector_session->Append(
      InspectorPerformanceAgent::Create(inspected_frames));

  inspector_session->Append(
      InspectorApplicationCacheAgent::Create(inspected_frames));

  inspector_session->Append(
      new InspectorWorkerAgent(inspected_frames, nullptr));

  InspectorPageAgent* page_agent = InspectorPageAgent::Create(
      inspected_frames, this, resource_content_loader_.Get(),
      inspector_session->V8Session());
  inspector_session->Append(page_agent);

  inspector_session->Append(new InspectorLogAgent(
      &inspected_frames->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames->Root()->GetPerformanceMonitor(),
      inspector_session->V8Session()));

  InspectorOverlayAgent* overlay_agent =
      new InspectorOverlayAgent(web_local_frame_impl_.Get(), inspected_frames,
                                inspector_session->V8Session(), dom_agent);
  inspector_session->Append(overlay_agent);

  inspector_session->Append(
      new InspectorIOAgent(isolate, inspector_session->V8Session()));

  inspector_session->Append(new InspectorAuditsAgent(network_agent));

  // TODO(dgozman): we should actually pass the view instead of frame, but
  // during remote->local transition we cannot access mainFrameImpl() yet, so
  // we have to store the frame which will become the main frame later.
  inspector_session->Append(
      new InspectorEmulationAgent(web_local_frame_impl_.Get()));

  inspector_session->Append(new InspectorTestingAgent(inspected_frames));

  // Call session init callbacks registered from higher layers
  CoreInitializer::GetInstance().InitInspectorAgentSession(
      inspector_session, include_view_agents_, dom_agent, inspected_frames,
      web_local_frame_impl_->ViewImpl()->GetPage());

  if (should_reattach) {
    inspector_session->Restore();
    if (worker_client_)
      worker_client_->ResumeStartup();
  }

  if (node_to_inspect_) {
    overlay_agent->Inspect(node_to_inspect_);
    node_to_inspect_ = nullptr;
  }

  sessions_.insert(inspector_session);
  network_agents_.insert(inspector_session, network_agent);
  page_agents_.insert(inspector_session, page_agent);
  overlay_agents_.insert(inspector_session, overlay_agent);
  return inspector_session;
}

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForFrame(
    WebLocalFrameImpl* frame) {
  return new WebDevToolsAgentImpl(frame, IsMainFrame(frame), nullptr);
}

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForWorker(
    WebLocalFrameImpl* frame,
    WorkerClient* worker_client) {
  return new WebDevToolsAgentImpl(frame, true, worker_client);
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* web_local_frame_impl,
    bool include_view_agents,
    WorkerClient* worker_client)
    : worker_client_(worker_client),
      web_local_frame_impl_(web_local_frame_impl),
      probe_sink_(web_local_frame_impl_->GetFrame()->GetProbeSink()),
      resource_content_loader_(InspectorResourceContentLoader::Create(
          web_local_frame_impl_->GetFrame())),
      inspected_frames_(new InspectedFrames(web_local_frame_impl_->GetFrame())),
      resource_container_(new InspectorResourceContainer(inspected_frames_)),
      include_view_agents_(include_view_agents) {
  DCHECK(IsMainThread());
  agent_ = new DevToolsAgent(
      this, web_local_frame_impl_->GetFrame()->GetInspectorTaskRunner(),
      Platform::Current()->GetIOTaskRunner());
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DCHECK(!worker_client_);
}

void WebDevToolsAgentImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(sessions_);
  visitor->Trace(network_agents_);
  visitor->Trace(page_agents_);
  visitor->Trace(overlay_agents_);
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(probe_sink_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(resource_container_);
  visitor->Trace(node_to_inspect_);
}

void WebDevToolsAgentImpl::WillBeDestroyed() {
  DCHECK(web_local_frame_impl_->GetFrame());
  DCHECK(inspected_frames_->Root()->View());
  agent_->Dispose();
  resource_content_loader_->Dispose();
  worker_client_ = nullptr;
}

void WebDevToolsAgentImpl::BindRequest(
    mojom::blink::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
    mojom::blink::DevToolsAgentAssociatedRequest request) {
  agent_->BindRequest(
      std::move(host_ptr_info), std::move(request),
      web_local_frame_impl_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void WebDevToolsAgentImpl::DetachSession(InspectorSession* session) {
  network_agents_.erase(session);
  page_agents_.erase(session);
  overlay_agents_.erase(session);
  sessions_.erase(session);
  if (!sessions_.size())
    Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
}

void WebDevToolsAgentImpl::InspectElement(const WebPoint& point_in_local_root) {
  WebPoint point = point_in_local_root;
  // TODO(dgozman): the ViewImpl() check must not be necessary,
  // but it is required when attaching early to a provisional frame.
  // We should clean this up once provisional frames are gone.
  // See https://crbug.com/578349.
  if (web_local_frame_impl_->ViewImpl() &&
      web_local_frame_impl_->ViewImpl()->Client()) {
    WebFloatRect rect(point.x, point.y, 0, 0);
    web_local_frame_impl_->ViewImpl()->WidgetClient()->ConvertWindowToViewport(
        &rect);
    point = WebPoint(rect.x, rect.y);
  }

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  HitTestRequest request(hit_type);
  WebMouseEvent dummy_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WTF::CurrentTimeTicks());
  dummy_event.SetPositionInWidget(point.x, point.y);
  IntPoint transformed_point = FlooredIntPoint(
      TransformWebMouseEvent(web_local_frame_impl_->GetFrameView(), dummy_event)
          .PositionInRootFrame());
  HitTestLocation location(
      web_local_frame_impl_->GetFrameView()->ConvertFromRootFrame(
          transformed_point));
  HitTestResult result(request, location);
  web_local_frame_impl_->GetFrame()->ContentLayoutObject()->HitTest(location,
                                                                    result);
  Node* node = result.InnerNode();
  if (!node && web_local_frame_impl_->GetFrame()->GetDocument())
    node = web_local_frame_impl_->GetFrame()->GetDocument()->documentElement();

  if (!overlay_agents_.IsEmpty()) {
    for (auto& it : overlay_agents_)
      it.value->Inspect(node);
  } else {
    node_to_inspect_ = node;
  }
}

void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  resource_container_->DidCommitLoadForLocalFrame(frame);
  resource_content_loader_->DidCommitLoadForLocalFrame(frame);
  for (auto& session : sessions_)
    session->DidCommitLoadForLocalFrame(frame);
}

bool WebDevToolsAgentImpl::ScreencastEnabled() {
  for (auto& it : page_agents_) {
    if (it.value->ScreencastEnabled())
      return true;
  }
  return false;
}

void WebDevToolsAgentImpl::PageLayoutInvalidated(bool resized) {
  for (auto& it : overlay_agents_)
    it.value->PageLayoutInvalidated(resized);
}

bool WebDevToolsAgentImpl::IsInspectorLayer(GraphicsLayer* layer) {
  for (auto& it : overlay_agents_) {
    if (it.value->IsInspectorLayer(layer))
      return true;
  }
  return false;
}

String WebDevToolsAgentImpl::EvaluateInOverlayForTesting(const String& script) {
  String result;
  for (auto& it : overlay_agents_)
    result = it.value->EvaluateInOverlayForTest(script);
  return result;
}

void WebDevToolsAgentImpl::UpdateOverlays() {
  for (auto& it : overlay_agents_)
    it.value->UpdateAllOverlayLifecyclePhases();
}

void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {
  for (auto& it : overlay_agents_)
    it.value->DispatchBufferedTouchEvents();
}

bool WebDevToolsAgentImpl::HandleInputEvent(const WebInputEvent& event) {
  for (auto& it : overlay_agents_) {
    if (it.value->HandleInputEvent(event))
      return true;
  }
  return false;
}

String WebDevToolsAgentImpl::NavigationInitiatorInfo(LocalFrame* frame) {
  for (auto& it : network_agents_) {
    String initiator = it.value->NavigationInitiatorInfo(frame);
    if (!initiator.IsNull())
      return initiator;
  }
  return String();
}

void WebDevToolsAgentImpl::FlushProtocolNotifications() {
  agent_->FlushProtocolNotifications();
}

void WebDevToolsAgentImpl::WillProcessTask(
    const base::PendingTask& pending_task) {
  if (sessions_.IsEmpty())
    return;
  ThreadDebugger::IdleFinished(V8PerIsolateData::MainThreadIsolate());
}

void WebDevToolsAgentImpl::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (sessions_.IsEmpty())
    return;
  ThreadDebugger::IdleStarted(V8PerIsolateData::MainThreadIsolate());
  FlushProtocolNotifications();
}

}  // namespace blink
