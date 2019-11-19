// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_GLOBAL_SCOPE_H_

#include <bitset>
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class FetchClientSettingsObject;
class FetchClientSettingsObjectSnapshot;
class Modulator;
class ModuleTreeClient;
class ResourceFetcher;
class WorkerResourceTimingNotifier;
class SubresourceFilter;
class WebWorkerFetchContext;
class WorkerOrWorkletScriptController;
class WorkerReportingProxy;
class WorkerThread;

class CORE_EXPORT WorkerOrWorkletGlobalScope : public EventTargetWithInlineData,
                                               public ExecutionContext,
                                               public SecurityContext {
 public:
  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;

  WorkerOrWorkletGlobalScope(
      v8::Isolate*,
      scoped_refptr<SecurityOrigin> origin,
      Agent* agent,
      OffMainThreadWorkerScriptFetchOption,
      const String& name,
      const base::UnguessableToken& parent_devtools_token,
      V8CacheOptions,
      WorkerClients*,
      std::unique_ptr<WebContentSettingsClient>,
      scoped_refptr<WebWorkerFetchContext>,
      WorkerReportingProxy&);
  ~WorkerOrWorkletGlobalScope() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;

  // ScriptWrappable
  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) final;
  v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;
  bool HasPendingActivity() const override;

  // ExecutionContext
  bool IsWorkerOrWorkletGlobalScope() const final { return true; }
  bool IsJSExecutionForbidden() const final;
  void DisableEval(const String& error_message) final;
  bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) final;

  // Returns true when the WorkerOrWorkletGlobalScope is closing (e.g. via
  // WorkerGlobalScope#close() method). If this returns true, the worker is
  // going to be shutdown after the current task execution. Globals that
  // don't support close operation should always return false.
  virtual bool IsClosing() const = 0;

  // Should be called before destroying the global scope object. Allows
  // sub-classes to perform any cleanup needed.
  virtual void Dispose();

  void SetModulator(Modulator*);

  // Called from UseCounter to record API use in this execution context.
  // TODO(yhirano): Unify this with CountUse.
  void CountFeature(WebFeature);

  // UseCounter
  void CountUse(WebFeature feature) final { CountFeature(feature); }
  void CountDeprecation(WebFeature) final;

  // May return nullptr if this global scope is not threaded (i.e.,
  // WorkletGlobalScope for the main thread) or after Dispose() is called.
  virtual WorkerThread* GetThread() const = 0;

  // Returns nullptr if this global scope is a WorkletGlobalScope
  virtual WorkerNavigator* navigator() const { return nullptr; }

  // Returns the resource fetcher for subresources (a.k.a. inside settings
  // resource fetcher). See core/workers/README.md for details.
  ResourceFetcher* Fetcher() const override;
  ResourceFetcher* EnsureFetcher();

  // ResourceFetcher for off-the-main-thread worker top-level script fetching,
  // corresponding to "outside" fetch client's settings object.
  // CreateOutsideSettingsFetcher() is called for each invocation of top-level
  // script fetch, which can occur multiple times in worklets.
  // TODO(hiroshige, nhiroki): Currently this outside ResourceFetcher and its
  // WorkerFetchContext is mostly the copy of the insideSettings
  // ResourceFetcher, and have dependencies to WorkerOrWorkletGlobalScope. Plumb
  // more data to the outside ResourceFetcher to fix the behavior and reduce the
  // dependencies.
  ResourceFetcher* CreateOutsideSettingsFetcher(
      const FetchClientSettingsObject&,
      WorkerResourceTimingNotifier&);

  const String Name() const { return name_; }
  const base::UnguessableToken& GetParentDevToolsToken() {
    return parent_devtools_token_;
  }

  WorkerClients* Clients() const { return worker_clients_.Get(); }

  // May return nullptr.
  WebContentSettingsClient* ContentSettingsClient() const {
    return content_settings_client_.get();
  }

  WorkerOrWorkletScriptController* ScriptController() {
    return script_controller_.Get();
  }
  V8CacheOptions GetV8CacheOptions() const { return v8_cache_options_; }

  WorkerReportingProxy& ReportingProxy() { return reporting_proxy_; }

  void Trace(blink::Visitor*) override;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  OffMainThreadWorkerScriptFetchOption GetOffMainThreadWorkerScriptFetchOption()
      const {
    return off_main_thread_fetch_option_;
  }

  void ApplySandboxFlags(SandboxFlags mask);

 protected:
  // Sets outside's CSP used for off-main-thread top-level worker script
  // fetch.
  void SetOutsideContentSecurityPolicyHeaders(const Vector<CSPHeaderAndType>&);

  // Initializes inside's CSP used for subresource fetch etc.
  void InitContentSecurityPolicyFromVector(const Vector<CSPHeaderAndType>&);
  virtual void BindContentSecurityPolicyToExecutionContext();

  void FetchModuleScript(const KURL& module_url_record,
                         const FetchClientSettingsObjectSnapshot&,
                         WorkerResourceTimingNotifier&,
                         mojom::RequestContextType destination,
                         network::mojom::CredentialsMode,
                         ModuleScriptCustomFetchType,
                         ModuleTreeClient*);

  void TasksWerePaused() override;
  void TasksWereUnpaused() override;

  const Vector<CSPHeaderAndType>& OutsideContentSecurityPolicyHeaders() const {
    return outside_content_security_policy_headers_;
  }

 private:
  void InitializeWebFetchContextIfNeeded();
  ResourceFetcher* CreateFetcherInternal(const FetchClientSettingsObject&,
                                         ContentSecurityPolicy&,
                                         WorkerResourceTimingNotifier&);

  bool web_fetch_context_initialized_ = false;

  const OffMainThreadWorkerScriptFetchOption off_main_thread_fetch_option_;
  const String name_;
  const base::UnguessableToken parent_devtools_token_;

  CrossThreadPersistent<WorkerClients> worker_clients_;
  std::unique_ptr<WebContentSettingsClient> content_settings_client_;

  Member<ResourceFetcher> inside_settings_resource_fetcher_;

  // Keeps track of all ResourceFetchers (including
  // |inside_settings_resource_fetcher_|) for disposing and pausing/unpausing.
  HeapHashSet<WeakMember<ResourceFetcher>> resource_fetchers_;

  // A WorkerOrWorkletGlobalScope has one WebWorkerFetchContext and one
  // corresponding SubresourceFilter, which are shared by all
  // WorkerFetchContexts of |this| global scope, i.e. those behind
  // ResourceFetchers created by EnsureFetcher() and
  // CreateOutsideSettingsFetcher().
  // As all references to |web_worker_fetch_context_| are on the context
  // thread, |web_worker_fetch_context_| is destructed on the context
  // thread.
  //
  // TODO(crbug/903579): Consider putting WebWorkerFetchContext-originated
  // things at a single place. Currently they are placed here and subclasses of
  // WebWorkerFetchContext.
  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context_;
  Member<SubresourceFilter> subresource_filter_;

  Member<WorkerOrWorkletScriptController> script_controller_;
  const V8CacheOptions v8_cache_options_;

  // TODO(hiroshige): Pass outsideSettings-CSP via
  // outsideSettings-FetchClientSettingsObject.
  Vector<CSPHeaderAndType> outside_content_security_policy_headers_;

  WorkerReportingProxy& reporting_proxy_;

  // This is the set of features that this worker has used.
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      used_features_;

  // LocalDOMWindow::modulator_ workaround equivalent.
  // TODO(kouhei): Remove this.
  Member<Modulator> modulator_;
};

template <>
struct DowncastTraits<WorkerOrWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsWorkerGlobalScope() || context.IsWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_GLOBAL_SCOPE_H_
