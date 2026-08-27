// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "services/webnn/public/cpp/in_process_context_provider.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#if BUILDFLAG(USE_DAWN)
#include "third_party/dawn/include/dawn/wire/WireClient.h"
#endif

namespace blink {

namespace {

webnn::mojom::blink::Device ConvertBlinkDeviceTypeToMojo(
    const V8MLDeviceType& device_type_blink) {
  switch (device_type_blink.AsEnum()) {
    case V8MLDeviceType::Enum::kCpu:
      return webnn::mojom::blink::Device::kCpu;
    case V8MLDeviceType::Enum::kGpu:
      return webnn::mojom::blink::Device::kGpu;
    case V8MLDeviceType::Enum::kNpu:
      return webnn::mojom::blink::Device::kNpu;
  }
}

webnn::mojom::blink::CreateContextOptions::PowerPreference
ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kDefault:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kDefault;
    case V8MLPowerPreference::Enum::kLowPower:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kLowPower;
    case V8MLPowerPreference::Enum::kHighPerformance:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kHighPerformance;
  }
}

#if BUILDFLAG(USE_DAWN)
WebGPUExecutionContextToken GetExecutionContextToken(
    const ExecutionContext* execution_context) {
  if (execution_context->IsDedicatedWorkerGlobalScope()) {
    return execution_context->GetExecutionContextToken()
        .GetAs<DedicatedWorkerToken>();
  }
  if (execution_context->IsSharedWorkerGlobalScope()) {
    return execution_context->GetExecutionContextToken()
        .GetAs<SharedWorkerToken>();
  }
  if (execution_context->IsServiceWorkerGlobalScope()) {
    return execution_context->GetExecutionContextToken()
        .GetAs<ServiceWorkerToken>();
  }
  if (execution_context->IsWindow()) {
    return To<LocalDOMWindow>(execution_context)->document()->Token();
  }
  NOTREACHED();
}

void OnWebGpuDeviceRequested(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    webnn::WebGpuContextHelperOnceCallback callback,
    wgpu::Device device) {
  wgpu::Device wgpu_device = std::move(device);
  std::move(callback).Run(webnn::WebGpuContextProperties{
      .wgpu_device = wgpu_device.Get(),
      .dawn_procs = &dawn::wire::client::GetProcs(),
      // Explicitly retain references to both `DawnControlClientHolder` and
      // `wgpu::Device` within this closure so that the underlying GPU
      // command buffer channel and Dawn device remain alive throughout the
      // entire lifetime of the WebNN context, while invoking Flush() when
      // requested by LiteRT.
      .webgpu_flush = blink::BindRepeating(
          [](const scoped_refptr<DawnControlClientHolder>& dawn_control_client,
             const wgpu::Device& /*wgpu_device*/) {
            dawn_control_client->Flush();
          },
          dawn_control_client, wgpu_device)});
}

void OnWebGpuAdapterRequested(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    webnn::WebGpuContextHelperOnceCallback callback,
    wgpu::Adapter adapter) {
  Vector<wgpu::FeatureName> required_features;
  if (adapter.HasFeature(wgpu::FeatureName::ShaderF16)) {
    required_features.push_back(wgpu::FeatureName::ShaderF16);
  }
  if (adapter.HasFeature(wgpu::FeatureName::Subgroups)) {
    required_features.push_back(wgpu::FeatureName::Subgroups);
  }

  wgpu::DeviceDescriptor dawn_desc = {};
  if (!required_features.empty()) {
    dawn_desc.requiredFeatures = required_features.data();
    dawn_desc.requiredFeatureCount = required_features.size();
  }

  auto dawn_control_client_for_device = dawn_control_client;
  auto* device_callback = BindWGPUOnceCallback(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client,
         webnn::WebGpuContextHelperOnceCallback callback,
         wgpu::RequestDeviceStatus status, wgpu::Device device,
         wgpu::StringView /*message*/) {
        if (status != wgpu::RequestDeviceStatus::Success || !device) {
          std::move(callback).Run(webnn::WebGpuContextProperties());
          return;
        }
        OnWebGpuDeviceRequested(std::move(dawn_control_client),
                                std::move(callback), std::move(device));
      },
      dawn_control_client, std::move(callback));
  adapter.RequestDevice(&dawn_desc, wgpu::CallbackMode::AllowProcessEvents,
                        device_callback->UnboundCallback(),
                        device_callback->AsUserdata());
  dawn_control_client_for_device->Flush();
}

void OnWebGpuContextProviderCreated(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebGPUExecutionContextToken execution_context_token,
    webnn::WebGpuContextHelperOnceCallback callback,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider) {
  if (!context_provider || !context_provider->BindToCurrentSequence()) {
    std::move(callback).Run(webnn::WebGpuContextProperties());
    return;
  }

  context_provider->WebGPUInterface()->SetWebGPUExecutionContextToken(
      execution_context_token);

  auto dawn_control_client =
      DawnControlClientHolder::Create(std::move(context_provider), task_runner);
  wgpu::Instance instance = dawn_control_client->GetWGPUInstance();
  if (!instance) {
    std::move(callback).Run(webnn::WebGpuContextProperties());
    return;
  }

  // TODO(crbug.com/524317888): Pass the requested power preference from
  // `MLContextOptions` to `dawn_options.powerPreference`.
  wgpu::RequestAdapterOptions dawn_options = {};
  auto dawn_control_client_for_adapter = dawn_control_client;
  auto* adapter_callback = BindWGPUOnceCallback(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client,
         webnn::WebGpuContextHelperOnceCallback callback,
         wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
         wgpu::StringView /*message*/) {
        if (status != wgpu::RequestAdapterStatus::Success || !adapter) {
          std::move(callback).Run(webnn::WebGpuContextProperties());
          return;
        }
        OnWebGpuAdapterRequested(std::move(dawn_control_client),
                                 std::move(callback), std::move(adapter));
      },
      dawn_control_client, std::move(callback));
  instance.RequestAdapter(&dawn_options, wgpu::CallbackMode::AllowProcessEvents,
                          adapter_callback->UnboundCallback(),
                          adapter_callback->AsUserdata());
  dawn_control_client_for_adapter->Flush();
}

// Asynchronously creates a WebGPU 3D graphics context provider, requests a GPU
// adapter and device with required features (e.g. ShaderF16, Subgroups), and
// passes them as WebGpuContextProperties to WebNN for in-renderer LiteRT
// execution.
void InitializeWebGpuAsyncContext(
    const KURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebGPUExecutionContextToken execution_context_token,
    webnn::WebGpuContextHelperOnceCallback callback) {
  CreateWebGPUGraphicsContext3DProviderAsync(
      url, Platform::WebGPUReplyThread::kIOThread, task_runner,
      CrossThreadBindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             WebGPUExecutionContextToken execution_context_token,
             webnn::WebGpuContextHelperOnceCallback callback,
             std::unique_ptr<WebGraphicsContext3DProvider> provider) {
            if (task_runner && !task_runner->RunsTasksInCurrentSequence()) {
              PostCrossThreadTask(
                  *task_runner, FROM_HERE,
                  CrossThreadBindOnce(&OnWebGpuContextProviderCreated,
                                      task_runner, execution_context_token,
                                      std::move(callback),
                                      std::move(provider)));
            } else {
              OnWebGpuContextProviderCreated(
                  task_runner, execution_context_token, std::move(callback),
                  std::move(provider));
            }
          },
          task_runner, execution_context_token, std::move(callback)));
}
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace

ML::ML(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      in_process_context_provider_(execution_context),
      webnn_context_provider_(execution_context) {
}

void ML::Trace(Visitor* visitor) const {
  visitor->Trace(webnn_context_provider_);
  visitor->Trace(in_process_context_provider_);
  visitor->Trace(in_process_pending_resolvers_);
  visitor->Trace(pending_resolvers_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLContext> ML::createContext(ScriptState* script_state,
                                           MLContextOptions* options,
                                           ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("ML::createContext(MLContextOptions)");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  // Check if it is allowed by Permissions Policy to call WebNN API.
  if (!GetExecutionContext()->IsFeatureEnabled(
          network::mojom::blink::PermissionsPolicyFeature::kWebNN,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(
        "Access to the WebNN API is blocked by Permissions Policy.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLContext>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // Ensure `resolver` is rejected if the `CreateWebNNContext()` callback isn't
  // run due to a WebNN service connection error.
  pending_resolvers_.insert(resolver);

  EnsureWebNNServiceConnection();

  webnn_context_provider_->CreateWebNNContext(
      webnn::mojom::blink::CreateContextOptions::New(
          ConvertBlinkDeviceTypeToMojo(options->deviceType()),
          ConvertBlinkPowerPreferenceToMojo(options->powerPreference())),
      BindOnce(
          [](ML* ml, ScriptPromiseResolver<MLContext>* resolver,
             MLContextOptions* options, webnn::ScopedTrace scoped_trace,
             webnn::mojom::blink::CreateContextResultPtr result) {
            ml->pending_resolvers_.erase(resolver);

            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }

            if (result->is_error()) {
              const webnn::mojom::blink::Error& create_context_error =
                  *result->get_error();
              if (create_context_error.code ==
                  webnn::mojom::blink::Error::Code::kFallbackToInProcess) {
                // The GPU-process backend has signaled that the request
                // should be served by the in-renderer backend.
                ml->CreateInProcessContext(resolver, options,
                                           std::move(scoped_trace));
                return;
              }
              resolver->RejectWithDOMException(
                  WebNNErrorCodeToDOMExceptionCode(create_context_error.code),
                  create_context_error.message);
              return;
            }

            resolver->Resolve(MakeGarbageCollected<MLContext>(
                context, options->deviceType(), options->powerPreference(),
                std::move(result->get_success())));
          },
          WrapPersistent(this), WrapPersistent(resolver),
          WrapPersistent(options), std::move(scoped_trace)));

  return promise;
}

void ML::OnWebNNServiceConnectionError() {
  webnn_context_provider_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                     "WebNN service connection error.");
  }
  pending_resolvers_.clear();
}

void ML::EnsureWebNNServiceConnection() {
  if (webnn_context_provider_.is_bound()) {
    return;
  }
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      webnn_context_provider_.BindNewPipeAndPassReceiver(
          GetExecutionContext()->GetTaskRunner(TaskType::kMachineLearning)));
  // Bind should always succeed because ml.idl is gated on the same feature flag
  // as `WebNNContextProvider`.
  CHECK(webnn_context_provider_.is_bound());
  webnn_context_provider_.set_disconnect_handler(
      BindOnce(&ML::OnWebNNServiceConnectionError, WrapWeakPersistent(this)));
}

void ML::CreateInProcessContext(ScriptPromiseResolver<MLContext>* resolver,
                                MLContextOptions* options,
                                webnn::ScopedTrace scoped_trace) {
  EnsureInProcessServiceConnection();

  // Track this resolver in the in-renderer-specific set so that only an
  // in-renderer disconnect (not a GPU-process disconnect) can reject it.
  in_process_pending_resolvers_.insert(resolver);

  // The in_process_context_provider_ remote uses the blink Mojo variant
  // (connected via cross-variant pipe to the non-blink receiver), so
  // we can use the same callback pattern as the GPU process path.
  in_process_context_provider_->CreateWebNNContext(
      webnn::mojom::blink::CreateContextOptions::New(
          ConvertBlinkDeviceTypeToMojo(options->deviceType()),
          ConvertBlinkPowerPreferenceToMojo(options->powerPreference())),
      BindOnce(
          [](ML* ml, ScriptPromiseResolver<MLContext>* resolver,
             MLContextOptions* options, webnn::ScopedTrace scoped_trace,
             webnn::mojom::blink::CreateContextResultPtr result) {
            ml->in_process_pending_resolvers_.erase(resolver);

            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }

            if (result->is_error()) {
              const webnn::mojom::blink::Error& create_context_error =
                  *result->get_error();
              resolver->RejectWithDOMException(
                  WebNNErrorCodeToDOMExceptionCode(create_context_error.code),
                  create_context_error.message);
              return;
            }

            resolver->Resolve(MakeGarbageCollected<MLContext>(
                context, options->deviceType(), options->powerPreference(),
                std::move(result->get_success())));
          },
          WrapPersistent(this), WrapPersistent(resolver),
          WrapPersistent(options), std::move(scoped_trace)));
}

void ML::EnsureInProcessServiceConnection() {
  if (in_process_context_provider_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMachineLearning);

  // Get a WebNNWeightsFileCreator remote from the browser process to create
  // weight files for the in-renderer context provider. The remote is passed
  // to the provider as a raw message pipe handle.
  mojo::PendingRemote<webnn::mojom::blink::WebNNWeightsFileCreator>
      weights_file_creator;
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      weights_file_creator.InitWithNewPipeAndPassReceiver());

  // Create the in-renderer context provider via the thin factory.
  // The factory returns a raw pipe handle for a WebNNContextProvider remote.
  // We wrap it into a blink-variant PendingRemote — this works because blink
  // and non-blink Mojo variants use the same wire format.
  mojo::ScopedMessagePipeHandle context_provider_pipe =
      webnn::CreateInProcessContextProvider(
          weights_file_creator.PassPipe(), task_runner,
          blink::BindRepeating(&ML::GetWebGpuContextHelper,
                               WrapWeakPersistent(this)));
  in_process_context_provider_.Bind(
      mojo::PendingRemote<webnn::mojom::blink::WebNNContextProvider>(
          std::move(context_provider_pipe), 0u),
      task_runner);
  CHECK(in_process_context_provider_.is_bound());
  in_process_context_provider_.set_disconnect_handler(BindOnce(
      &ML::OnInProcessServiceConnectionError, WrapWeakPersistent(this)));
}

void ML::OnInProcessServiceConnectionError() {
  in_process_context_provider_.reset();
  for (const auto& resolver : in_process_pending_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kUnknownError,
        "In-renderer WebNN service connection error.");
  }
  in_process_pending_resolvers_.clear();
}

void ML::GetWebGpuContextHelper(
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    webnn::WebGpuContextHelperOnceCallback callback) {
#if !BUILDFLAG(USE_DAWN)
  context_task_runner->PostTask(
      FROM_HERE,
      blink::BindOnce(std::move(callback), webnn::WebGpuContextProperties()));
  return;
#else
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    context_task_runner->PostTask(
        FROM_HERE,
        blink::BindOnce(std::move(callback), webnn::WebGpuContextProperties()));
    return;
  }

  InitializeWebGpuAsyncContext(execution_context->Url(), context_task_runner,
                               GetExecutionContextToken(execution_context),
                               std::move(callback));
#endif  // !BUILDFLAG(USE_DAWN)
}

}  // namespace blink
