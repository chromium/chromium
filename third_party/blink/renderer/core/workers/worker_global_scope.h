/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 2. Redistributions in binary form must reproduce the above copyright
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_GLOBAL_SCOPE_H_

#include <memory>
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/loader/worker_main_script_load_parameters.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

struct BlinkTransferableMessage;
class ConsoleMessage;
class FetchClientSettingsObjectSnapshot;
class FontFaceSet;
class InstalledScriptsManager;
class OffscreenFontSelector;
class WorkerResourceTimingNotifier;
class TrustedTypePolicyFactory;
class V8VoidFunction;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

class CORE_EXPORT WorkerGlobalScope
    : public WorkerOrWorkletGlobalScope,
      public ActiveScriptWrappable<WorkerGlobalScope>,
      public Supplementable<WorkerGlobalScope> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WorkerGlobalScope() override;

  // Returns null if caching is not supported.
  virtual SingleCachedMetadataHandler* CreateWorkerScriptCachedMetadataHandler(
      const KURL& script_url,
      std::unique_ptr<Vector<uint8_t>> meta_data) {
    return nullptr;
  }

  // WorkerOrWorkletGlobalScope
  bool IsClosing() const final { return closing_; }
  void Dispose() override;
  WorkerThread* GetThread() const final { return thread_; }
  const base::UnguessableToken& GetDevToolsToken() const override;

  void ExceptionUnhandled(int exception_id);

  // WorkerGlobalScope
  WorkerGlobalScope* self() { return this; }
  WorkerLocation* location() const;
  WorkerNavigator* navigator() const override;
  void close();
  bool isSecureContextForBindings() const {
    return ExecutionContext::IsSecureContext();
  }

  String origin() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(languagechange, kLanguagechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(rejectionhandled, kRejectionhandled)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(timezonechange, kTimezonechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unhandledrejection, kUnhandledrejection)

  // This doesn't take an ExceptionState argument, but actually can throw
  // exceptions directly to V8 (crbug/1114610).
  virtual void importScripts(const Vector<String>& urls);

  // ExecutionContext
  const KURL& Url() const final;
  KURL CompleteURL(const String&) const final;
  bool IsWorkerGlobalScope() const final { return true; }
  bool IsContextThread() const final;
  const KURL& BaseURL() const final;
  String UserAgent() const final { return user_agent_; }
  const UserAgentMetadata& GetUserAgentMetadata() const { return ua_metadata_; }
  HttpsState GetHttpsState() const override { return https_state_; }
  scheduler::WorkerScheduler* GetScheduler() final;
  ukm::UkmRecorder* UkmRecorder() final;

  void AddConsoleMessageImpl(ConsoleMessage*, bool discard_duplicates) final;
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() final;

  OffscreenFontSelector* GetFontSelector() { return font_selector_; }

  CoreProbeSink* GetProbeSink() final;

  // EventTarget
  ExecutionContext* GetExecutionContext() const final;
  bool IsWindowOrWorkerGlobalScope() const final { return true; }

  // Initializes this global scope. This must be called after worker script
  // fetch, and before initiali script evaluation.
  //
  // This corresponds to following specs:
  // - For dedicated/shared workers, step 12.3-12.6 (a custom perform the fetch
  //   hook) in https://html.spec.whatwg.org/C/#run-a-worker
  // - For service workers, step 4.5-4.11 in
  //   https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
  virtual void Initialize(
      const KURL& response_url,
      network::mojom::ReferrerPolicy response_referrer_policy,
      network::mojom::IPAddressSpace response_address_space,
      const Vector<CSPHeaderAndType>& response_csp_headers,
      const Vector<String>* response_origin_trial_tokens,
      int64_t appcache_id) = 0;

  // These methods should be called in the scope of a pausable
  // task runner. ie. They should not be called when the context
  // is paused.
  void EvaluateClassicScript(const KURL& script_url,
                             String source_code,
                             std::unique_ptr<Vector<uint8_t>> cached_meta_data,
                             const v8_inspector::V8StackTraceId& stack_id);

  // Should be called (in all successful cases) when the worker top-level
  // script fetch is finished.
  // At this time, WorkerGlobalScope::Initialize() should be already called.
  // Spec: https://html.spec.whatwg.org/C/#run-a-worker Step 12 is completed,
  // and it's ready to proceed to Step 23.
  void WorkerScriptFetchFinished(Script&,
                                 base::Optional<v8_inspector::V8StackTraceId>);

  // Fetches and evaluates the top-level classic script.
  virtual void FetchAndRunClassicScript(
      const KURL& script_url,
      std::unique_ptr<WorkerMainScriptLoadParameters>
          worker_main_script_load_params_for_modules,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      const v8_inspector::V8StackTraceId& stack_id) = 0;

  // Fetches and evaluates the top-level module script.
  virtual void FetchAndRunModuleScript(
      const KURL& module_url_record,
      std::unique_ptr<WorkerMainScriptLoadParameters>
          worker_main_script_load_params_for_modules,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      network::mojom::CredentialsMode,
      RejectCoepUnsafeNone reject_coep_unsafe_none) = 0;

  void ReceiveMessage(BlinkTransferableMessage);
  base::TimeTicks TimeOrigin() const { return time_origin_; }
  WorkerSettings* GetWorkerSettings() const { return worker_settings_.get(); }

  void Trace(Visitor*) const override;

  virtual InstalledScriptsManager* GetInstalledScriptsManager() {
    return nullptr;
  }

  // TODO(fserb): This can be removed once we WorkerGlobalScope implements
  // FontFaceSource on the IDL.
  FontFaceSet* fonts();

  // https://html.spec.whatwg.org/C/#windoworworkerglobalscope-mixin
  void queueMicrotask(V8VoidFunction*);

  TrustedTypePolicyFactory* GetTrustedTypes() const override;
  TrustedTypePolicyFactory* trustedTypes() const { return GetTrustedTypes(); }

  // TODO(https://crbug.com/835717): Remove this function after dedicated
  // workers support off-the-main-thread script fetch by default.
  virtual bool IsOffMainThreadScriptFetchDisabled() { return false; }

  // Takes the ownership of the parameters used to load the worker main module
  // script in renderer process.
  std::unique_ptr<WorkerMainScriptLoadParameters>
  TakeWorkerMainScriptLoadingParametersForModules();

  ukm::SourceId UkmSourceID() const override { return ukm_source_id_; }

  // Returns the token uniquely identifying this worker. The token type will
  // match the actual worker type.
  virtual WorkerToken GetWorkerToken() const = 0;

 protected:
  WorkerGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                    WorkerThread*,
                    base::TimeTicks time_origin,
                    ukm::SourceId);

  // ExecutionContext
  void ExceptionThrown(ErrorEvent*) override;
  void RemoveURLFromMemoryCache(const KURL&) final;

  virtual bool FetchClassicImportedScript(
      const KURL& script_url,
      KURL* out_response_url,
      String* out_source_code,
      std::unique_ptr<Vector<uint8_t>>* out_cached_meta_data);

  // Notifies that the top-level worker script is ready to evaluate.
  // Worker top-level script is evaluated after it is fetched and
  // ReadyToRunWorkerScript() is called.
  void ReadyToRunWorkerScript();

  void InitializeURL(const KURL& url);

  mojom::ScriptType GetScriptType() const { return script_type_; }

  // Sets the parameters for the worker main module script loaded by the browser
  // process.
  void SetWorkerMainScriptLoadingParametersForModules(
      std::unique_ptr<WorkerMainScriptLoadParameters>
          worker_main_script_load_params_for_modules);

 private:
  void SetWorkerSettings(std::unique_ptr<WorkerSettings>);

  // https://html.spec.whatwg.org/C/#run-a-worker Step 24.
  void RunWorkerScript();

  // Used for importScripts().
  void ImportScriptsInternal(const Vector<String>& urls);
  // ExecutionContext
  void AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr) final;
  EventTarget* ErrorEventTarget() final { return this; }

  KURL url_;
  const mojom::ScriptType script_type_;
  const String user_agent_;
  const UserAgentMetadata ua_metadata_;
  std::unique_ptr<WorkerSettings> worker_settings_;

  mutable Member<WorkerLocation> location_;
  mutable Member<WorkerNavigator> navigator_;
  mutable Member<TrustedTypePolicyFactory> trusted_types_;

  WorkerThread* thread_;

  bool closing_ = false;

  const base::TimeTicks time_origin_;

  HeapHashMap<int, Member<ErrorEvent>> pending_error_events_;
  int last_pending_error_event_id_ = 0;

  Member<OffscreenFontSelector> font_selector_;

  blink::BrowserInterfaceBrokerProxy browser_interface_broker_proxy_;

  // State transition about worker top-level script evaluation.
  enum class ScriptEvalState {
    // Initial state: ReadyToRunWorkerScript() was not yet called.
    // Worker top-level script fetch might or might not be completed, and even
    // when the fetch completes in this state, script evaluation will be
    // deferred to when ReadyToRunWorkerScript() is called later.
    kPauseAfterFetch,
    // ReadyToRunWorkerScript() was already called.
    kReadyToEvaluate,
    // The worker top-level script was evaluated.
    kEvaluated,
  };
  ScriptEvalState script_eval_state_;

  Member<Script> worker_script_;
  base::Optional<v8_inspector::V8StackTraceId> stack_id_;

  HttpsState https_state_;

  std::unique_ptr<ukm::UkmRecorder> ukm_recorder_;

  // |worker_main_script_load_params_for_modules_| is used to load a root module
  // script for dedicated workers (when PlzDedicatedWorker is enabled) and
  // shared workers.
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params_for_modules_;

  const ukm::SourceId ukm_source_id_;
};

template <>
struct DowncastTraits<WorkerGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsWorkerGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_GLOBAL_SCOPE_H_
