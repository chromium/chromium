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

#include "base/auto_reset.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"
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
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_io_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_memory_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_timeline_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
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
  ~ClientMessageLoopAdapter() override {
    DCHECK(running_for_debug_break_kind_ != kInstrumentationPause);
    instance_ = nullptr;
  }

  static void EnsureMainThreadDebuggerCreated(v8::Isolate* isolate) {
    if (instance_)
      return;
    std::unique_ptr<ClientMessageLoopAdapter> instance(
        new ClientMessageLoopAdapter(
            Platform::Current()->CreateNestedMessageLoopRunner()));
    instance_ = instance.get();
    MainThreadDebugger::Instance(isolate)->SetClientMessageLoop(
        std::move(instance));
  }

  static void ActivatePausedDebuggerWindow(WebLocalFrameImpl* frame) {
    if (!instance_ || !instance_->paused_frame_ ||
        instance_->paused_frame_ == frame) {
      return;
    }
    if (!base::FeatureList::IsEnabled(
            features::kShowHudDisplayForPausedPages)) {
      return;
    }
    instance_->paused_frame_->DevToolsAgentImpl(/*create_if_necessary=*/true)
        ->GetDevToolsAgent()
        ->BringDevToolsWindowToFocus();
  }

  static void ContinueProgram() {
    // Release render thread if necessary.
    if (instance_)
      instance_->QuitNow();
  }

  static void PauseForPageWait(WebLocalFrameImpl* frame) {
    if (instance_)
      instance_->RunForPageWait(frame);
  }

 private:
  // A RAII class that disables input events for frames that belong to the
  // same browsing context group. Note that this does not support nesting, as
  // DevTools doesn't require nested pauses.
  class ScopedInputEventsDisabler {
   public:
    explicit ScopedInputEventsDisabler(WebLocalFrameImpl& frame)
        : browsing_context_group_token_(WebFrame::ToCoreFrame(frame)
                                            ->GetPage()
                                            ->BrowsingContextGroupToken()) {
      WebFrameWidgetImpl::SetIgnoreInputEvents(browsing_context_group_token_,
                                               true);
    }

    ~ScopedInputEventsDisabler() {
      WebFrameWidgetImpl::SetIgnoreInputEvents(browsing_context_group_token_,
                                               false);
    }

   private:
    const base::UnguessableToken browsing_context_group_token_;
  };

  explicit ClientMessageLoopAdapter(
      std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop)
      : message_loop_(std::move(message_loop)) {
    DCHECK(message_loop_.get());
  }

  void Run(LocalFrame* frame, MessageLoopKind message_loop_kind) override {
    if (running_for_debug_break_kind_) {
      return;
    }

    running_for_debug_break_kind_ = message_loop_kind;
    if (!running_for_page_wait_) {
      switch (message_loop_kind) {
        case kNormalPause:
          RunLoop(WebLocalFrameImpl::FromFrame(frame));
          break;
        case kInstrumentationPause:
          RunInstrumentationPauseLoop(WebLocalFrameImpl::FromFrame(frame));
          break;
      }
    }
  }

  void RunForPageWait(WebLocalFrameImpl* frame) {
    if (running_for_page_wait_)
      return;

    running_for_page_wait_ = true;
    if (!running_for_debug_break_kind_) {
      RunLoop(frame);
    } else {
      // We should not start waiting for the debugger during instrumentation
      // pauses, so the current pause must be a normal pause.
      DCHECK_EQ(*running_for_debug_break_kind_, kNormalPause);
    }
  }

  void QuitNow() override {
    if (!running_for_debug_break_kind_) {
      return;
    }

    if (!running_for_page_wait_) {
      switch (*running_for_debug_break_kind_) {
        case kNormalPause:
          DoQuitNormalPause();
          break;
        case kInstrumentationPause:
          DoQuitInstrumentationPause();
          break;
      }
    }
    running_for_debug_break_kind_.reset();
  }

  void RunInstrumentationPauseLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent =
        frame->DevToolsAgentImpl(/*create_if_necessary=*/true);
    agent->FlushProtocolNotifications();

    // 1. Run the instrumentation message loop. Also remember the task runner
    // so that we can later quit the loop.
    DCHECK(!inspector_task_runner_for_instrumentation_pause_);
    inspector_task_runner_for_instrumentation_pause_ =
        frame->GetFrame()->GetInspectorTaskRunner();
    inspector_task_runner_for_instrumentation_pause_
        ->ProcessInterruptingTasks();
  }

  void DoQuitInstrumentationPause() {
    DCHECK(inspector_task_runner_for_instrumentation_pause_);
    inspector_task_runner_for_instrumentation_pause_
        ->RequestQuitProcessingInterruptingTasks();
    inspector_task_runner_for_instrumentation_pause_.reset();
  }

  void RunLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent =
        frame->DevToolsAgentImpl(/*create_if_necessary=*/true);
    agent->FlushProtocolNotifications();
    agent->MainThreadDebuggerPaused();
    CHECK(!paused_frame_);
    paused_frame_ = WrapWeakPersistent(frame);

    // 1. Disable input events.
    CHECK(!input_events_disabler_);
    input_events_disabler_ =
        std::make_unique<ScopedInputEventsDisabler>(*frame);
    for (auto* const view : WebViewImpl::AllInstances())
      view->GetChromeClient().NotifyPopupOpeningObservers();

    // 2. Disable active objects
    page_pauser_ = std::make_unique<WebScopedPagePauser>(*frame);

    // 3. Process messages until quitNow is called.
    message_loop_->Run();
  }

  void RunIfWaitingForDebugger(LocalFrame* frame) override {
    if (!running_for_page_wait_)
      return;
    if (!running_for_debug_break_kind_) {
      DoQuitNormalPause();
    }
    running_for_page_wait_ = false;
  }

  void DoQuitNormalPause() {
    // Undo steps (3), (2) and (1) from above.
    // NOTE: This code used to be above right after the |mesasge_loop_->Run()|
    // code, but it is moved here to support browser-side navigation.
    DCHECK(running_for_page_wait_ ||
           *running_for_debug_break_kind_ == kNormalPause);
    message_loop_->QuitNow();
    page_pauser_.reset();
    input_events_disabler_.reset();

    CHECK(paused_frame_);
    if (paused_frame_->GetFrame()) {
      paused_frame_->DevToolsAgentImpl(/*create_if_necessary=*/true)
          ->MainThreadDebuggerResumed();
    }
    paused_frame_ = nullptr;
  }

  std::optional<MessageLoopKind> running_for_debug_break_kind_;
  bool running_for_page_wait_ = false;
  std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop_;
  std::unique_ptr<ScopedInputEventsDisabler> input_events_disabler_;
  std::unique_ptr<WebScopedPagePauser> page_pauser_;
  WeakPersistent<WebLocalFrameImpl> paused_frame_;
  scoped_refptr<InspectorTaskRunner>
      inspector_task_runner_for_instrumentation_pause_;
  static ClientMessageLoopAdapter* instance_;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::instance_ = nullptr;

void WebDevToolsAgentImpl::AttachSession(DevToolsSession* session,
                                         bool restore) {
  if (!network_agents_.size())
    Thread::Current()->AddTaskObserver(this);

  InspectedFrames* inspected_frames = inspected_frames_.Get();
  v8::Isolate* isolate =
      inspected_frames->Root()->GetPage()->GetAgentGroupScheduler().Isolate();
  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated(isolate);
  MainThreadDebugger* main_thread_debugger =
      MainThreadDebugger::Instance(isolate);

  int context_group_id =
      main_thread_debugger->ContextGroupId(inspected_frames->Root());
  session->ConnectToV8(main_thread_debugger->GetV8Inspector(),
                       context_group_id);

  InspectorDOMAgent* dom_agent = session->CreateAndAppend<InspectorDOMAgent>(
      isolate, inspected_frames, session->V8Session());

  session->CreateAndAppend<InspectorLayerTreeAgent>(inspected_frames, this);

  InspectorNetworkAgent* network_agent =
      session->CreateAndAppend<InspectorNetworkAgent>(inspected_frames, nullptr,
                                                      session->V8Session());

  auto* css_agent = session->CreateAndAppend<InspectorCSSAgent>(
      dom_agent, inspected_frames, network_agent,
      resource_content_loader_.Get(), resource_container_.Get());

  InspectorDOMDebuggerAgent* dom_debugger_agent =
      session->CreateAndAppend<InspectorDOMDebuggerAgent>(isolate, dom_agent,
                                                          session->V8Session());

  session->CreateAndAppend<InspectorEventBreakpointsAgent>(
      session->V8Session());

  session->CreateAndAppend<InspectorPerformanceAgent>(inspected_frames);

  session->CreateAndAppend<InspectorDOMSnapshotAgent>(inspected_frames,
                                                      dom_debugger_agent);

  session->CreateAndAppend<InspectorAnimationAgent>(inspected_frames, css_agent,
                                                    session->V8Session());

  session->CreateAndAppend<InspectorMemoryAgent>(inspected_frames);

  auto* page_agent = session->CreateAndAppend<InspectorPageAgent>(
      inspected_frames, this, resource_content_loader_.Get(),
      session->V8Session());

  session->CreateAndAppend<InspectorLogAgent>(
      &inspected_frames->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames->Root()->GetPerformanceMonitor(), session->V8Session());

  InspectorOverlayAgent* overlay_agent =
      session->CreateAndAppend<InspectorOverlayAgent>(
          web_local_frame_impl_.Get(), inspected_frames, session->V8Session(),
          dom_agent);

  session->CreateAndAppend<InspectorIOAgent>(isolate, session->V8Session());

  session->CreateAndAppend<InspectorAuditsAgent>(
      network_agent,
      &inspected_frames->Root()->GetPage()->GetInspectorIssueStorage(),
      inspected_frames, web_local_frame_impl_->AutofillClient());

  session->CreateAndAppend<InspectorMediaAgent>(
      inspected_frames, /*worker_global_scope=*/nullptr);

  auto* virtual_time_controller =
      web_local_frame_impl_->View()->Scheduler()->GetVirtualTimeController();
  DCHECK(virtual_time_controller);
  // TODO(dgozman): we should actually pass the view instead of frame, but
  // during remote->local transition we cannot access mainFrameImpl() yet, so
  // we have to store the frame which will become the main frame later.
  session->CreateAndAppend<InspectorEmulationAgent>(web_local_frame_impl_.Get(),
                                                    *virtual_time_controller);

  session->CreateAndAppend<InspectorPerformanceTimelineAgent>(inspected_frames);

  session->CreateAndAppend<InspectorPreloadAgent>(inspected_frames);

  // Call session init callbacks registered from higher layers.
  CoreInitializer::GetInstance().InitInspectorAgentSession(
      session, include_view_agents_, dom_agent, inspected_frames,
      web_local_frame_impl_->ViewImpl()->GetPage());

  if (node_to_inspect_) {
    overlay_agent->Inspect(node_to_inspect_);
    node_to_inspect_ = nullptr;
  }

  network_agents_.insert(session, network_agent);
  page_agents_.insert(session, page_agent);
  overlay_agents_.insert(session, overlay_agent);
}

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForFrame(
    WebLocalFrameImpl* frame) {
  return MakeGarbageCollected<WebDevToolsAgentImpl>(frame, IsMainFrame(frame));
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* web_local_frame_impl,
    bool include_view_agents)
    : web_local_frame_impl_(web_local_frame_impl),
      probe_sink_(web_local_frame_impl_->GetFrame()->GetProbeSink()),
      resource_content_loader_(
          MakeGarbageCollected<InspectorResourceContentLoader>(
              web_local_frame_impl_->GetFrame())),
      inspected_frames_(MakeGarbageCollected<InspectedFrames>(
          web_local_frame_impl_->GetFrame())),
      resource_container_(
          MakeGarbageCollected<InspectorResourceContainer>(inspected_frames_)),
      include_view_agents_(include_view_agents) {
  DCHECK(IsMainThread());
  agent_ = MakeGarbageCollected<DevToolsAgent>(
      this, inspected_frames_.Get(), probe_sink_.Get(),
      web_local_frame_impl_->GetFrame()->GetInspectorTaskRunner(),
      Platform::Current()->GetIOTaskRunner());
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {}

void WebDevToolsAgentImpl::Trace(Visitor* visitor) const {
  visitor->Trace(agent_);
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
}

void WebDevToolsAgentImpl::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver) {
  agent_->BindReceiver(
      std::move(host_remote), std::move(receiver),
      web_local_frame_impl_->GetTaskRunner(TaskType::kInternalInspector));
}

void WebDevToolsAgentImpl::DetachSession(DevToolsSession* session) {
  network_agents_.erase(session);
  page_agents_.erase(session);
  overlay_agents_.erase(session);
  if (!network_agents_.size())
    Thread::Current()->RemoveTaskObserver(this);
}

void WebDevToolsAgentImpl::InspectElement(
    const gfx::Point& point_in_local_root) {
  gfx::PointF point =
      web_local_frame_impl_->FrameWidgetImpl()->DIPsToBlinkSpace(
          gfx::PointF(point_in_local_root));

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  HitTestRequest request(hit_type);
  WebMouseEvent dummy_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            base::TimeTicks::Now());
  dummy_event.SetPositionInWidget(point);
  gfx::Point transformed_point = gfx::ToFlooredPoint(
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

  if (!overlay_agents_.empty()) {
    for (auto& it : overlay_agents_)
      it.value->Inspect(node);
  } else {
    node_to_inspect_ = node;
  }
}

void WebDevToolsAgentImpl::DebuggerTaskStarted() {
  probe::WillStartDebuggerTask(probe_sink_);
}

void WebDevToolsAgentImpl::DebuggerTaskFinished() {
  probe::DidFinishDebuggerTask(probe_sink_);
}

void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  resource_container_->DidCommitLoadForLocalFrame(frame);
  resource_content_loader_->DidCommitLoadForLocalFrame(frame);
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

void WebDevToolsAgentImpl::DidShowNewWindow() {
  if (!wait_for_debugger_when_shown_)
    return;
  wait_for_debugger_when_shown_ = false;
  base::AutoReset<bool> is_paused(&is_paused_for_new_window_shown_, true);
  WaitForDebugger();
}

void WebDevToolsAgentImpl::WaitForDebuggerWhenShown() {
  wait_for_debugger_when_shown_ = true;
}

void WebDevToolsAgentImpl::WaitForDebugger() {
  ClientMessageLoopAdapter::PauseForPageWait(web_local_frame_impl_);
}

bool WebDevToolsAgentImpl::IsPausedForNewWindow() {
  return is_paused_for_new_window_shown_;
}

bool WebDevToolsAgentImpl::IsInspectorLayer(const cc::Layer* layer) {
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

void WebDevToolsAgentImpl::UpdateOverlaysPrePaint() {
  for (auto& it : overlay_agents_)
    it.value->UpdatePrePaint();
}

void WebDevToolsAgentImpl::PaintOverlays(GraphicsContext& context) {
  for (auto& it : overlay_agents_)
    it.value->PaintOverlay(context);
}

void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {
  for (auto& it : overlay_agents_)
    it.value->DispatchBufferedTouchEvents();
}

void WebDevToolsAgentImpl::SetPageIsScrolling(bool is_scrolling) {
  for (auto& it : overlay_agents_)
    it.value->SetPageIsScrolling(is_scrolling);
}

WebInputEventResult WebDevToolsAgentImpl::HandleInputEvent(
    const WebInputEvent& event) {
  for (auto& it : overlay_agents_) {
    auto result = it.value->HandleInputEvent(event);
    if (result != WebInputEventResult::kNotHandled)
      return result;
  }
  return WebInputEventResult::kNotHandled;
}

void WebDevToolsAgentImpl::ActivatePausedDebuggerWindow(
    WebLocalFrameImpl* local_root) {
  ClientMessageLoopAdapter::ActivatePausedDebuggerWindow(local_root);
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

void WebDevToolsAgentImpl::MainThreadDebuggerPaused() {
  agent_->DebuggerPaused();
}

void WebDevToolsAgentImpl::MainThreadDebuggerResumed() {
  agent_->DebuggerResumed();
}

void WebDevToolsAgentImpl::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {
  if (network_agents_.empty())
    return;
  v8::Isolate* isolate =
      inspected_frames_->Root()->GetPage()->GetAgentGroupScheduler().Isolate();
  ThreadDebugger::IdleFinished(isolate);
}

void WebDevToolsAgentImpl::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (network_agents_.empty())
    return;
  v8::Isolate* isolate =
      inspected_frames_->Root()->GetPage()->GetAgentGroupScheduler().Isolate();
  ThreadDebugger::IdleStarted(isolate);
  FlushProtocolNotifications();
}

}  // namespace blink
