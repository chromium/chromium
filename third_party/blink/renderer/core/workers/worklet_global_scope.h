// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class FetchClientSettingsObjectSnapshot;
class WorkletPendingTasks;
class WorkerReportingProxy;
struct GlobalScopeCreationParams;

// This is an implementation of the web-exposed WorkletGlobalScope interface
// defined in the Worklets spec:
// https://drafts.css-houdini.org/worklets/#workletglobalscope
//
// This instance lives either on the main thread (main thread worklet) or a
// worker thread (threaded worklet). It's determined by constructors. See
// comments on the constructors.
class CORE_EXPORT WorkletGlobalScope
    : public WorkerOrWorkletGlobalScope,
      public ActiveScriptWrappable<WorkletGlobalScope> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);

 public:
  ~WorkletGlobalScope() override;

  bool IsMainThreadWorkletGlobalScope() const final;
  bool IsThreadedWorkletGlobalScope() const final;
  bool IsWorkletGlobalScope() const final { return true; }

  // Always returns false here as PaintWorkletGlobalScope and
  // AnimationWorkletGlobalScope don't have a #close() method on the global.
  // Note that AudioWorkletGlobal overrides this behavior.
  bool IsClosing() const override { return false; }

  ExecutionContext* GetExecutionContext() const override;

  // ExecutionContext
  const KURL& Url() const final { return url_; }
  const KURL& BaseURL() const final { return url_; }
  KURL CompleteURL(const String&) const final;
  String UserAgent() const final { return user_agent_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  const SecurityContext& GetSecurityContext() const final { return *this; }
  bool IsSecureContext(String& error_message) const final;
  bool IsContextThread() const final;
  void AddConsoleMessageImpl(ConsoleMessage*, bool discard_duplicates) final;
  void ExceptionThrown(ErrorEvent*) final;
  CoreProbeSink* GetProbeSink() final;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) final;
  FrameOrWorkerScheduler* GetScheduler() final;

  // WorkerOrWorkletGlobalScope
  void Dispose() override;
  WorkerThread* GetThread() const final;

  virtual LocalFrame* GetFrame() const;

  const base::UnguessableToken& GetAgentClusterID() const final {
    // Currently, worklet agents have no clearly defined owner. See
    // https://html.spec.whatwg.org/C/#integration-with-the-javascript-agent-cluster-formalism
    //
    // However, it is intended that a SharedArrayBuffer can be shared with a
    // worklet, e.g. the AudioWorklet. If this WorkletGlobalScope's creation
    // params included an agent cluster ID, we'll assume that this worklet is
    // in the same agent cluster. See
    // https://bugs.chromium.org/p/chromium/issues/detail?id=892067.
    return agent_cluster_id_;
  }

  DOMTimerCoordinator* Timers() final {
    // WorkletGlobalScopes don't have timers.
    NOTREACHED();
    return nullptr;
  }

  // Implementation of the "fetch and invoke a worklet script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  // When script evaluation is done or any exception happens, it's notified to
  // the given WorkletPendingTasks via |outside_settings_task_runner| (i.e., the
  // parent frame's task runner).
  void FetchAndInvokeScript(
      const KURL& module_url_record,
      network::mojom::CredentialsMode,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*);

  WorkletModuleResponsesMap* GetModuleResponsesMap() const {
    return module_responses_map_.Get();
  }

  const SecurityOrigin* DocumentSecurityOrigin() const {
    return document_security_origin_.get();
  }

  // Customize the security context used for origin trials.
  // Origin trials are only enabled in secure contexts, but WorkletGlobalScopes
  // are defined to have a unique, opaque origin, so are not secure:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  // For origin trials, instead consider the context of the document which
  // created the worklet, since the origin trial tokens are inherited from the
  // document.
  bool DocumentSecureContext() const { return document_secure_context_; }

  void Trace(blink::Visitor*) override;

  HttpsState GetHttpsState() const override { return https_state_; }

  // Constructs an instance as a main thread worklet. Must be called on the main
  // thread.
  WorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                     WorkerReportingProxy&,
                     LocalFrame*,
                     Agent* = nullptr);
  // Constructs an instance as a threaded worklet. Must be called on a worker
  // thread.
  WorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                     WorkerReportingProxy&,
                     WorkerThread*);

  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override;

 private:
  enum class ThreadType {
    // Indicates this global scope lives on the main thread.
    kMainThread,
    // Indicates this global scope lives on a worker thread.
    kOffMainThread
  };

  // The base constructor delegated from other public constructors. This
  // partially implements the "set up a worklet environment settings object"
  // algorithm defined in the Worklets spec:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  WorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                     WorkerReportingProxy&,
                     v8::Isolate*,
                     ThreadType,
                     LocalFrame*,
                     WorkerThread*,
                     Agent*);

  EventTarget* ErrorEventTarget() final { return nullptr; }

  void BindContentSecurityPolicyToExecutionContext() override;

  // The |url_| and |user_agent_| are inherited from the parent Document.
  const KURL url_;
  const String user_agent_;

  // Used for module fetch and origin trials, inherited from the parent
  // Document.
  const scoped_refptr<const SecurityOrigin> document_security_origin_;

  // Used for origin trials, inherited from the parent Document.
  const bool document_secure_context_;

  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map_;

  const HttpsState https_state_;

  const base::UnguessableToken agent_cluster_id_;

  const ThreadType thread_type_;
  // |frame_| is available only when |thread_type_| is kMainThread.
  Member<LocalFrame> frame_;
  // |worker_thread_| is available only when |thread_type_| is kOffMainThread.
  WorkerThread* worker_thread_;
};

template <>
struct DowncastTraits<WorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_GLOBAL_SCOPE_H_
