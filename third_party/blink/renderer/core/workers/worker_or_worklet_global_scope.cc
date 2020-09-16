// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"

#include "base/threading/thread_checker.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/content_security_notifier.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/loader_factory_for_worker.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/resource_load_observer_for_worker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/loader/worker_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/null_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// This is the implementation of ContentSecurityPolicyDelegate for the
// outsideSettings fetch.
// This lives on the worker thread.
//
// Unlike the ContentSecurityPolicy bound to ExecutionContext (which is
// used for insideSettings fetch), OutsideSettingsCSPDelegate shouldn't
// access WorkerOrWorkletGlobalScope (except for logging).
//
// For details of outsideSettings/insideSettings fetch, see README.md.
class OutsideSettingsCSPDelegate final
    : public GarbageCollected<OutsideSettingsCSPDelegate>,
      public ContentSecurityPolicyDelegate {
 public:
  OutsideSettingsCSPDelegate(
      const FetchClientSettingsObject& outside_settings_object,
      UseCounter& use_counter,
      WorkerOrWorkletGlobalScope& global_scope_for_logging)
      : outside_settings_object_(&outside_settings_object),
        use_counter_(use_counter),
        global_scope_for_logging_(&global_scope_for_logging) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(global_scope_for_logging_);
    visitor->Trace(use_counter_);
    visitor->Trace(outside_settings_object_);
  }

  const KURL& Url() const override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return outside_settings_object_->GlobalObjectUrl();
  }

  const SecurityOrigin* GetSecurityOrigin() override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return outside_settings_object_->GetSecurityOrigin();
  }

  // We don't have to do anything here, as we don't want to update
  // SecurityContext of either parent context or WorkerOrWorkletGlobalScope.
  void SetSandboxFlags(network::mojom::blink::WebSandboxFlags) override {}
  void SetRequireTrustedTypes() override {}
  void AddInsecureRequestPolicy(mojom::blink::InsecureRequestPolicy) override {}
  void DisableEval(const String& error_message) override {}

  std::unique_ptr<SourceLocation> GetSourceLocation() override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // https://w3c.github.io/webappsec-csp/#create-violation-for-global
    // Step 2. If the user agent is currently executing script, and can extract
    // a source file's URL, line number, and column number from the global, set
    // violation's source file, line number, and column number accordingly.
    // [spec text]
    //
    // We can assume the user agent is not executing script during fetching the
    // top-level worker script, so return nullptr.
    return nullptr;
  }

  base::Optional<uint16_t> GetStatusCode() override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // TODO(crbug/928965): Plumb the status code of the parent Document if any.
    return base::nullopt;
  }

  String GetDocumentReferrer() override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // TODO(crbug/928965): Plumb the referrer from the parent context.
    return String();
  }

  void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                              Element*) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // TODO(crbug/928964): Fire an event on the parent context.
    // Before OutsideSettingsCSPDelegate was introduced, the event had been
    // fired on WorkerGlobalScope, which had been virtually no-op because
    // there can't be no event handlers yet.
    // Currently, no events are fired.
  }

  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           const String& stringified_report,
                           bool is_frame_ancestors_violation,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // TODO(crbug/929370): Support POSTing violation reports from a Worker.
  }

  void Count(WebFeature feature) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    use_counter_->CountUse(feature);
  }

  void AddConsoleMessage(ConsoleMessage* message) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    global_scope_for_logging_->AddConsoleMessage(message);
  }

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) override {
    // This shouldn't be called during top-level worker script fetch.
    NOTREACHED();
  }

  void DidAddContentSecurityPolicies(
      WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    // We do nothing here, because if the added policies should be reported to
    // LocalFrameClient, then they are already reported on the parent
    // Document.
    // ExecutionContextCSPDelegate::DidAddContentSecurityPolicies() does
    // nothing for workers/worklets.
  }

  void AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr info) override {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    global_scope_for_logging_->AddInspectorIssue(std::move(info));
  }

 private:
  const Member<const FetchClientSettingsObject> outside_settings_object_;
  const Member<UseCounter> use_counter_;

  // |global_scope_for_logging_| should be used only for AddConsoleMessage() and
  // AddInspectorIssue().
  const Member<WorkerOrWorkletGlobalScope> global_scope_for_logging_;

  THREAD_CHECKER(worker_thread_checker_);
};

}  // namespace

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope(
    v8::Isolate* isolate,
    scoped_refptr<SecurityOrigin> origin,
    Agent* agent,
    const String& name,
    const base::UnguessableToken& parent_devtools_token,
    V8CacheOptions v8_cache_options,
    WorkerClients* worker_clients,
    std::unique_ptr<WebContentSettingsClient> content_settings_client,
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context,
    WorkerReportingProxy& reporting_proxy)
    : ExecutionContext(isolate, agent),
      name_(name),
      parent_devtools_token_(parent_devtools_token),
      worker_clients_(worker_clients),
      content_settings_client_(std::move(content_settings_client)),
      web_worker_fetch_context_(std::move(web_worker_fetch_context)),
      script_controller_(
          MakeGarbageCollected<WorkerOrWorkletScriptController>(this, isolate)),
      v8_cache_options_(v8_cache_options),
      reporting_proxy_(reporting_proxy) {
  GetSecurityContext().SetSecurityOrigin(std::move(origin));
  if (worker_clients_)
    worker_clients_->ReattachThread();
}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() = default;

// EventTarget
const AtomicString& WorkerOrWorkletGlobalScope::InterfaceName() const {
  NOTREACHED() << "Each global scope that uses events should define its own "
                  "interface name.";
  return g_null_atom;
}

v8::Local<v8::Value> WorkerOrWorkletGlobalScope::Wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context) {
  LOG(FATAL) << "WorkerOrWorkletGlobalScope must never be wrapped with wrap "
                "method. The global object of ECMAScript environment is used "
                "as the wrapper.";
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkerOrWorkletGlobalScope::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  LOG(FATAL) << "WorkerOrWorkletGlobalScope must never be wrapped with wrap "
                "method. The global object of ECMAScript environment is used "
                "as the wrapper.";
  return v8::Local<v8::Object>();
}

bool WorkerOrWorkletGlobalScope::HasPendingActivity() const {
  // The global scope wrapper is kept alive as longs as its execution context is
  // active.
  return !ExecutionContext::IsContextDestroyed();
}

void WorkerOrWorkletGlobalScope::CountUse(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(WebFeature::kNumberOfFeatures, feature);
  if (used_features_[static_cast<size_t>(feature)])
    return;
  used_features_.set(static_cast<size_t>(feature));
  ReportingProxy().CountFeature(feature);
}

ResourceLoadScheduler::ThrottleOptionOverride
WorkerOrWorkletGlobalScope::GetThrottleOptionOverride() const {
  return ResourceLoadScheduler::ThrottleOptionOverride::kNone;
}

void WorkerOrWorkletGlobalScope::UpdateFetcherThrottleOptionOverride() {
  if (inside_settings_resource_fetcher_) {
    inside_settings_resource_fetcher_->SetThrottleOptionOverride(
        GetThrottleOptionOverride());
  }
}

void WorkerOrWorkletGlobalScope::InitializeWebFetchContextIfNeeded() {
  if (web_fetch_context_initialized_)
    return;
  web_fetch_context_initialized_ = true;

  if (!web_worker_fetch_context_)
    return;

  DCHECK(!subresource_filter_);
  web_worker_fetch_context_->InitializeOnWorkerThread(navigator());
  std::unique_ptr<blink::WebDocumentSubresourceFilter> web_filter =
      web_worker_fetch_context_->TakeSubresourceFilter();
  if (web_filter) {
    subresource_filter_ =
        MakeGarbageCollected<SubresourceFilter>(this, std::move(web_filter));
  }
}

ResourceFetcher* WorkerOrWorkletGlobalScope::EnsureFetcher() {
  DCHECK(IsContextThread());
  // Worklets don't support subresource fetch.
  DCHECK(IsWorkerGlobalScope());

  if (inside_settings_resource_fetcher_)
    return inside_settings_resource_fetcher_;

  // Because CSP is initialized inside the WorkerGlobalScope or
  // WorkletGlobalScope constructor, GetContentSecurityPolicy() should be
  // non-null here.
  DCHECK(GetContentSecurityPolicy());

  auto* resource_timing_notifier =
      WorkerResourceTimingNotifierImpl::CreateForInsideResourceFetcher(*this);
  inside_settings_resource_fetcher_ = CreateFetcherInternal(
      *MakeGarbageCollected<FetchClientSettingsObjectImpl>(*this),
      *GetContentSecurityPolicy(), *resource_timing_notifier);
  return inside_settings_resource_fetcher_;
}

ResourceFetcher* WorkerOrWorkletGlobalScope::CreateFetcherInternal(
    const FetchClientSettingsObject& fetch_client_settings_object,
    ContentSecurityPolicy& content_security_policy,
    WorkerResourceTimingNotifier& resource_timing_notifier) {
  DCHECK(IsContextThread());
  InitializeWebFetchContextIfNeeded();
  ResourceFetcher* fetcher = nullptr;
  if (web_worker_fetch_context_) {
    auto& properties =
        *MakeGarbageCollected<DetachableResourceFetcherProperties>(
            *MakeGarbageCollected<WorkerResourceFetcherProperties>(
                *this, fetch_client_settings_object,
                web_worker_fetch_context_));
    auto* worker_fetch_context = MakeGarbageCollected<WorkerFetchContext>(
        properties, *this, web_worker_fetch_context_, subresource_filter_,
        content_security_policy, resource_timing_notifier);
    ResourceFetcherInit init(properties, worker_fetch_context,
                             GetTaskRunner(TaskType::kNetworking),
                             MakeGarbageCollected<LoaderFactoryForWorker>(
                                 *this, web_worker_fetch_context_),
                             this);
    init.use_counter = MakeGarbageCollected<DetachableUseCounter>(this);
    init.console_logger = MakeGarbageCollected<DetachableConsoleLogger>(this);

    // Potentially support throttling network requests from a worker.  Note,
    // this does not work currently for worklets, but worklets should not be
    // able to make network requests anyway.
    if (IsWorkerGlobalScope()) {
      init.frame_or_worker_scheduler = GetScheduler();

      // The only network requests possible from a worker are
      // RequestContext::FETCH which are not normally throttlable.
      // Possibly override this restriction so network from a throttled
      // worker will also be throttled.
      init.throttle_option_override = GetThrottleOptionOverride();
    }

    fetcher = MakeGarbageCollected<ResourceFetcher>(init);
    fetcher->SetResourceLoadObserver(
        MakeGarbageCollected<ResourceLoadObserverForWorker>(
            *probe::ToCoreProbeSink(static_cast<ExecutionContext*>(this)),
            fetcher->GetProperties(), *worker_fetch_context,
            GetDevToolsToken()));
  } else {
    auto& properties =
        *MakeGarbageCollected<DetachableResourceFetcherProperties>(
            *MakeGarbageCollected<NullResourceFetcherProperties>());
    // This code path is for unittests.
    fetcher = MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(properties, &FetchContext::NullInstance(),
                            GetTaskRunner(TaskType::kNetworking),
                            nullptr /* loader_factory */, this));
  }
  if (IsContextPaused())
    fetcher->SetDefersLoading(true);
  resource_fetchers_.insert(fetcher);
  return fetcher;
}

ResourceFetcher* WorkerOrWorkletGlobalScope::Fetcher() const {
  DCHECK(IsContextThread());
  // Worklets don't support subresource fetch.
  DCHECK(IsWorkerGlobalScope());
  DCHECK(inside_settings_resource_fetcher_);
  return inside_settings_resource_fetcher_;
}

ResourceFetcher* WorkerOrWorkletGlobalScope::CreateOutsideSettingsFetcher(
    const FetchClientSettingsObject& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier) {
  DCHECK(IsContextThread());

  auto* content_security_policy = MakeGarbageCollected<ContentSecurityPolicy>();
  content_security_policy->SetSupportsWasmEval(
      SchemeRegistry::SchemeSupportsWasmEvalCSP(
          outside_settings_object.GetSecurityOrigin()->Protocol()));
  for (const auto& policy_and_type : outside_content_security_policy_headers_) {
    content_security_policy->DidReceiveHeader(
        policy_and_type.first, policy_and_type.second,
        network::mojom::ContentSecurityPolicySource::kHTTP);
  }

  OutsideSettingsCSPDelegate* csp_delegate =
      MakeGarbageCollected<OutsideSettingsCSPDelegate>(outside_settings_object,
                                                       *this, *this);
  content_security_policy->BindToDelegate(*csp_delegate);

  return CreateFetcherInternal(outside_settings_object,
                               *content_security_policy,
                               outside_resource_timing_notifier);
}

bool WorkerOrWorkletGlobalScope::IsJSExecutionForbidden() const {
  return script_controller_->IsExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::DisableEval(const String& error_message) {
  script_controller_->DisableEval(error_message);
}

bool WorkerOrWorkletGlobalScope::CanExecuteScripts(
    ReasonForCallingCanExecuteScripts) {
  return !IsJSExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::Dispose() {
  DCHECK(script_controller_);

  RemoveAllEventListeners();

  script_controller_->Dispose();
  script_controller_.Clear();

  for (ResourceFetcher* resource_fetcher : resource_fetchers_) {
    resource_fetcher->StopFetching();
    resource_fetcher->ClearContext();
  }
}

void WorkerOrWorkletGlobalScope::SetModulator(Modulator* modulator) {
  modulator_ = modulator;
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerOrWorkletGlobalScope::GetTaskRunner(TaskType type) {
  DCHECK(IsContextThread());
  return GetThread()->GetTaskRunner(type);
}

void WorkerOrWorkletGlobalScope::ApplySandboxFlags(
    network::mojom::blink::WebSandboxFlags mask) {
  GetSecurityContext().ApplySandboxFlags(mask);
  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin) &&
      !GetSecurityOrigin()->IsOpaque()) {
    GetSecurityContext().SetSecurityOrigin(
        GetSecurityOrigin()->DeriveNewOpaqueOrigin());
  }
}

void WorkerOrWorkletGlobalScope::SetOutsideContentSecurityPolicyHeaders(
    const Vector<CSPHeaderAndType>& headers) {
  outside_content_security_policy_headers_ = headers;
}

void WorkerOrWorkletGlobalScope::InitContentSecurityPolicyFromVector(
    const Vector<CSPHeaderAndType>& headers) {
  if (!GetContentSecurityPolicy()) {
    auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->SetSupportsWasmEval(SchemeRegistry::SchemeSupportsWasmEvalCSP(
        GetSecurityOrigin()->Protocol()));
    GetSecurityContext().SetContentSecurityPolicy(csp);
  }
  for (const auto& policy_and_type : headers) {
    GetContentSecurityPolicy()->DidReceiveHeader(
        policy_and_type.first, policy_and_type.second,
        network::mojom::ContentSecurityPolicySource::kHTTP);
  }
}

void WorkerOrWorkletGlobalScope::BindContentSecurityPolicyToExecutionContext() {
  DCHECK(IsContextThread());
  GetContentSecurityPolicy()->BindToDelegate(
      GetContentSecurityPolicyDelegate());
}

// Implementation of the "fetch a module worker script graph" algorithm in the
// HTML spec:
// https://html.spec.whatwg.org/C/#fetch-a-module-worker-script-tree
void WorkerOrWorkletGlobalScope::FetchModuleScript(
    const KURL& module_url_record,
    const FetchClientSettingsObjectSnapshot& fetch_client_settings_object,
    WorkerResourceTimingNotifier& resource_timing_notifier,
    mojom::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    network::mojom::CredentialsMode credentials_mode,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client) {
  // Step 2: "Let options be a script fetch options whose cryptographic nonce is
  // the empty string,
  String nonce;
  // integrity metadata is the empty string,
  String integrity_attribute;
  // parser metadata is "not-parser-inserted,
  ParserDisposition parser_state = kNotParserInserted;

  RejectCoepUnsafeNone reject_coep_unsafe_none(false);
  if (ShouldRejectCoepUnsafeNoneTopModuleScript() &&
      destination == network::mojom::RequestDestination::kWorker) {
    DCHECK(!base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
    reject_coep_unsafe_none = RejectCoepUnsafeNone(true);
  }

  // credentials mode is credentials mode, and referrer policy is the empty
  // string."
  // TODO(domfarolino): Module worker scripts are fetched with kImportanceAuto.
  // Priority Hints is currently non-standard, but we can assume "fetch a module
  // worker script tree" sets the script fetch options struct's "importance" to
  // "auto". See https://github.com/whatwg/html/issues/3670 and
  // https://crbug.com/821464.
  ScriptFetchOptions options(
      nonce, IntegrityMetadataSet(), integrity_attribute, parser_state,
      credentials_mode, network::mojom::ReferrerPolicy::kDefault,
      mojom::FetchImportanceMode::kImportanceAuto, reject_coep_unsafe_none);

  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());
  // Step 3. "Perform the internal module script graph fetching procedure ..."
  modulator->FetchTree(
      module_url_record,
      CreateOutsideSettingsFetcher(fetch_client_settings_object,
                                   resource_timing_notifier),
      context_type, destination, options, custom_fetch_type, client);
}

void WorkerOrWorkletGlobalScope::SetDefersLoadingForResourceFetchers(
    bool defers) {
  for (ResourceFetcher* resource_fetcher : resource_fetchers_)
    resource_fetcher->SetDefersLoading(defers);
}

int WorkerOrWorkletGlobalScope::GetOutstandingThrottledLimit() const {
  // Default to what has been a typical throttle limit for iframes.  Note,
  // however, this value is largely meaningless unless the global has set
  // a ThrottleOptionOverride.  Workers can only make fetch/xhr requests
  // which are not throttlable by default.  If GetThrottleOptionOverride()
  // is overridden, then this method should also be overridden with a
  // more meaningful value.
  return 2;
}

CrossVariantMojoRemote<mojom::ResourceLoadInfoNotifierInterfaceBase>
WorkerOrWorkletGlobalScope::CloneResourceLoadInfoNotifier() {
  return web_worker_fetch_context_->CloneResourceLoadInfoNotifier();
}

void WorkerOrWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(inside_settings_resource_fetcher_);
  visitor->Trace(resource_fetchers_);
  visitor->Trace(subresource_filter_);
  visitor->Trace(script_controller_);
  visitor->Trace(modulator_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContext::Trace(visitor);
}

}  // namespace blink
