// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu.h"

#include <utility>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/gpu/gpu.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/modules/webgpu/wgsl_language_features.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

[[maybe_unused]] void AddConsoleWarning(ExecutionContext* execution_context,
                                        const char* message) {
  if (execution_context) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StringFromASCIIAndUTF8(message));
    execution_context->AddConsoleMessage(console_message);
  }
}

wgpu::PowerPreference AsDawnType(V8GPUPowerPreference power_preference) {
  switch (power_preference.AsEnum()) {
    case V8GPUPowerPreference::Enum::kLowPower:
      return wgpu::PowerPreference::LowPower;
    case V8GPUPowerPreference::Enum::kHighPerformance:
      return wgpu::PowerPreference::HighPerformance;
  }
}

wgpu::RequestAdapterOptions AsDawnType(
    const GPURequestAdapterOptions* webgpu_options) {
  DCHECK(webgpu_options);

  wgpu::RequestAdapterOptions dawn_options = {
      .forceFallbackAdapter = webgpu_options->forceFallbackAdapter(),
      .compatibilityMode = webgpu_options->compatibilityMode(),
  };
  if (webgpu_options->hasPowerPreference()) {
    dawn_options.powerPreference =
        AsDawnType(webgpu_options->powerPreference());
  }

  return dawn_options;
}

// Returns the execution context token given the context. Currently returning
// the WebGPU specific execution context token.
// TODO(dawn:549) Might be able to use ExecutionContextToken instead of WebGPU
//     specific execution context token if/when DocumentToken becomes a part of
//     ExecutionContextToken.
WebGPUExecutionContextToken GetExecutionContextToken(
    const ExecutionContext* execution_context) {
  // WebGPU only supports the following types of context tokens: DocumentTokens,
  // DedicatedWorkerTokens, SharedWorkerTokens, and ServiceWorkerTokens. The
  // token is sent to the GPU process so that it can be cross-referenced against
  // the browser process to get an isolation key for caching purposes.
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
  NOTREACHED_IN_MIGRATION();
  return WebGPUExecutionContextToken();
}

}  // anonymous namespace

// static
const char GPU::kSupplementName[] = "GPU";

// static
GPU* GPU::gpu(NavigatorBase& navigator) {
  GPU* gpu = Supplement<NavigatorBase>::From<GPU>(navigator);
  if (!gpu) {
    gpu = MakeGarbageCollected<GPU>(navigator);
    ProvideTo(navigator, gpu);
  }
  return gpu;
}

GPU::GPU(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      wgsl_language_features_(
          MakeGarbageCollected<WGSLLanguageFeatures>(GatherWGSLFeatures())),
      mappable_buffer_handles_(
          base::MakeRefCounted<BoxedMappableWGPUBufferHandles>()) {}

GPU::~GPU() = default;

WGSLLanguageFeatures* GPU::wgslLanguageFeatures() const {
  return wgsl_language_features_.Get();
}

void GPU::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(mappable_buffers_);
  visitor->Trace(wgsl_language_features_);
}

void GPU::ContextDestroyed() {
  if (!dawn_control_client_) {
    return;
  }
  // Ensure all DOMArrayBuffers backed by shared memory are detached before
  // the WebGPU command buffer and transfer buffers are destroyed.
  // This is necessary because we will free the shmem backings, and some
  // short amount of JS can still execute after the ContextDestroyed event
  // is received.
  if (!mappable_buffers_.empty()) {
    v8::Isolate* isolate = GetExecutionContext()->GetIsolate();
    v8::HandleScope scope(isolate);
    for (GPUBuffer* buffer : mappable_buffers_) {
      buffer->DetachMappedArrayBuffers(isolate);
    }
  }
  // GPUBuffer::~GPUBuffer and GPUBuffer::destroy will remove wgpu::Buffers from
  // |mappable_buffer_handles_|.
  // However, there may be GPUBuffers that were removed from mappable_buffers_
  // for which ~GPUBuffer has not run yet. These GPUBuffers and their
  // DOMArrayBuffer mappings are no longer reachable from JS, so we don't need
  // to detach them, but we do need to eagerly destroy the wgpu::Buffer so that
  // its shared memory is freed before the context is completely destroyed.
  mappable_buffer_handles_->ClearAndDestroyAll();
  dawn_control_client_->Destroy();
}

void GPU::OnRequestAdapterCallback(
    ScriptState* script_state,
    const GPURequestAdapterOptions* options,
    ScriptPromiseResolver<IDLNullable<GPUAdapter>>* resolver,
    wgpu::RequestAdapterStatus status,
    wgpu::Adapter adapter,
    const char* error_message) {
  GPUAdapter* gpu_adapter = nullptr;
  switch (status) {
    case wgpu::RequestAdapterStatus::Success:
      gpu_adapter = MakeGarbageCollected<GPUAdapter>(
          this, std::move(adapter), dawn_control_client_, options);
      break;

    // Note: requestAdapter never rejects, but we print a console warning if
    // there are error messages.
    case wgpu::RequestAdapterStatus::Unavailable:
    case wgpu::RequestAdapterStatus::Error:
    case wgpu::RequestAdapterStatus::Unknown:
    case wgpu::RequestAdapterStatus::InstanceDropped:
      break;
  }
  if (error_message) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StringFromASCIIAndUTF8(error_message));
    execution_context->AddConsoleMessage(console_message);
  }
  RecordAdapterForIdentifiability(script_state, options, gpu_adapter);
  resolver->Resolve(gpu_adapter);
}

void GPU::RecordAdapterForIdentifiability(
    ScriptState* script_state,
    const GPURequestAdapterOptions* options,
    GPUAdapter* adapter) const {
  constexpr IdentifiableSurface::Type type =
      IdentifiableSurface::Type::kGPU_RequestAdapter;
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(type))
    return;
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;

  IdentifiableTokenBuilder input_builder;
  if (options && options->hasPowerPreference()) {
    input_builder.AddToken(IdentifiabilityBenignStringToken(
        options->powerPreference().AsString()));
  }
  const auto surface =
      IdentifiableSurface::FromTypeAndToken(type, input_builder.GetToken());

  IdentifiableTokenBuilder output_builder;
  if (adapter) {
    for (const auto& feature : adapter->features()->FeatureNameSet()) {
      output_builder.AddToken(IdentifiabilityBenignStringToken(feature));
    }
  }

  IdentifiabilityMetricBuilder(context->UkmSourceID())
      .Add(surface, output_builder.GetToken())
      .Record(context->UkmRecorder());
}

std::unique_ptr<WebGraphicsContext3DProvider> CheckContextProvider(
    const KURL& url,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider) {
  // Note that we check for API blocking *after* creating the context. This is
  // because context creation synchronizes against GpuProcessHost lifetime in
  // the browser process, and GpuProcessHost destruction is what updates API
  // blocking state on a GPU process crash. See https://crbug.com/1215907#c10
  // for more details.
  bool blocked = true;
  mojo::Remote<mojom::blink::GpuDataManager> gpu_data_manager;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      gpu_data_manager.BindNewPipeAndPassReceiver());
  gpu_data_manager->Are3DAPIsBlockedForUrl(url, &blocked);
  if (blocked) {
    return nullptr;
  }

  // TODO(kainino): we will need a better way of accessing the GPU interface
  // from multiple threads than BindToCurrentSequence et al.
  if (context_provider && !context_provider->BindToCurrentSequence()) {
    // TODO(crbug.com/973017): Collect GPU info and surface context creation
    // error.
    return nullptr;
  }
  return context_provider;
}

void GPU::RequestAdapterImpl(
    ScriptState* script_state,
    const GPURequestAdapterOptions* options,
    ScriptPromiseResolver<IDLNullable<GPUAdapter>>* resolver) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Validate that the featureLevel is undefined. If not return a null adapter.
  // This logic will evolve as feature levels are added in the future.
  if (options->hasFeatureLevel()) {
    OnRequestAdapterCallback(script_state, options, resolver,
                             wgpu::RequestAdapterStatus::Error, nullptr,
                             "Unknown feature level");
    return;
  }

  if (!dawn_control_client_ || dawn_control_client_->IsContextLost()) {
    dawn_control_client_initialized_callbacks_.push_back(WTF::BindOnce(
        [](GPU* gpu, ScriptState* script_state,
           const GPURequestAdapterOptions* options,
           ScriptPromiseResolver<IDLNullable<GPUAdapter>>* resolver) {
          if (gpu->dawn_control_client_ &&
              !gpu->dawn_control_client_->IsContextLost()) {
            gpu->RequestAdapterImpl(script_state, options, resolver);
          } else {
            // Failed to create context provider, won't be able to request
            // adapter
            // TODO(crbug.com/973017): Collect GPU info and surface context
            // creation error.
            gpu->OnRequestAdapterCallback(
                script_state, options, resolver,
                wgpu::RequestAdapterStatus::Error, nullptr,
                "Failed to create WebGPU Context Provider");
          }
        },
        WrapPersistent(this), WrapPersistent(script_state),
        WrapPersistent(options), WrapPersistent(resolver)));

    // Returning since the task to create the control client from a previous
    // call to EnsureDawnControlClientInitialized should be already running
    if (dawn_control_client_initialized_callbacks_.size() > 1) {
      return;
    }

    CreateWebGPUGraphicsContext3DProviderAsync(
        execution_context->Url(),
        execution_context->GetTaskRunner(TaskType::kWebGPU),
        CrossThreadBindOnce(
            [](CrossThreadHandle<GPU> gpu_handle,
               CrossThreadHandle<ExecutionContext> execution_context_handle,
               std::unique_ptr<WebGraphicsContext3DProvider> context_provider) {
              auto unwrap_gpu = MakeUnwrappingCrossThreadHandle(gpu_handle);
              auto unwrap_execution_context =
                  MakeUnwrappingCrossThreadHandle(execution_context_handle);
              if (!unwrap_gpu || !unwrap_execution_context) {
                return;
              }
              auto* gpu = unwrap_gpu.GetOnCreationThread();
              auto* execution_context =
                  unwrap_execution_context.GetOnCreationThread();
              const KURL& url = execution_context->Url();
              context_provider =
                  CheckContextProvider(url, std::move(context_provider));
              if (context_provider) {
                context_provider->WebGPUInterface()
                    ->SetWebGPUExecutionContextToken(
                        GetExecutionContextToken(execution_context));

                // Make a new DawnControlClientHolder with the context provider
                // we just made and set the lost context callback
                gpu->dawn_control_client_ = DawnControlClientHolder::Create(
                    std::move(context_provider),
                    execution_context->GetTaskRunner(TaskType::kWebGPU));
              }

              WTF::Vector<base::OnceCallback<void()>> callbacks =
                  std::move(gpu->dawn_control_client_initialized_callbacks_);
              for (auto& callback : callbacks) {
                std::move(callback).Run();
              }
            },
            MakeCrossThreadHandle(this),
            MakeCrossThreadHandle(execution_context)));
    return;
  }

  DCHECK_NE(dawn_control_client_, nullptr);

  wgpu::RequestAdapterOptions dawn_options = AsDawnType(options);
  auto* callback = MakeWGPUOnceCallback(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&GPU::OnRequestAdapterCallback, WrapPersistent(this),
                    WrapPersistent(script_state), WrapPersistent(options))));

  dawn_control_client_->GetWGPUInstance().RequestAdapter(
      &dawn_options, wgpu::CallbackMode::AllowSpontaneous,
      callback->UnboundCallback(), callback->AsUserdata());
  dawn_control_client_->EnsureFlush(
      *execution_context->GetAgent()->event_loop());

  UseCounter::Count(execution_context, WebFeature::kWebGPURequestAdapter);
}

ScriptPromise<IDLNullable<GPUAdapter>> GPU::requestAdapter(
    ScriptState* script_state,
    const GPURequestAdapterOptions* options) {
  // Remind developers when they are using WebGPU on unsupported platforms.
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context &&
      !base::FeatureList::IsEnabled(features::kWebGPUService)) {
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kInfo,
        "WebGPU is experimental on this platform. See "
        "https://github.com/gpuweb/gpuweb/wiki/"
        "Implementation-Status#implementation-status"));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<GPUAdapter>>>(
          script_state);
  auto promise = resolver->Promise();
  RequestAdapterImpl(script_state, options, resolver);
  return promise;
}

V8GPUTextureFormat GPU::getPreferredCanvasFormat() {
  return FromDawnEnum(preferred_canvas_format());
}

wgpu::TextureFormat GPU::preferred_canvas_format() {
#if BUILDFLAG(IS_ANDROID)
  return wgpu::TextureFormat::RGBA8Unorm;
#else
  return wgpu::TextureFormat::BGRA8Unorm;
#endif
}

void GPU::TrackMappableBuffer(GPUBuffer* buffer) {
  mappable_buffers_.insert(buffer);
  mappable_buffer_handles_->insert(buffer->GetHandle());
}

void GPU::UntrackMappableBuffer(GPUBuffer* buffer) {
  mappable_buffers_.erase(buffer);
  mappable_buffer_handles_->erase(buffer->GetHandle());
}

void BoxedMappableWGPUBufferHandles::ClearAndDestroyAll() {
  for (const wgpu::Buffer& b : contents_) {
    b.Destroy();
  }
  contents_.clear();
}

void GPU::SetDawnControlClientHolderForTesting(
    scoped_refptr<DawnControlClientHolder> dawn_control_client) {
  dawn_control_client_ = std::move(dawn_control_client);
}

}  // namespace blink
