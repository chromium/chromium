/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"

#include <memory>
#include "base/feature_list.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    SharedWorkerThread* thread,
    base::TimeTicks time_origin,
    const SharedWorkerToken& token,
    bool require_cross_site_request_for_cookies)
    : WorkerGlobalScope(std::move(creation_params),
                        thread,
                        time_origin,
                        /*is_service_worker_global_scope=*/false),
      token_(token),
      require_cross_site_request_for_cookies_(
          require_cross_site_request_for_cookies) {}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope() = default;

const AtomicString& SharedWorkerGlobalScope::InterfaceName() const {
  return event_target_names::kSharedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::Initialize(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> response_csp,
    const Vector<String>* response_origin_trial_tokens) {
  // Step 12.3. "Set worker global scope's url to response's url."
  InitializeURL(response_url);

  // Step 12.4. "Set worker global scope's HTTPS state to response's HTTPS
  // state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 12.5. "Set worker global scope's referrer policy to the result of
  // parsing the `Referrer-Policy` header of response."
  SetReferrerPolicy(response_referrer_policy);

  // Step 12.6. "Execute the Initialize a global object's CSP list algorithm
  // on worker global scope and response. [CSP]"
  // SharedWorkerGlobalScope inherits the outside's CSP instead of the response
  // CSP headers when the response's url's scheme is a local scheme. Otherwise,
  // use the response CSP headers. Here a local scheme is defined as follows:
  // "A local scheme is a scheme that is "about", "blob", or "data"."
  // https://fetch.spec.whatwg.org/#local-scheme
  //
  // https://w3c.github.io/webappsec-csp/#initialize-global-object-csp
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> csp_headers =
      response_url.ProtocolIsAbout() || response_url.ProtocolIsData() ||
              response_url.ProtocolIs("blob")
          ? mojo::Clone(OutsideContentSecurityPolicies())
          : std::move(response_csp);
  InitContentSecurityPolicyFromVector(std::move(csp_headers));
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

  // This should be called after OriginTrialContext::AddTokens() to install
  // origin trial features in JavaScript's global object.
  ScriptController()->PrepareForEvaluation();

  ReadyToRunWorkerScript();
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::FetchAndRunClassicScript(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<PolicyContainer> policy_container,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(!IsContextPaused());

  SetPolicyContainer(std::move(policy_container));

  // Step 12. "Fetch a classic worker script given url, outside settings,
  // destination, and inside settings."
  auto context_type = mojom::blink::RequestContextType::SHARED_WORKER;
  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kSharedWorker;

  // Step 12.1. "Set request's reserved client to inside settings."
  // The browesr process takes care of this.

  // Step 12.2. "Fetch request, and asynchronously wait to run the remaining
  // steps as part of fetch's process response for the response response."
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  classic_script_loader->LoadTopLevelScriptAsynchronously(
      *this,
      CreateOutsideSettingsFetcher(outside_settings_object,
                                   outside_resource_timing_notifier),
      script_url, std::move(worker_main_script_load_params), context_type,
      destination, network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      WTF::BindOnce(
          &SharedWorkerGlobalScope::DidReceiveResponseForClassicScript,
          WrapWeakPersistent(this), WrapPersistent(classic_script_loader)),
      WTF::BindOnce(&SharedWorkerGlobalScope::DidFetchClassicScript,
                    WrapWeakPersistent(this),
                    WrapPersistent(classic_script_loader), stack_id));
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::FetchAndRunModuleScript(
    const KURL& module_url_record,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<PolicyContainer> policy_container,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    RejectCoepUnsafeNone reject_coep_unsafe_none) {
  DCHECK(!reject_coep_unsafe_none);
  if (worker_main_script_load_params) {
    SetWorkerMainScriptLoadingParametersForModules(
        std::move(worker_main_script_load_params));
  }
  SetPolicyContainer(std::move(policy_container));

  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."
  auto context_type = mojom::blink::RequestContextType::SHARED_WORKER;
  auto destination = network::mojom::RequestDestination::kSharedWorker;

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."
  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier, context_type, destination,
                    credentials_mode,
                    ModuleScriptCustomFetchType::kWorkerConstructor,
                    MakeGarbageCollected<WorkerModuleTreeClient>(
                        ScriptController()->GetScriptState()));
}

const String SharedWorkerGlobalScope::name() const {
  return Name();
}

void SharedWorkerGlobalScope::Connect(MessagePortChannel channel) {
  DCHECK(!IsContextPaused());
  auto* port = MakeGarbageCollected<MessagePort>(*this);
  port->Entangle(std::move(channel));
  MessageEvent* event =
      MessageEvent::Create(MakeGarbageCollected<MessagePortArray>(1, port),
                           String(), String(), port);
  event->initEvent(event_type_names::kConnect, false, false);
  DispatchEvent(*event);
}

void SharedWorkerGlobalScope::DidReceiveResponseForClassicScript(
    WorkerClassicScriptLoader* classic_script_loader) {
  DCHECK(IsContextThread());
  probe::DidReceiveScriptResponse(this, classic_script_loader->Identifier());
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::DidFetchClassicScript(
    WorkerClassicScriptLoader* classic_script_loader,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());

  // Step 12. "If the algorithm asynchronously completes with null or with
  // script whose error to rethrow is non-null, then:"
  //
  // The case |error to rethrow| is non-null indicates the parse error.
  // Parsing the script should be done during fetching according to the spec
  // but it is done in EvaluateClassicScript() for classic scripts.
  // Therefore, we cannot catch parse error events here.
  // TODO(https://crbug.com/1058259) Catch parse error events for classic
  // shared workers.
  if (classic_script_loader->Failed()) {
    // Step 12.1. "Queue a task to fire an event named error at worker."
    // Step 12.2. "Run the environment discarding steps for inside settings."
    // Step 12.3. "Return."
    ReportingProxy().DidFailToFetchClassicScript();
    return;
  }
  ReportingProxy().DidFetchScript();
  probe::ScriptImported(this, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());

  auto response_referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!classic_script_loader->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        classic_script_loader->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
  }

  // Step 12.3-12.6 are implemented in Initialize().
  Initialize(classic_script_loader->ResponseURL(), response_referrer_policy,
             classic_script_loader->GetContentSecurityPolicy()
                 ? mojo::Clone(classic_script_loader->GetContentSecurityPolicy()
                                   ->GetParsedPolicies())
                 : Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
             classic_script_loader->OriginTrialTokens());

  // Step 12.7. "Asynchronously complete the perform the fetch steps with
  // response."
  EvaluateClassicScript(
      classic_script_loader->ResponseURL(), classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

void SharedWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

void SharedWorkerGlobalScope::Trace(Visitor* visitor) const {
  WorkerGlobalScope::Trace(visitor);
}

bool SharedWorkerGlobalScope::CrossOriginIsolatedCapability() const {
  return Agent::IsCrossOriginIsolated();
}

bool SharedWorkerGlobalScope::IsIsolatedContext() const {
  return Agent::IsIsolatedContext();
}

}  // namespace blink
