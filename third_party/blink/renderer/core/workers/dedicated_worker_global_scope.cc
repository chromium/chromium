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

#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"

#include <memory>
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

// static
DedicatedWorkerGlobalScope* DedicatedWorkerGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    DedicatedWorkerThread* thread,
    base::TimeTicks time_origin) {
  std::unique_ptr<Vector<String>> outside_origin_trial_tokens =
      std::move(creation_params->origin_trial_tokens);
  BeginFrameProviderParams begin_frame_provider_params =
      creation_params->begin_frame_provider_params;

  // Off-the-main-thread worker script fetch:
  // Initialize() is called after script fetch.
  if (creation_params->off_main_thread_fetch_option ==
      OffMainThreadWorkerScriptFetchOption::kEnabled) {
    return MakeGarbageCollected<DedicatedWorkerGlobalScope>(
        std::move(creation_params), thread, time_origin,
        std::move(outside_origin_trial_tokens), begin_frame_provider_params);
  }

  // Legacy on-the-main-thread worker script fetch (to be removed):
  KURL response_script_url = creation_params->script_url;
  network::mojom::ReferrerPolicy response_referrer_policy =
      creation_params->referrer_policy;
  network::mojom::IPAddressSpace response_address_space =
      *creation_params->response_address_space;
  auto* global_scope = MakeGarbageCollected<DedicatedWorkerGlobalScope>(
      std::move(creation_params), thread, time_origin,
      std::move(outside_origin_trial_tokens), begin_frame_provider_params);
  // Pass dummy CSP headers here as it is superseded by outside's CSP headers in
  // Initialize().
  // Pass dummy origin trial tokens here as it is already set to outside's
  // origin trial tokens in DedicatedWorkerGlobalScope's constructor.
  // Pass kAppCacheNoCacheId here as on-the-main-thread script fetch doesn't
  // have its own appcache and instead depends on the parent frame's one.
  global_scope->Initialize(response_script_url, response_referrer_policy,
                           response_address_space, Vector<CSPHeaderAndType>(),
                           nullptr /* response_origin_trial_tokens */,
                           mojom::blink::kAppCacheNoCacheId);
  return global_scope;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    DedicatedWorkerThread* thread,
    base::TimeTicks time_origin,
    std::unique_ptr<Vector<String>> outside_origin_trial_tokens,
    const BeginFrameProviderParams& begin_frame_provider_params)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin),
      animation_frame_provider_(
          MakeGarbageCollected<WorkerAnimationFrameProvider>(
              this,
              begin_frame_provider_params)) {
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(*this);

  // Dedicated workers don't need to pause after script fetch.
  ReadyToRunWorkerScript();
  // Inherit the outside's origin trial tokens.
  OriginTrialContext::AddTokens(this, outside_origin_trial_tokens.get());
}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() = default;

const AtomicString& DedicatedWorkerGlobalScope::InterfaceName() const {
  return event_target_names::kDedicatedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorkerGlobalScope::Initialize(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    network::mojom::IPAddressSpace response_address_space,
    const Vector<CSPHeaderAndType>& /* response_csp_headers */,
    const Vector<String>* /* response_origin_trial_tokens */,
    int64_t appcache_id) {
  // Step 12.3. "Set worker global scope's url to response's url."
  InitializeURL(response_url);

  // Step 12.4. "Set worker global scope's HTTPS state to response's HTTPS
  // state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 12.5. "Set worker global scope's referrer policy to the result of
  // parsing the `Referrer-Policy` header of response."
  SetReferrerPolicy(response_referrer_policy);

  // https://wicg.github.io/cors-rfc1918/#integration-html
  SetAddressSpace(response_address_space);

  // Step 12.6. "Execute the Initialize a global object's CSP list algorithm
  // on worker global scope and response. [CSP]"
  // DedicatedWorkerGlobalScope inherits the outside's CSP instead of the
  // response CSP headers. These should be called after SetAddressSpace() to
  // correctly override the address space by the "treat-as-public-address" CSP
  // directive.
  InitContentSecurityPolicyFromVector(OutsideContentSecurityPolicyHeaders());
  BindContentSecurityPolicyToExecutionContext();

  // This should be called after OriginTrialContext::AddTokens() to install
  // origin trial features in JavaScript's global object.
  // DedicatedWorkerGlobalScope inherits the outside's OriginTrialTokens in the
  // constructor instead of the response origin trial tokens.
  ScriptController()->PrepareForEvaluation();

  // TODO(https://crbug.com/945673): Notify an application cache host of
  // |appcache_id| here to support AppCache with PlzDedicatedWorker.
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorkerGlobalScope::FetchAndRunClassicScript(
    const KURL& script_url,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
  DCHECK(!IsContextPaused());

  // Step 12. "Fetch a classic worker script given url, outside settings,
  // destination, and inside settings."
  auto destination = mojom::RequestContextType::WORKER;

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
      script_url, destination, network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      WTF::Bind(&DedicatedWorkerGlobalScope::DidReceiveResponseForClassicScript,
                WrapWeakPersistent(this),
                WrapPersistent(classic_script_loader)),
      WTF::Bind(&DedicatedWorkerGlobalScope::DidFetchClassicScript,
                WrapWeakPersistent(this), WrapPersistent(classic_script_loader),
                stack_id));
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorkerGlobalScope::FetchAndRunModuleScript(
    const KURL& module_url_record,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode) {
  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."
  mojom::RequestContextType destination = mojom::RequestContextType::WORKER;

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."
  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier, destination,
                    credentials_mode,
                    ModuleScriptCustomFetchType::kWorkerConstructor,
                    MakeGarbageCollected<WorkerModuleTreeClient>(
                        ScriptController()->GetScriptState()));
}

const String DedicatedWorkerGlobalScope::name() const {
  return Name();
}

void DedicatedWorkerGlobalScope::postMessage(ScriptState* script_state,
                                             const ScriptValue& message,
                                             HeapVector<ScriptValue>& transfer,
                                             ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void DedicatedWorkerGlobalScope::postMessage(ScriptState* script_state,
                                             const ScriptValue& message,
                                             const PostMessageOptions* options,
                                             ExceptionState& exception_state) {
  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);
  BlinkTransferableMessage transferable_message;
  transferable_message.message = serialized_message;
  transferable_message.sender_origin =
      GetExecutionContext()->GetSecurityOrigin()->IsolatedCopy();
  // Disentangle the port in preparation for sending it to the remote context.
  transferable_message.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(script_state->GetIsolate());
  transferable_message.sender_stack_trace_id =
      debugger->StoreCurrentStackTrace("postMessage");
  WorkerObjectProxy().PostMessageToWorkerObject(
      std::move(transferable_message));
}

void DedicatedWorkerGlobalScope::DidReceiveResponseForClassicScript(
    WorkerClassicScriptLoader* classic_script_loader) {
  DCHECK(IsContextThread());
  DCHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
  probe::DidReceiveScriptResponse(this, classic_script_loader->Identifier());
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorkerGlobalScope::DidFetchClassicScript(
    WorkerClassicScriptLoader* classic_script_loader,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());
  DCHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));

  // Step 12. "If the algorithm asynchronously completes with null, then:"
  if (classic_script_loader->Failed()) {
    // Step 12.1. "Queue a task to fire an event named error at worker."
    // DidFailToFetchClassicScript() will asynchronously fire the event.
    ReportingProxy().DidFailToFetchClassicScript();

    // Step 12.2. "Run the environment discarding steps for inside settings."
    // Do nothing because the HTML spec doesn't define these steps for web
    // workers.

    // Schedule worker termination.
    close();

    // Step 12.3. "Return."
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
  // Pass dummy CSP headers here as it is superseded by outside's CSP headers in
  // Initialize().
  // Pass dummy origin trial tokens here as it is already set to outside's
  // origin trial tokens in DedicatedWorkerGlobalScope's constructor.
  Initialize(classic_script_loader->ResponseURL(), response_referrer_policy,
             classic_script_loader->ResponseAddressSpace(),
             Vector<CSPHeaderAndType>(),
             nullptr /* response_origin_trial_tokens */,
             classic_script_loader->AppCacheID());

  // Step 12.7. "Asynchronously complete the perform the fetch steps with
  // response."
  EvaluateClassicScript(
      classic_script_loader->ResponseURL(), classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

int DedicatedWorkerGlobalScope::requestAnimationFrame(
    V8FrameRequestCallback* callback,
    ExceptionState& exception_state) {
  auto* frame_callback =
      MakeGarbageCollected<FrameRequestCallbackCollection::V8FrameCallback>(
          callback);
  frame_callback->SetUseLegacyTimeBase(false);

  int ret = animation_frame_provider_->RegisterCallback(frame_callback);

  if (ret == WorkerAnimationFrameProvider::kInvalidCallbackId) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "requestAnimationFrame not supported in this Worker.");
  }

  return ret;
}

void DedicatedWorkerGlobalScope::cancelAnimationFrame(int id) {
  animation_frame_provider_->CancelCallback(id);
}

DedicatedWorkerObjectProxy& DedicatedWorkerGlobalScope::WorkerObjectProxy()
    const {
  return static_cast<DedicatedWorkerThread*>(GetThread())->WorkerObjectProxy();
}

void DedicatedWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(animation_frame_provider_);
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
