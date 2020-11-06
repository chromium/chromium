// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_agent.h"

#include <v8-inspector.h>
#include <memory>

#include "base/bind_helpers.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

using StatePtr = mojo::StructPtr<blink::mojom::blink::DevToolsSessionState>;
template <>
struct CrossThreadCopier<StatePtr>
    : public CrossThreadCopierByValuePassThrough<StatePtr> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

DevToolsAgent* DevToolsAgentFromContext(ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    return scope->GetThread()
        ->GetWorkerInspectorController()
        ->GetDevToolsAgent();
  }
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame)
      return nullptr;
    WebLocalFrameImpl* web_frame =
        WebLocalFrameImpl::FromFrame(frame->LocalFrameRoot());
    if (!web_frame)
      return nullptr;
    return web_frame->DevToolsAgentImpl()->GetDevToolsAgent();
  }
  return nullptr;
}

}  // namespace

class DevToolsAgent::IOAgent : public mojom::blink::DevToolsAgent {
 public:
  IOAgent(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
          scoped_refptr<InspectorTaskRunner> inspector_task_runner,
          CrossThreadWeakPersistent<::blink::DevToolsAgent> agent,
          mojo::PendingReceiver<mojom::blink::DevToolsAgent> receiver)
      : io_task_runner_(io_task_runner),
        inspector_task_runner_(inspector_task_runner),
        agent_(std::move(agent)) {
    // Binds on the IO thread and receive messages there too. Messages are
    // posted to the worker thread in a way that interrupts V8 execution. This
    // is necessary so that AttachDevToolsSession can be called on a worker
    // which has already started and is stuck in JS, e.g. polling using
    // Atomics.wait() which is a common pattern.
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&IOAgent::BindInterface,
                            CrossThreadUnretained(this), std::move(receiver)));
  }

  void BindInterface(
      mojo::PendingReceiver<mojom::blink::DevToolsAgent> receiver) {
    receiver_.Bind(std::move(receiver), io_task_runner_);
  }

  void DeleteSoon() { io_task_runner_->DeleteSoon(FROM_HERE, this); }

  ~IOAgent() override = default;

  // mojom::blink::DevToolsAgent implementation.
  void AttachDevToolsSession(
      mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost> host,
      mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
          main_session,
      mojo::PendingReceiver<mojom::blink::DevToolsSession> io_session,
      mojom::blink::DevToolsSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      const WTF::String& session_id) override {
    DCHECK(receiver_.is_bound());
    inspector_task_runner_->AppendTask(CrossThreadBindOnce(
        &::blink::DevToolsAgent::AttachDevToolsSessionImpl, agent_,
        std::move(host), std::move(main_session), std::move(io_session),
        std::move(reattach_session_state), client_expects_binary_responses,
        session_id));
  }

  void InspectElement(const gfx::Point& point) override {
    // InspectElement on a worker doesn't make sense.
    NOTREACHED();
  }

  void ReportChildWorkers(bool report,
                          bool wait_for_debugger,
                          base::OnceClosure callback) override {
    DCHECK(receiver_.is_bound());
    inspector_task_runner_->AppendTask(CrossThreadBindOnce(
        &::blink::DevToolsAgent::ReportChildWorkersPostCallbackToIO, agent_,
        report, wait_for_debugger, CrossThreadBindOnce(std::move(callback))));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakPersistent<::blink::DevToolsAgent> agent_;
  mojo::Receiver<mojom::blink::DevToolsAgent> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(IOAgent);
};

DevToolsAgent::DevToolsAgent(
    Client* client,
    InspectedFrames* inspected_frames,
    CoreProbeSink* probe_sink,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : client_(client),
      inspected_frames_(inspected_frames),
      probe_sink_(probe_sink),
      inspector_task_runner_(std::move(inspector_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {}

DevToolsAgent::~DevToolsAgent() = default;

void DevToolsAgent::Trace(Visitor* visitor) const {
  visitor->Trace(associated_receiver_);
  visitor->Trace(host_remote_);
  visitor->Trace(associated_host_remote_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(probe_sink_);
  visitor->Trace(sessions_);
}

void DevToolsAgent::Dispose() {
  HeapHashSet<Member<DevToolsSession>> copy(sessions_);
  for (auto& session : copy)
    session->Detach();
  CleanupConnection();
}

void DevToolsAgent::BindReceiverForWorker(
    mojo::PendingRemote<mojom::blink::DevToolsAgentHost> host_remote,
    mojo::PendingReceiver<mojom::blink::DevToolsAgent> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!associated_receiver_.is_bound());

  host_remote_.Bind(std::move(host_remote), std::move(task_runner));
  host_remote_.set_disconnect_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));

  io_agent_ =
      new IOAgent(io_task_runner_, inspector_task_runner_,
                  WrapCrossThreadWeakPersistent(this), std::move(receiver));
}

void DevToolsAgent::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!associated_receiver_.is_bound());
  associated_receiver_.Bind(std::move(receiver), task_runner);
  associated_host_remote_.Bind(std::move(host_remote), task_runner);
  associated_host_remote_.set_disconnect_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

void DevToolsAgent::AttachDevToolsSessionImpl(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        session_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_session_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    const WTF::String& session_id) {
  TRACE_EVENT0("devtools", "Agent::AttachDevToolsSessionImpl");
  client_->DebuggerTaskStarted();
  DevToolsSession* session = MakeGarbageCollected<DevToolsSession>(
      this, std::move(host), std::move(session_receiver),
      std::move(io_session_receiver), std::move(reattach_session_state),
      client_expects_binary_responses, session_id,
      inspector_task_runner_->isolate_task_runner());
  sessions_.insert(session);
  client_->DebuggerTaskFinished();
}

void DevToolsAgent::AttachDevToolsSession(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        session_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_session_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    const WTF::String& session_id) {
  TRACE_EVENT0("devtools", "Agent::AttachDevToolsSession");
  if (associated_receiver_.is_bound()) {
    AttachDevToolsSessionImpl(std::move(host), std::move(session_receiver),
                              std::move(io_session_receiver),
                              std::move(reattach_session_state),
                              client_expects_binary_responses, session_id);
  } else {
    io_agent_->AttachDevToolsSession(
        std::move(host), std::move(session_receiver),
        std::move(io_session_receiver), std::move(reattach_session_state),
        client_expects_binary_responses, session_id);
  }
}

void DevToolsAgent::InspectElementImpl(const gfx::Point& point) {
  client_->InspectElement(point);
}

void DevToolsAgent::InspectElement(const gfx::Point& point) {
  if (associated_receiver_.is_bound()) {
    client_->InspectElement(point);
  } else {
    // InspectElement on a worker doesn't make sense.
    NOTREACHED();
  }
}

void DevToolsAgent::FlushProtocolNotifications() {
  for (auto& session : sessions_)
    session->FlushProtocolNotifications();
}

void DevToolsAgent::ReportChildWorkersPostCallbackToIO(
    bool report,
    bool wait_for_debugger,
    CrossThreadOnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildWorkersPostCallbackToIO");
  ReportChildWorkersImpl(report, wait_for_debugger, base::DoNothing());
  // This message originally came from the IOAgent for a worker which means the
  // response needs to be sent on the IO thread as well, so we post the callback
  // task back there to be run. In the non-IO case, this callback would be run
  // synchronously at the end of ReportChildWorkersImpl, so the ordering between
  // ReportChildWorkers and running the callback is preserved.
  PostCrossThreadTask(*io_task_runner_, FROM_HERE, std::move(callback));
}

void DevToolsAgent::ReportChildWorkersImpl(bool report,
                                           bool wait_for_debugger,
                                           base::OnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildWorkersImpl");
  report_child_workers_ = report;
  pause_child_workers_on_start_ = wait_for_debugger;
  if (report_child_workers_) {
    auto workers = std::move(unreported_child_worker_threads_);
    for (auto& it : workers)
      ReportChildWorker(std::move(it.value));
  }
  std::move(callback).Run();
}

void DevToolsAgent::ReportChildWorkers(bool report,
                                       bool wait_for_debugger,
                                       base::OnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildWorkers");
  if (associated_receiver_.is_bound()) {
    ReportChildWorkersImpl(report, wait_for_debugger, std::move(callback));
  } else {
    io_agent_->ReportChildWorkers(report, wait_for_debugger,
                                  std::move(callback));
  }
}

// static
std::unique_ptr<WorkerDevToolsParams> DevToolsAgent::WorkerThreadCreated(
    ExecutionContext* parent_context,
    WorkerThread* worker_thread,
    const KURL& url,
    const String& global_scope_name,
    const base::Optional<const blink::DedicatedWorkerToken>& token) {
  auto result = std::make_unique<WorkerDevToolsParams>();
  base::UnguessableToken devtools_worker_token =
      token.has_value() ? token.value().value()
                        : base::UnguessableToken::Create();
  result->devtools_worker_token = devtools_worker_token;

  DevToolsAgent* agent = DevToolsAgentFromContext(parent_context);
  if (!agent)
    return result;

  auto data = std::make_unique<WorkerData>();
  data->url = url;
  result->agent_receiver = data->agent_remote.InitWithNewPipeAndPassReceiver();
  data->host_receiver =
      result->agent_host_remote.InitWithNewPipeAndPassReceiver();
  data->devtools_worker_token = result->devtools_worker_token;
  data->waiting_for_debugger = agent->pause_child_workers_on_start_;
  data->name = global_scope_name;
  result->wait_for_debugger = agent->pause_child_workers_on_start_;

  if (agent->report_child_workers_) {
    agent->ReportChildWorker(std::move(data));
  } else {
    agent->unreported_child_worker_threads_.insert(worker_thread,
                                                   std::move(data));
  }
  return result;
}

// static
void DevToolsAgent::WorkerThreadTerminated(ExecutionContext* parent_context,
                                           WorkerThread* worker_thread) {
  if (DevToolsAgent* agent = DevToolsAgentFromContext(parent_context))
    agent->unreported_child_worker_threads_.erase(worker_thread);
}

void DevToolsAgent::ReportChildWorker(std::unique_ptr<WorkerData> data) {
  if (host_remote_.is_bound()) {
    host_remote_->ChildWorkerCreated(
        std::move(data->agent_remote), std::move(data->host_receiver),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger);
  } else if (associated_host_remote_.is_bound()) {
    associated_host_remote_->ChildWorkerCreated(
        std::move(data->agent_remote), std::move(data->host_receiver),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger);
  }
}

void DevToolsAgent::CleanupConnection() {
  if (io_agent_) {
    io_agent_->DeleteSoon();
    io_agent_ = nullptr;
  }
  associated_receiver_.reset();
  host_remote_.reset();
  associated_host_remote_.reset();
  report_child_workers_ = false;
  pause_child_workers_on_start_ = false;
}

}  // namespace blink
