// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_fetch_client_settings_object.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_structured_serialize_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

DedicatedWorker* DedicatedWorker::Create(
    ExecutionContext* context,
    const V8UnionTrustedScriptURLOrUSVString* url,
    const WorkerOptions* options,
    ExceptionState& exception_state) {
  DCHECK(context->IsContextThread());
  UseCounter::Count(context, WebFeature::kWorkerStart);
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  String compliant_url = TrustedTypesCheckForScriptURL(
      url, context, trusted_types_names::kWorker,
      trusted_types_names::kConstructor, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  KURL script_request_url = ResolveURL(context, compliant_url, exception_state);
  if (!script_request_url.IsValid()) {
    // Don't throw an exception here because it's already thrown in
    // ResolveURL().
    return nullptr;
  }

  if (context->IsWorkerGlobalScope())
    UseCounter::Count(context, WebFeature::kNestedDedicatedWorker);

  DedicatedWorker* worker = MakeGarbageCollected<DedicatedWorker>(
      context, script_request_url, options);
  worker->UpdateStateIfNeeded();
  worker->Start();
  return worker;
}

DedicatedWorker::DedicatedWorker(ExecutionContext* context,
                                 const KURL& script_request_url,
                                 const WorkerOptions* options)
    : DedicatedWorker(
          context,
          script_request_url,
          options,
          [context](DedicatedWorker* worker) {
            return MakeGarbageCollected<DedicatedWorkerMessagingProxy>(context,
                                                                       worker);
          }) {}

DedicatedWorker::DedicatedWorker(
    ExecutionContext* context,
    const KURL& script_request_url,
    const WorkerOptions* options,
    base::FunctionRef<DedicatedWorkerMessagingProxy*(DedicatedWorker*)>
        context_proxy_factory)
    : AbstractWorker(context),
      ActiveScriptWrappable<DedicatedWorker>({}),
      script_request_url_(script_request_url),
      options_(options),
      context_proxy_(context_proxy_factory(this)),
      factory_client_(
          Platform::Current()->CreateDedicatedWorkerHostFactoryClient(
              this,
              GetExecutionContext()->GetBrowserInterfaceBroker())) {
  DCHECK(context->IsContextThread());
  DCHECK(script_request_url_.IsValid());
  DCHECK(context_proxy_);

  outside_fetch_client_settings_object_ =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          context->Fetcher()->GetProperties().GetFetchClientSettingsObject());
}

DedicatedWorker::~DedicatedWorker() = default;

void DedicatedWorker::Dispose() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  context_proxy_->ParentObjectDestroyed();
  factory_client_.reset();
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  const ScriptValue& message,
                                  HeapVector<ScriptObject> transfer,
                                  ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.empty())
    options->setTransfer(std::move(transfer));
  postMessage(script_state, message, options, exception_state);
}

void DedicatedWorker::postMessage(ScriptState* script_state,
                                  const ScriptValue& message,
                                  const PostMessageOptions* options,
                                  ExceptionState& exception_state) {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  if (!GetExecutionContext())
    return;

  BlinkTransferableMessage transferable_message;
  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);
  transferable_message.message = serialized_message;
  transferable_message.sender_origin =
      GetExecutionContext()->GetSecurityOrigin()->IsolatedCopy();

  // Disentangle the port in preparation for sending it to the remote context.
  transferable_message.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;
  transferable_message.user_activation =
      PostMessageHelper::CreateUserActivationSnapshot(GetExecutionContext(),
                                                      options);

  transferable_message.sender_stack_trace_id =
      ThreadDebugger::From(script_state->GetIsolate())
          ->StoreCurrentStackTrace("Worker.postMessage");
  uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  transferable_message.trace_id = trace_id;
  context_proxy_->PostMessageToWorkerGlobalScope(
      std::move(transferable_message));
  TRACE_EVENT_INSTANT(
      "devtools.timeline", "SchedulePostMessage", "data",
      [&](perfetto::TracedValue context) {
        inspector_schedule_post_message_event::Data(
            std::move(context), GetExecutionContext(), trace_id);
      },
      perfetto::Flow::Global(trace_id));  // SchedulePostMessage
}

void DedicatedWorker::PostCustomEvent(
    TaskType task_type,
    ScriptState* script_state,
    CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
        event_factory_callback,
    CrossThreadFunction<Event*(ScriptState*)> event_factory_error_callback,
    const ScriptValue& message,
    HeapVector<ScriptObject> transfer,
    ExceptionState& exception_state) {
  CHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  if (!GetExecutionContext()) {
    return;
  }

  StructuredSerializeOptions* options = StructuredSerializeOptions::Create();
  if (!transfer.empty()) {
    options->setTransfer(std::move(transfer));
  }
  CustomEventMessage transferable_message;
  Transferables transferables;

  if (!message.IsEmpty()) {
    scoped_refptr<SerializedScriptValue> serialized_message =
        PostMessageHelper::SerializeMessageByMove(
            script_state->GetIsolate(), message, options, transferables,
            exception_state);
    if (exception_state.HadException()) {
      return;
    }
    CHECK(serialized_message);
    transferable_message.message = serialized_message;
  }
  // Disentangle the port in preparation for sending it to the remote context.
  transferable_message.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  transferable_message.sender_stack_trace_id =
      ThreadDebugger::From(script_state->GetIsolate())
          ->StoreCurrentStackTrace("Worker.PostCustomEvent");
  uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  transferable_message.trace_id = trace_id;
  context_proxy_->PostCustomEventToWorkerGlobalScope(
      task_type, std::move(event_factory_callback),
      std::move(event_factory_error_callback), std::move(transferable_message));
  TRACE_EVENT_INSTANT(
      "devtools.timeline", "SchedulePostCustomEvent", "data",
      [&](perfetto::TracedValue context) {
        inspector_schedule_post_message_event::Data(
            std::move(context), GetExecutionContext(), trace_id);
      },
      perfetto::Flow::Global(trace_id));  // SchedulePostCustomEvent
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void DedicatedWorker::Start() {
  TRACE_EVENT("blink.worker", "DedicatedWorker::Start");
  DCHECK(GetExecutionContext()->IsContextThread());

  if (!CheckAllowedByCSPForNoThrow(script_request_url_)) {
    // The same as in OnScriptLoadStartFailed, reset factory_client_ and return.
    // This leaves the worker in a state the same as if script loading failed.
    factory_client_.reset();
    context_proxy_->DidFailToFetchScript();
    return;
  }

  start_time_ = base::TimeTicks::Now();

  // This needs to be done after the UpdateStateIfNeeded is called as
  // calling into the debugger can cause a breakpoint.
  v8_stack_trace_id_ = ThreadDebugger::From(GetExecutionContext()->GetIsolate())
                           ->StoreCurrentStackTrace("Worker Created");

  // For classic script, always use "same-origin" credentials mode.
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  // For module script, respect the credentials mode specified by
  // WorkerOptions.
  // https://html.spec.whatwg.org/C/#workeroptions
  auto credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  if (options_->type() == V8WorkerType::Enum::kModule) {
    credentials_mode = Request::V8RequestCredentialsToCredentialsMode(
        options_->credentials().AsEnum());
  }

  mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token;
  if (script_request_url_.ProtocolIs("blob")) {
    GetExecutionContext()->GetPublicURLManager().ResolveAsBlobURLToken(
        script_request_url_, blob_url_token.InitWithNewPipeAndPassReceiver(),
        /*is_top_level_navigation=*/false);
  }

  if (script_request_url_.ProtocolIs("data")) {
    GetExecutionContext()->CountUse(WebFeature::kDataUrlDedicatedWorker);
  }

  factory_client_->CreateWorkerHost(
      token_, script_request_url_, credentials_mode,
      WebFetchClientSettingsObject(*outside_fetch_client_settings_object_),
      std::move(blob_url_token),
      GetExecutionContext()->GetStorageAccessApiStatus());
  // Continue in OnScriptLoadStarted() or OnScriptLoadStartFailed().
}

void DedicatedWorker::terminate() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  context_proxy_->TerminateGlobalScope();
}

void DedicatedWorker::ContextDestroyed() {
  DCHECK(GetExecutionContext()->IsContextThread());
  if (classic_script_loader_)
    classic_script_loader_->Cancel();
  factory_client_.reset();
  terminate();
}

bool DedicatedWorker::HasPendingActivity() const {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  // The worker context does not exist while loading, so we must ensure that the
  // worker object is not collected, nor are its event listeners.
  return context_proxy_->HasPendingActivity() || classic_script_loader_;
}

void DedicatedWorker::OnWorkerHostCreated(
    CrossVariantMojoRemote<mojom::blink::BrowserInterfaceBrokerInterfaceBase>
        browser_interface_broker,
    CrossVariantMojoRemote<mojom::blink::DedicatedWorkerHostInterfaceBase>
        dedicated_worker_host,
    const WebSecurityOrigin& origin) {
  TRACE_EVENT("blink.worker", "DedicatedWorker::OnWorkerHostCreated");
  base::UmaHistogramTimes("Worker.TopLevelScript.WorkerHostCreatedTime",
                          base::TimeTicks::Now() - start_time_);
  DCHECK(!browser_interface_broker_);
  browser_interface_broker_ = std::move(browser_interface_broker);
  pending_dedicated_worker_host_ = std::move(dedicated_worker_host);
  origin_ = blink::SecurityOrigin::CreateFromUrlOrigin(url::Origin(origin));
}

void DedicatedWorker::OnScriptLoadStarted(
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    CrossVariantMojoRemote<
        mojom::blink::BackForwardCacheControllerHostInterfaceBase>
        back_forward_cache_controller_host,
    CrossVariantMojoReceiver<mojom::blink::ReportingObserverInterfaceBase>
        coep_reporting_observer,
    CrossVariantMojoReceiver<mojom::blink::ReportingObserverInterfaceBase>
        dip_reporting_observer) {
  TRACE_EVENT("blink.worker", "DedicatedWorker::OnScriptLoadStarted");
  // Specify empty source code here because scripts will be fetched on the
  // worker thread.
  ContinueStart(script_request_url_, std::move(worker_main_script_load_params),
                network::mojom::ReferrerPolicy::kDefault,
                Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
                std::move(back_forward_cache_controller_host),
                std::move(coep_reporting_observer),
                std::move(dip_reporting_observer));
}

void DedicatedWorker::OnScriptLoadStartFailed() {
  TRACE_EVENT("blink.worker", "DedicatedWorker::OnScriptLoadStartFailed");
  // Specify empty source code here because scripts will be fetched on the
  context_proxy_->DidFailToFetchScript();
  factory_client_.reset();
}

void DedicatedWorker::DispatchErrorEventForScriptFetchFailure() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  // TODO(nhiroki): Add a console error message.
  DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
}

std::unique_ptr<WebContentSettingsClient>
DedicatedWorker::CreateWebContentSettingsClient() {
  std::unique_ptr<WebContentSettingsClient> content_settings_client;
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    return window->GetFrame()->Client()->CreateWorkerContentSettingsClient();
  } else if (GetExecutionContext()->IsWorkerGlobalScope()) {
    WebContentSettingsClient* web_worker_content_settings_client =
        To<WorkerGlobalScope>(GetExecutionContext())->ContentSettingsClient();
    if (web_worker_content_settings_client)
      return web_worker_content_settings_client->Clone();
  }
  return nullptr;
}

void DedicatedWorker::OnResponse() {
  DCHECK(GetExecutionContext()->IsContextThread());
  probe::DidReceiveScriptResponse(GetExecutionContext(),
                                  classic_script_loader_->Identifier());
}

void DedicatedWorker::OnFinished(
    mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>
        back_forward_cache_controller_host) {
  DCHECK(GetExecutionContext()->IsContextThread());
  TRACE_EVENT("blink.worker", "DedicatedWorker::OnFinished");
  if (classic_script_loader_->Canceled()) {
    // Do nothing.
  } else if (classic_script_loader_->Failed()) {
    context_proxy_->DidFailToFetchScript();
  } else {
    network::mojom::ReferrerPolicy referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    if (!classic_script_loader_->GetReferrerPolicy().IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          classic_script_loader_->GetReferrerPolicy(),
          kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    }
    const KURL script_response_url = classic_script_loader_->ResponseURL();
    DCHECK(script_request_url_ == script_response_url ||
           SecurityOrigin::AreSameOrigin(script_request_url_,
                                         script_response_url));
    ContinueStart(
        script_response_url, nullptr /* worker_main_script_load_params */,
        referrer_policy,
        classic_script_loader_->GetContentSecurityPolicy()
            ? mojo::Clone(classic_script_loader_->GetContentSecurityPolicy()
                              ->GetParsedPolicies())
            : Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        std::move(back_forward_cache_controller_host),
        /*coep_reporting_observer=*/mojo::NullReceiver(),
        /*dip_reporting_observer=*/mojo::NullReceiver());
    probe::ScriptImported(GetExecutionContext(),
                          classic_script_loader_->Identifier(),
                          classic_script_loader_->SourceText());
  }
  classic_script_loader_ = nullptr;
}

void DedicatedWorker::ContinueStart(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    network::mojom::ReferrerPolicy referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr>
        response_content_security_policies,
    mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>
        back_forward_cache_controller_host,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        coep_reporting_observer,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        dip_reporting_observer) {
  UMA_HISTOGRAM_TIMES("Worker.TopLevelScript.LoadStartedTime",
                      base::TimeTicks::Now() - start_time_);
  TRACE_EVENT("blink.worker", "DedicatedWorker::ContinueStart");
  if (base::FeatureList::IsEnabled(
          features::kDedicatedWorkerAblationStudyEnabled)) {
    CHECK(GetExecutionContext());
    TRACE_EVENT("blink.worker", "DedicatedWorkerAblationStudyEnabled",
                "DedicatedWorkerStartDelayInMs",
                features::kDedicatedWorkerStartDelayInMs.Get());
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kInternalDefault)
        ->PostDelayedTask(
            FROM_HERE,
            BindOnce(&DedicatedWorker::ContinueStartInternal,
                     WrapWeakPersistent(this), script_url,
                     std::move(worker_main_script_load_params),
                     std::move(referrer_policy),
                     std::move(response_content_security_policies),
                     std::move(back_forward_cache_controller_host),
                     std::move(coep_reporting_observer),
                     std::move(dip_reporting_observer)),
            base::Milliseconds(features::kDedicatedWorkerStartDelayInMs.Get()));
    return;
  }
  ContinueStartInternal(
      script_url, std::move(worker_main_script_load_params),
      std::move(referrer_policy), std::move(response_content_security_policies),
      std::move(back_forward_cache_controller_host),
      std::move(coep_reporting_observer), std::move(dip_reporting_observer));
}

void DedicatedWorker::ContinueStartInternal(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    network::mojom::ReferrerPolicy referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr>
        response_content_security_policies,
    mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>
        back_forward_cache_controller_host,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        coep_reporting_observer,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        dip_reporting_observer) {
  TRACE_EVENT("blink.worker", "DedicatedWorker::ContinueStartInternal");
  if (!GetExecutionContext()) {
    return;
  }
  context_proxy_->StartWorkerGlobalScope(
      CreateGlobalScopeCreationParams(
          script_url, referrer_policy,
          std::move(response_content_security_policies),
          std::move(coep_reporting_observer),
          std::move(dip_reporting_observer)),
      std::move(worker_main_script_load_params), options_, script_url,
      *outside_fetch_client_settings_object_, v8_stack_trace_id_, token_,
      std::move(pending_dedicated_worker_host_),
      std::move(back_forward_cache_controller_host));
}

namespace {

BeginFrameProviderParams CreateBeginFrameProviderParams(
    ExecutionContext& execution_context) {
  DCHECK(execution_context.IsContextThread());
  // If we don't have a frame or we are not in window, some of the SinkIds
  // won't be initialized. If that's the case, the Worker will initialize it by
  // itself later.
  BeginFrameProviderParams begin_frame_provider_params;
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    auto* web_local_frame = WebLocalFrameImpl::FromFrame(window->GetFrame());
    if (web_local_frame) {
      WebFrameWidgetImpl* widget = web_local_frame->LocalRootFrameWidget();
      begin_frame_provider_params.parent_frame_sink_id =
          widget->GetFrameSinkId();
    }
    begin_frame_provider_params.frame_sink_id =
        Platform::Current()->GenerateFrameSinkId();
  }
  return begin_frame_provider_params;
}

}  // namespace

std::unique_ptr<GlobalScopeCreationParams>
DedicatedWorker::CreateGlobalScopeCreationParams(
    const KURL& script_url,
    network::mojom::ReferrerPolicy referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr>
        response_content_security_policies,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        coep_reporting_observer,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        dip_reporting_observer) {
  base::UnguessableToken parent_devtools_token;
  std::unique_ptr<WorkerSettings> settings;
  ExecutionContext* execution_context = GetExecutionContext();
  scoped_refptr<base::SingleThreadTaskRunner>
      agent_group_scheduler_compositor_task_runner;
  const SecurityOrigin* top_level_frame_security_origin;

  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    // When the main thread creates a new DedicatedWorker.
    auto* frame = window->GetFrame();
    parent_devtools_token = frame->GetDevToolsFrameToken();
    settings = std::make_unique<WorkerSettings>(frame->GetSettings());
    agent_group_scheduler_compositor_task_runner =
        execution_context->GetScheduler()
            ->ToFrameScheduler()
            ->GetAgentGroupScheduler()
            ->CompositorTaskRunner();
    top_level_frame_security_origin =
        window->GetFrame()->Top()->GetSecurityContext()->GetSecurityOrigin();
  } else {
    // When a DedicatedWorker creates another DedicatedWorker (nested worker).
    WorkerGlobalScope* worker_global_scope =
        To<WorkerGlobalScope>(execution_context);
    parent_devtools_token =
        worker_global_scope->GetThread()->GetDevToolsWorkerToken();
    settings = WorkerSettings::Copy(worker_global_scope->GetWorkerSettings());
    agent_group_scheduler_compositor_task_runner =
        worker_global_scope->GetAgentGroupSchedulerCompositorTaskRunner();
    top_level_frame_security_origin =
        worker_global_scope->top_level_frame_security_origin();
  }
  DCHECK(agent_group_scheduler_compositor_task_runner);
  DCHECK(top_level_frame_security_origin);

  mojom::blink::ScriptType script_type =
      (options_->type() == V8WorkerType::Enum::kClassic)
          ? mojom::blink::ScriptType::kClassic
          : mojom::blink::ScriptType::kModule;

  auto params = std::make_unique<GlobalScopeCreationParams>(
      script_url, script_type, options_->name(), execution_context->UserAgent(),
      execution_context->GetUserAgentMetadata(), CreateWebWorkerFetchContext(),
      mojo::Clone(
          execution_context->GetContentSecurityPolicy()->GetParsedPolicies()),
      std::move(response_content_security_policies), referrer_policy,
      execution_context->GetSecurityOrigin(),
      execution_context->IsSecureContext(), execution_context->GetHttpsState(),
      MakeGarbageCollected<WorkerClients>(), CreateWebContentSettingsClient(),
      OriginTrialContext::GetInheritedTrialFeatures(execution_context).get(),
      parent_devtools_token, std::move(settings),
      mojom::blink::V8CacheOptions::kDefault,
      nullptr /* worklet_module_responses_map */,
      std::move(browser_interface_broker_),
      mojo::NullRemote() /* code_cache_host_interface */,
      mojo::NullRemote() /* blob_url_store */,
      CreateBeginFrameProviderParams(*execution_context),
      execution_context->GetSecurityContext().GetPermissionsPolicy(),
      execution_context->GetAgentClusterID(), execution_context->UkmSourceID(),
      execution_context->GetExecutionContextToken(),
      execution_context->CrossOriginIsolatedCapability(),
      execution_context->IsIsolatedContext(),
      /*interface_registry=*/nullptr,
      std::move(agent_group_scheduler_compositor_task_runner),
      top_level_frame_security_origin,
      execution_context->GetStorageAccessApiStatus(),
      /*require_cross_site_request_for_cookies=*/false,
      origin_ ? origin_->IsolatedCopy() : nullptr,
      std::move(coep_reporting_observer), std::move(dip_reporting_observer));
  params->dedicated_worker_start_time = start_time_;
  return params;
}

scoped_refptr<WebWorkerFetchContext>
DedicatedWorker::CreateWebWorkerFetchContext() {
  // This worker is being created by the window.
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context;
    LocalFrame* frame = window->GetFrame();
    web_worker_fetch_context =
        frame->Client()->CreateWorkerFetchContext(factory_client_.get());
    web_worker_fetch_context->SetIsOnSubframe(!frame->IsOutermostMainFrame());
    return web_worker_fetch_context;
  }

  // This worker is being created by an existing worker (i.e., nested workers).
  // Clone the worker fetch context from the parent's one.
  auto* scope = To<WorkerGlobalScope>(GetExecutionContext());
  auto& worker_fetch_context =
      static_cast<WorkerFetchContext&>(scope->Fetcher()->Context());

  return factory_client_->CloneWorkerFetchContext(
      worker_fetch_context.GetWebWorkerFetchContext(),
      scope->GetTaskRunner(TaskType::kNetworking));
}

const AtomicString& DedicatedWorker::InterfaceName() const {
  return event_target_names::kWorker;
}

void DedicatedWorker::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  DCHECK(GetExecutionContext()->IsContextThread());
  switch (state) {
    case mojom::blink::FrameLifecycleState::kPaused:
      // Do not do anything in this case. kPaused is only used
      // for when the main thread is paused we shouldn't worry
      // about pausing the worker thread in this case.
      break;
    case mojom::blink::FrameLifecycleState::kFrozen:
      if (!requested_frozen_) {
        requested_frozen_ = true;
        context_proxy_->Freeze(
            GetExecutionContext()->is_in_back_forward_cache());
      }
      break;
    case mojom::blink::FrameLifecycleState::kRunning:
      if (requested_frozen_) {
        context_proxy_->Resume();
        requested_frozen_ = false;
      }
      break;
  }
}

void DedicatedWorker::Trace(Visitor* visitor) const {
  visitor->Trace(options_);
  visitor->Trace(outside_fetch_client_settings_object_);
  visitor->Trace(context_proxy_);
  visitor->Trace(classic_script_loader_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
