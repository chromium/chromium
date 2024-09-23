// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_agent.h"

#include <v8-inspector.h>

#include <memory>

#include "base/debug/crash_logging.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

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
    return web_frame->DevToolsAgentImpl(/*create_if_necessary=*/true)
        ->GetDevToolsAgent();
  }
  return nullptr;
}

}  // namespace

// Used by the DevToolsAgent class to bind the passed |receiver| on the IO
// thread. Lives on the IO thread and posts to |inspector_task_runner| to do
// actual work. This class is used when DevToolsAgent runs on a worker so we
// don't block its execution.
class DevToolsAgent::IOAgent : public mojom::blink::DevToolsAgent {
 public:
  IOAgent(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
          scoped_refptr<InspectorTaskRunner> inspector_task_runner,
          CrossThreadWeakHandle<::blink::DevToolsAgent> agent,
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

  IOAgent(const IOAgent&) = delete;
  IOAgent& operator=(const IOAgent&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::blink::DevToolsAgent> receiver) {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    receiver_.Bind(std::move(receiver), io_task_runner_);
  }

  // May be called from any thread.
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
      bool client_is_trusted,
      const WTF::String& session_id,
      bool session_waits_for_debugger) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    DCHECK(receiver_.is_bound());
    inspector_task_runner_->AppendTask(CrossThreadBindOnce(
        &::blink::DevToolsAgent::AttachDevToolsSessionImpl,
        MakeUnwrappingCrossThreadWeakHandle(agent_), std::move(host),
        std::move(main_session), std::move(io_session),
        std::move(reattach_session_state), client_expects_binary_responses,
        client_is_trusted, session_id, session_waits_for_debugger));
  }

  void InspectElement(const gfx::Point& point) override {
    // InspectElement on a worker doesn't make sense.
    NOTREACHED_IN_MIGRATION();
  }

  void ReportChildTargets(bool report,
                          bool wait_for_debugger,
                          base::OnceClosure callback) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    DCHECK(receiver_.is_bound());

    // Splitting the mojo callback so we don't drop it if the
    // inspector_task_runner_ has been disposed already.
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    bool did_append_task =
        inspector_task_runner_->AppendTask(CrossThreadBindOnce(
            &blink::DevToolsAgent::ReportChildTargetsPostCallbackToIO,
            MakeUnwrappingCrossThreadWeakHandle(agent_), report,
            wait_for_debugger,
            CrossThreadBindOnce(std::move(split_callback.first))));

    if (!did_append_task) {
      // If the task runner is no longer processing tasks (typically during
      // shutdown after InspectorTaskRunner::Dispose() has been called), `this`
      // is expected to be destroyed shortly after by a task posted to the IO
      // thread in DeleteSoon(). Until that task runs and tears down the Mojo
      // endpoint, Mojo expects all reply callbacks to be properly handled and
      // not simply dropped on the floor, so just invoke `callback` even though
      // it's somewhat pointless. Note that even if InspectorTaskRunner did
      // successfully append a task it's not guaranteed that it'll be executed
      // but it also won't simply be dropped.
      std::move(split_callback.second).Run();
    }
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakHandle<::blink::DevToolsAgent> agent_;
  mojo::Receiver<mojom::blink::DevToolsAgent> receiver_{this};
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
  host_remote_.set_disconnect_handler(WTF::BindOnce(
      &DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));

  io_agent_ = new IOAgent(io_task_runner_, inspector_task_runner_,
                          MakeCrossThreadWeakHandle(this), std::move(receiver));
}

void DevToolsAgent::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!associated_receiver_.is_bound());
  associated_receiver_.Bind(std::move(receiver), task_runner);
  associated_host_remote_.Bind(std::move(host_remote), task_runner);
  associated_host_remote_.set_disconnect_handler(WTF::BindOnce(
      &DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

namespace {
void UpdateSessionCountCrashKey(int delta) {
  static std::atomic_int s_session_count;

  int old_value = s_session_count.fetch_add(delta, std::memory_order_relaxed);
  CHECK_GE(old_value, 0);
  const bool need_update = old_value == 0 || (delta + old_value == 0);
  if (!need_update) {
    return;
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  base::AutoLock auto_lock(lock);
  static base::debug::CrashKeyString* devtools_present =
      base::debug::AllocateCrashKeyString("devtools_present",
                                          base::debug::CrashKeySize::Size32);
  SetCrashKeyString(
      devtools_present,
      s_session_count.load(std::memory_order_relaxed) ? "true" : "false");
}
}  // namespace

void DevToolsAgent::AttachDevToolsSessionImpl(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        session_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_session_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    bool client_is_trusted,
    const WTF::String& session_id,
    bool session_waits_for_debugger) {
  TRACE_EVENT0("devtools", "Agent::AttachDevToolsSessionImpl");
  client_->DebuggerTaskStarted();
  DevToolsSession* session = MakeGarbageCollected<DevToolsSession>(
      this, std::move(host), std::move(session_receiver),
      std::move(io_session_receiver), std::move(reattach_session_state),
      client_expects_binary_responses, client_is_trusted, session_id,
      session_waits_for_debugger,
      // crbug.com/333093232: Mojo ignores the task runner passed to Bind for
      // channel associated interfaces but uses it for disconnect. Since
      // devtools relies on a disconnect handler for detaching and is sensitive
      // to reordering of detach and attach, there's a dependency between task
      // queues, which is not allowed. To get around this, use the same task
      // runner that mojo uses for incoming channel associated messages.
      IsMainThread() ? Thread::MainThread()->GetTaskRunner(
                           MainThreadTaskRunnerRestricted{})
                     : inspector_task_runner_->isolate_task_runner());
  sessions_.insert(session);
  UpdateSessionCountCrashKey(1);
  client_->DebuggerTaskFinished();
}

void DevToolsAgent::DetachDevToolsSession(DevToolsSession* session) {
  sessions_.erase(session);
  UpdateSessionCountCrashKey(-1);
}

void DevToolsAgent::AttachDevToolsSession(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
        session_receiver,
    mojo::PendingReceiver<mojom::blink::DevToolsSession> io_session_receiver,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    bool client_is_trusted,
    const WTF::String& session_id,
    bool session_waits_for_debugger) {
  TRACE_EVENT0("devtools", "Agent::AttachDevToolsSession");
  if (associated_receiver_.is_bound()) {
    // Discard `session_waits_for_debugger` for regular pages, this is rather
    // handled by the navigation throttles machinery on the browser side.
    AttachDevToolsSessionImpl(
        std::move(host), std::move(session_receiver),
        std::move(io_session_receiver), std::move(reattach_session_state),
        client_expects_binary_responses, client_is_trusted, session_id,
        /* session_waits_for_debugger */ false);
  } else {
    io_agent_->AttachDevToolsSession(
        std::move(host), std::move(session_receiver),
        std::move(io_session_receiver), std::move(reattach_session_state),
        client_expects_binary_responses, client_is_trusted, session_id,
        session_waits_for_debugger);
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
    NOTREACHED_IN_MIGRATION();
  }
}

void DevToolsAgent::FlushProtocolNotifications() {
  for (auto& session : sessions_)
    session->FlushProtocolNotifications();
}

void DevToolsAgent::DebuggerPaused() {
  CHECK(!host_remote_.is_bound());
  if (associated_host_remote_.is_bound()) {
    associated_host_remote_->MainThreadDebuggerPaused();
  }
}

void DevToolsAgent::DebuggerResumed() {
  CHECK(!host_remote_.is_bound());
  if (associated_host_remote_.is_bound()) {
    associated_host_remote_->MainThreadDebuggerResumed();
  }
}

void DevToolsAgent::ReportChildTargetsPostCallbackToIO(
    bool report,
    bool wait_for_debugger,
    CrossThreadOnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildTargetsPostCallbackToIO");
  ReportChildTargetsImpl(report, wait_for_debugger, base::DoNothing());
  // This message originally came from the IOAgent for a worker which means the
  // response needs to be sent on the IO thread as well, so we post the callback
  // task back there to be run. In the non-IO case, this callback would be run
  // synchronously at the end of ReportChildTargetsImpl, so the ordering between
  // ReportChildTargets and running the callback is preserved.
  PostCrossThreadTask(*io_task_runner_, FROM_HERE, std::move(callback));
}

void DevToolsAgent::ReportChildTargetsImpl(bool report,
                                           bool wait_for_debugger,
                                           base::OnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildTargetsImpl");
  report_child_workers_ = report;
  pause_child_workers_on_start_ = wait_for_debugger;
  if (report_child_workers_) {
    auto workers = std::move(unreported_child_worker_threads_);
    for (auto& it : workers)
      ReportChildTarget(std::move(it.value));
  }
  std::move(callback).Run();
}

void DevToolsAgent::ReportChildTargets(bool report,
                                       bool wait_for_debugger,
                                       base::OnceClosure callback) {
  TRACE_EVENT0("devtools", "Agent::ReportChildTargets");
  if (associated_receiver_.is_bound()) {
    ReportChildTargetsImpl(report, wait_for_debugger, std::move(callback));
  } else {
    io_agent_->ReportChildTargets(report, wait_for_debugger,
                                  std::move(callback));
  }
}

// static
std::unique_ptr<WorkerDevToolsParams> DevToolsAgent::WorkerThreadCreated(
    ExecutionContext* parent_context,
    WorkerThread* worker_thread,
    const KURL& url,
    const String& global_scope_name,
    const std::optional<const blink::DedicatedWorkerToken>& token) {
  auto result = std::make_unique<WorkerDevToolsParams>();
  base::UnguessableToken devtools_worker_token =
      token.has_value() ? token.value().value()
                        : base::UnguessableToken::Create();
  result->devtools_worker_token = devtools_worker_token;

  DevToolsAgent* agent = DevToolsAgentFromContext(parent_context);
  if (!agent)
    return result;

  mojom::blink::DevToolsExecutionContextType context_type =
      token.has_value()
          ? mojom::blink::DevToolsExecutionContextType::kDedicatedWorker
          : mojom::blink::DevToolsExecutionContextType::kWorklet;

  auto data = std::make_unique<WorkerData>();
  data->url = url;
  result->agent_receiver = data->agent_remote.InitWithNewPipeAndPassReceiver();
  data->host_receiver =
      result->agent_host_remote.InitWithNewPipeAndPassReceiver();
  data->devtools_worker_token = result->devtools_worker_token;
  data->waiting_for_debugger = agent->pause_child_workers_on_start_;
  data->name = global_scope_name;
  data->context_type = context_type;
  result->wait_for_debugger = agent->pause_child_workers_on_start_;

  if (agent->report_child_workers_) {
    agent->ReportChildTarget(std::move(data));
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

void DevToolsAgent::ReportChildTarget(std::unique_ptr<WorkerData> data) {
  if (host_remote_.is_bound()) {
    host_remote_->ChildTargetCreated(
        std::move(data->agent_remote), std::move(data->host_receiver),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger,
        data->context_type);
  } else if (associated_host_remote_.is_bound()) {
    associated_host_remote_->ChildTargetCreated(
        std::move(data->agent_remote), std::move(data->host_receiver),
        std::move(data->url), std::move(data->name),
        data->devtools_worker_token, data->waiting_for_debugger,
        data->context_type);
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

void DevToolsAgent::BringDevToolsWindowToFocus() {
  if (associated_host_remote_.is_bound()) {
    associated_host_remote_->BringToForeground();
  }
}

}  // namespace blink
