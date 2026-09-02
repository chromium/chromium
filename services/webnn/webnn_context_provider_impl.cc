// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include <string>

#include "base/win/windows_version.h"
#include "services/webnn/ort/context_impl_ort.h"      // nogncheck
#include "services/webnn/ort/context_provider_ort.h"  // nogncheck
#include "services/webnn/ort/dispatch_context_impl_ort.h"  // nogncheck
#include "services/webnn/ort/environment.h"           // nogncheck
#include "services/webnn/ort/ort_data_type.h"         // nogncheck
#include "services/webnn/ort/ort_session_options.h"   // nogncheck
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/webnn_switches.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/context_impl_coreml.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_impl_litert.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_CHROME_ML_API)
#include "services/on_device_model/ml/chrome_ml.h"      // nogncheck
#include "services/on_device_model/ml/chrome_ml_api.h"  // nogncheck
#endif

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>

#include "base/debug/asan_service.h"
#endif

namespace webnn {

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(WEBNN_USE_LITERT)
// Whether to use mojo data pipe for transferring tensor data between processes.
BASE_FEATURE(kWebNNUseDataPipe, base::FEATURE_ENABLED_BY_DEFAULT);

struct TensorDataPipes {
  mojo::ScopedDataPipeProducerHandle write_producer;
  mojo::ScopedDataPipeConsumerHandle write_consumer;
  mojo::ScopedDataPipeProducerHandle read_producer;
  mojo::ScopedDataPipeConsumerHandle read_consumer;
};

TensorDataPipes CreateTensorDataPipes() {
  TensorDataPipes pipes;
  if (base::FeatureList::IsEnabled(kWebNNUseDataPipe)) {
    constexpr base::ByteSize kDataPipeSize = base::MiB(16);
    MojoResult result = mojo::CreateDataPipe(
        kDataPipeSize.InBytes(), pipes.write_producer, pipes.write_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for WriteTensor.";
    }
    result = mojo::CreateDataPipe(kDataPipeSize.InBytes(), pipes.read_producer,
                                  pipes.read_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for ReadTensor.";
    }
  }
  return pipes;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(WEBNN_USE_LITERT)

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

// These values are persisted to logs. Entries should not be renumbered or
// removed and numeric values should never be reused.
// Please keep in sync with DeviceTypeUma in
// //tools/metrics/histograms/metadata/webnn/enums.xml.
enum class DeviceTypeUma {
  kCpu = 0,
  kGpu = 1,
  kNpu = 2,
  kMaxValue = kNpu,
};

void RecordDeviceType(const mojom::Device device) {
  DeviceTypeUma uma_value;
  switch (device) {
    case mojom::Device::kCpu:
      uma_value = DeviceTypeUma::kCpu;
      break;
    case mojom::Device::kGpu:
      uma_value = DeviceTypeUma::kGpu;
      break;
    case mojom::Device::kNpu:
      uma_value = DeviceTypeUma::kNpu;
      break;
  }
  base::UmaHistogramEnumeration("WebNN.DeviceType", uma_value);
}

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
// Returns true if the request described by `options` should be served by the
// renderer-process (in-process) TFLite/LiteRT backend instead of the
// GPU-process backend. For `kGpu` device requests, the GPU process attempts
// execution first using the LiteRT WebGPU accelerator
// (`libLiteRtWebGpuAccelerator`, which is preloaded during
// `PreSandboxWebNNInitialization()`). If no GPU accelerator is available or if
// the request is for `kCpu` / `kNpu`, it falls back to the renderer-process
// (in-process) TFLite/LiteRT backend.
bool ShouldUseInProcessTflite(const mojom::CreateContextOptions& options) {
  if (options.device == mojom::Device::kGpu &&
      base::FeatureList::IsEnabled(
          mojom::features::kWebNNLiteRTGpuInRenderer)) {
    return true;
  }
  return options.device != mojom::Device::kGpu;
}

void FallbackInProcessTFLite(
    WebNNContextProvider::CreateWebNNContextCallback callback) {
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kFallbackToInProcess,
      "Falling back to in-process TFLite/LiteRT backend."));
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

#if defined(ADDRESS_SANITIZER)
NO_SANITIZE("address")
void AsanUnsafeFeatureWarning(const char* reason,
                              bool* should_exit_cleanly,
                              bool* should_abort) {
  auto* asan_service = base::debug::AsanService::GetInstance();
  asan_service->Log("\nUnsafe feature: WebMachineLearningNeuralNetwork");
}
#endif

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::PendingRemote<mojom::WebNNBrowserHost> webnn_browser_host)
    : gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      shared_image_manager_(shared_image_manager),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)),
      scheduler_(scheduler),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      peak_memory_monitor_(std::move(peak_memory_monitor)),
      webnn_browser_host_(std::move(webnn_browser_host)) {
  CHECK_NE(scheduler_, nullptr);
  CHECK_NE(main_thread_task_runner_, nullptr);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  CHECK(webnn_browser_host_.is_bound());

#if defined(ADDRESS_SANITIZER)
  LOG(ERROR) << "WebMachineLearningNeuralNetwork is an unsafe feature.";
  base::debug::AsanService::GetInstance()->AddErrorCallback(
      AsanUnsafeFeatureWarning);
#endif
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Destroy all GPU sequences on the main thread.
  // gpu::Scheduler will DCHECK if any sequences remain alive at destruction.
  for (const auto& [context_handle, sequence_id] : sequences_) {
    scheduler_->DestroySequence(sequence_id);
  }

  // Sequences for contexts which failed to be created and dropped the posted
  // reply must also be destroyed.
  for (const gpu::SequenceId sequence_id : pending_sequences_) {
    scheduler_->DestroySequence(sequence_id);
  }
}

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::PendingRemote<mojom::WebNNBrowserHost> webnn_browser_host) {
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(gpu_feature_info), std::move(gpu_info), shared_image_manager,
      std::move(peak_memory_monitor), std::move(lose_all_contexts_callback),
      std::move(main_thread_task_runner), scheduler,
      std::move(webnn_browser_host)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    const WebNNReceiversParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.Add(this, std::move(receiver), params);
}

void WebNNContextProviderImpl::SetDisconnectHandlerForTesting(  // IN-TEST
    base::RepeatingClosure handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.set_disconnect_handler(std::move(handler));
}

size_t WebNNContextProviderImpl::GetContextCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return context_impls_.size();
}

std::vector<std::string_view>
WebNNContextProviderImpl::GetContextBackendNamesForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::vector<std::string_view> backend_names;
  for (const auto& context_impl : context_impls_) {
    backend_names.push_back(context_impl->GetBackendName());
  }
  return backend_names;
}

void WebNNContextProviderImpl::BindWebNNServiceIntrospection(
    mojo::PendingReceiver<mojom::WebNNServiceIntrospection> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  service_introspection_receiver_.Bind(std::move(receiver));
}

void WebNNContextProviderImpl::SetClient(
    mojo::PendingRemote<mojom::WebNNServiceIntrospectionClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  service_introspection_client_.Bind(std::move(client));
}

std::vector<mojom::WebNNContextIntrospectionDetailsPtr>
WebNNContextProviderImpl::PopulateContextsDetailsForIntrospection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::vector<mojom::WebNNContextIntrospectionDetailsPtr> contexts_details;
  for (auto& context_impl : context_impls_) {
    auto details = mojom::WebNNContextIntrospectionDetails::New();
    details->context_id = context_impl->tracing_id();
    details->context_backend = context_impl->GetBackendName();
    details->execution_providers = context_impl->GetExecutionProvidersInfo();
    contexts_details.push_back(std::move(details));
  }
  return contexts_details;
}

void WebNNContextProviderImpl::GetExistingContextsDetails(
    GetExistingContextsDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto contexts_details = PopulateContextsDetailsForIntrospection();
  std::move(callback).Run(std::move(contexts_details));
}

void WebNNContextProviderImpl::GetAvailableExecutionProvidersDetails(
    GetAvailableExecutionProvidersDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // This implementation currently only supports reporting execution providers
  // for the ORT backend on Windows, and returns an empty list for other
  // platforms and backends. This is because the ORT backend is the only one
  // that has multiple execution providers and where the available execution
  // providers can vary based on the system configuration.
#if BUILDFLAG(IS_WIN)
  std::optional<scoped_refptr<ort::Environment>> environment =
      ort::Environment::GetInstance();
  // If the ORT environment is not initialized, there is no EP information to
  // report, so return an empty list.
  if (!environment.has_value()) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(environment.value()->GetAvailableEpDetails());
#else
  std::move(callback).Run({});
#endif  // BUILDFLAG(IS_WIN)
}

void WebNNContextProviderImpl::UpdateWebNNServiceIntrospection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!service_introspection_client_.is_bound()) {
    return;
  }
  auto contexts_details = PopulateContextsDetailsForIntrospection();
  service_introspection_client_->OnUpdateExistingContextDetails(
      std::move(contexts_details));

#if BUILDFLAG(IS_WIN)
  std::optional<scoped_refptr<ort::Environment>> environment =
      ort::Environment::GetInstance();
  // If the list of contexts is empty, then the ORT environment will be
  // destroyed soon.
  if (environment.has_value() && !context_impls_.empty()) {
    service_introspection_client_->OnUpdateAvailableExecutionProvidersDetails(
        environment.value()->GetAvailableEpDetails());
  } else {
    service_introspection_client_->OnUpdateAvailableExecutionProvidersDetails(
        {});
  }
#endif  // BUILDFLAG(IS_WIN)
}

void WebNNContextProviderImpl::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto context_it = context_impls_.find(handle);
  CHECK(context_it != context_impls_.end());
  context_impls_.erase(context_it);
  UpdateWebNNServiceIntrospection();
}

void WebNNContextProviderImpl::DestroyAndRemoveGpuSequence(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto sequence_it = sequences_.find(handle);
  CHECK(sequence_it != sequences_.end());
  scheduler_->DestroySequence(sequence_it->second);
  sequences_.erase(sequence_it);
}

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::DestroyAllContextsAndKillGpuProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  std::move(lose_all_contexts_callback_).Run();
}
#endif  // BUILDFLAG(IS_WIN)

// static
void WebNNContextProviderImpl::SetBackendForTesting(
    BackendForTesting* backend_for_testing) {
  g_backend_for_testing = backend_for_testing;
}

// static
bool WebNNContextProviderImpl::HasBackendForTesting() {
  return g_backend_for_testing != nullptr;
}

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // `current_context()` must only be called within the stack frame of an actual
  // interface method invocation or disconnect notification scheduled by a
  // receiver. It is illegal to attempt to call this at any other time, such as
  // from within an asynchronous task or callback posted from a message handler.
  const WebNNReceiversParams params = provider_receivers_.current_context();

  // Force context creation to fail if the WebNN GPU feature is disabled, which
  // happens when the GPU process has crashed too many times.
  if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] ==
      gpu::kGpuFeatureStatusDisabled) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kUnknownError,
        "WebNN is disabled due to some unresolvable issues."));
    return;
  }

  // Generates unique IDs for WebNNContextImpl.
  static base::AtomicSequenceNumber g_next_route_id;

  // WebNN IPC operations without a SyncToken are re-posted to the scheduled
  // task runner to ensure they execute in the same sequence and order as those
  // with a SyncToken.
  const gpu::CommandBufferId command_buffer_id =
      gpu::CommandBufferIdFromChannelAndRoute(params.client_id,
                                              g_next_route_id.GetNext());

  bool use_main_thread = (g_backend_for_testing != nullptr);

#if BUILDFLAG(IS_APPLE)
  bool should_create_coreml_context = false;
  if (__builtin_available(macOS 14.4, *)) {
    should_create_coreml_context =
        base::FeatureList::IsEnabled(mojom::features::kWebNNCoreML) &&
        !params.is_incognito
#if BUILDFLAG(IS_MAC)
        && base::mac::GetCPUType() == base::mac::CPUType::kArm
#endif  // BUILDFLAG(IS_MAC)
        ;
    // CoreML contexts are created and owned on the main thread.
  }
  use_main_thread |= should_create_coreml_context;
#endif  // BUILDFLAG(IS_APPLE)

  // Task runner used to create the context on gpu sequence.
  // Backends that support multi-threading can use a separate task runner.
  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      use_main_thread ? main_thread_task_runner_
                      : base::ThreadPool::CreateSingleThreadTaskRunner(
                            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Each context gets a GPU sequence that must be destroyed on the GPU main
  // thread. See `sequences_` and `pending_sequences_` comments in the header
  // for the full sequence lifecycle.
  const gpu::SequenceId sequence_id = scheduler_->CreateSequence(
      gpu::SchedulingPriority::kNormal, owning_task_runner,
      gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE, command_buffer_id);

  auto gpu_task_scheduler = std::make_unique<GpuTaskScheduler>(
      *scheduler_, command_buffer_id, sequence_id,
      gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE);

  scoped_refptr<gpu::MemoryTracker> memory_tracker =
      base::MakeRefCounted<gpu::MemoryTracker>(
          command_buffer_id, params.client_tracing_id, peak_memory_monitor_,
          gpu::GpuPeakMemoryAllocationSource::WEBNN);

  ScopedTrace scoped_trace("WebNNContextProviderImpl::CreateWebNNContext");

  if (g_backend_for_testing) {
    auto [it, inserted] =
        context_impls_.emplace(g_backend_for_testing->CreateWebNNContext(
            AsWeakPtr(), std::move(options), std::move(gpu_task_scheduler),
            memory_tracker, owning_task_runner, shared_image_manager_,
            main_thread_task_runner_, std::move(callback)));
    CHECK(inserted);
    sequences_.emplace((*it)->handle(), sequence_id);
    return;
  }

  pending_sequences_.insert(sequence_id);

  RecordDeviceType(options->device);

#if BUILDFLAG(IS_WIN)
  if (ort::ShouldTryCreateOrtContext()) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();

    scoped_trace.AddStep("EnsureWebNNExecutionProvidersReady");

    // If we're on a version of Windows which doesn't support EPs, or we're told
    // to ignore EPs, use empty `ep_package_info` to create the ORT context.
    if ((base::win::GetVersion() < kWinAppRuntimeSupportedMinVersion) ||
        command_line->HasSwitch(switches::kWebNNOrtIgnoreIhvEps)) {
      DidEnsureWebNNExecutionProvidersReady(
          std::move(scoped_trace), std::move(options),
          std::move(gpu_task_scheduler), std::move(owning_task_runner),
          std::move(callback), params.is_incognito, memory_tracker,
          /*ep_package_info=*/{});
      return;
    }

    webnn_browser_host_->EnsureExecutionProvidersReady(base::BindOnce(
        &WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady,
        AsWeakPtr(), std::move(scoped_trace), std::move(options),
        std::move(gpu_task_scheduler), std::move(owning_task_runner),
        std::move(callback), params.is_incognito, memory_tracker));
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
  if (should_create_coreml_context) {
    if (__builtin_available(macOS 14.4, *)) {
      mojo::PendingRemote<mojom::WebNNContext> remote;
      auto receiver = remote.InitWithNewPipeAndPassReceiver();
      WebNNContextImplPtr context_impl = coreml::ContextImplCoreml::Create(
          std::move(receiver), AsWeakPtr(), std::move(options),
          std::move(gpu_task_scheduler), memory_tracker, owning_task_runner,
          shared_image_manager_, main_thread_task_runner_);
      // Using mojo data pipe is not yet implemented in CoreML backend.
      OnCreateWebNNContextImpl(std::move(callback), std::move(remote),
                               mojo::ScopedDataPipeProducerHandle(),
                               mojo::ScopedDataPipeConsumerHandle(),
                               sequence_id, command_buffer_id,
                               std::move(context_impl));
      return;
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

  FallbackToTFLite(std::move(scoped_trace), std::move(options),
                   std::move(gpu_task_scheduler), std::move(owning_task_runner),
                   std::move(callback), params.is_incognito,
                   std::move(memory_tracker), sequence_id, command_buffer_id);
}

void WebNNContextProviderImpl::OnCreateWebNNContextImpl(
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<::webnn::mojom::WebNNContext> remote,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    gpu::SequenceId sequence_id,
    gpu::CommandBufferId command_buffer_id,
    WebNNContextImplPtr context_impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Remove from the pending set now that the reply has arrived.
  // This is a no-op for synchronous callers that never inserted.
  pending_sequences_.erase(sequence_id);

  if (!context_impl) {
    scheduler_->DestroySequence(sequence_id);
    WebNNContextImpl::RecordContextBackendUma(
        WebNNContextImpl::ContextBackendUma::kNotSupported);
    // TODO(crbug.com/40206287): Supporting WebNN on the platform.
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported on this platform."));
    LOG(ERROR) << "WebNN is not supported on this platform.";
    return;
  }

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();

  sequences_.emplace(context_handle, sequence_id);
  context_impls_.emplace(std::move(context_impl));

  UpdateWebNNServiceIntrospection();

  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), /*compiler_context_remote=*/mojo::NullRemote(),
      std::move(context_properties), std::move(context_handle),
      std::move(write_tensor_producer), std::move(read_tensor_consumer),
      command_buffer_id.GetUnsafeValue());
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

void WebNNContextProviderImpl::FallbackToTFLite(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    gpu::SequenceId sequence_id,
    gpu::CommandBufferId command_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // Ask the renderer to create an in-process context.
  if (ShouldUseInProcessTflite(*options)) {
    FallbackInProcessTFLite(std::move(callback));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_LITERT)
  // Attempt to create a LiteRT GPU context (`ContextImplLiteRt`) in the GPU
  // process using the WebGPU accelerator preloaded prior to sandbox lockdown
  // (`PreSandboxWebNNInitialization()`). If context creation fails or is not
  // supported, returning `kNotSupportedError` from `OnCreateWebNNContextImpl`
  // lets the renderer's `ML::createContext` fallback path create the in-process
  // LiteRT context instead.
  CreateLiteRtContext(std::move(scoped_trace), std::move(options),
                      std::move(gpu_task_scheduler), std::move(task_runner),
                      std::move(callback), is_incognito,
                      std::move(memory_tracker));
#else
  WebNNContextImplPtr context_impl(nullptr, OnTaskRunnerDeleter(task_runner));
  OnCreateWebNNContextImpl(std::move(callback),
                           mojo::PendingRemote<mojom::WebNNContext>(),
                           mojo::ScopedDataPipeProducerHandle(),
                           mojo::ScopedDataPipeConsumerHandle(), sequence_id,
                           command_buffer_id, std::move(context_impl));
#endif  // BUILDFLAG(WEBNN_USE_LITERT)
}

void WebNNContextProviderImpl::CreateWeightsFile(
    mojom::WebNNBrowserHost::CreateWeightsFileCallback callback) {
  webnn_browser_host_->CreateWeightsFile(std::move(callback));
}

#if BUILDFLAG(WEBNN_USE_LITERT)
void WebNNContextProviderImpl::CreateLiteRtContext(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker) {
  const gpu::SequenceId sequence_id = gpu_task_scheduler->sequence_id();
  const gpu::CommandBufferId command_buffer_id =
      gpu_task_scheduler->command_buffer_id();
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  TensorDataPipes pipes = CreateTensorDataPipes();
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &litert::ContextImplLiteRt::Create, std::move(receiver), AsWeakPtr(),
          std::move(options), std::move(pipes.write_consumer),
          std::move(pipes.read_producer), std::move(gpu_task_scheduler),
          std::move(memory_tracker), task_runner,
          base::Unretained(shared_image_manager_.get()),
          main_thread_task_runner_, std::move(scoped_trace), is_incognito),
      base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                     AsWeakPtr(), std::move(callback), std::move(remote),
                     std::move(pipes.write_producer),
                     std::move(pipes.read_consumer), sequence_id,
                     command_buffer_id));
}
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::OnOrtEnvCreated(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    base::expected<scoped_refptr<ort::Environment>, std::string>
        env_creation_results) {
  const gpu::SequenceId sequence_id = gpu_task_scheduler->sequence_id();
  const gpu::CommandBufferId command_buffer_id =
      gpu_task_scheduler->command_buffer_id();
  if (!env_creation_results.has_value()) {
    LOG(ERROR) << "[WebNN] Failed to create ONNX Runtime environment: "
               << env_creation_results.error();
  } else {
    auto env = std::move(env_creation_results.value());
    if (base::FeatureList::IsEnabled(mojom::features::kWebNNCompilerProcess)) {
      // When the Compiler process is enabled, graph compilation is delegated to
      // a per-EP-device Compiler process and this provider only creates a
      // dispatch context that runs the compiled graphs. If no compatible EP
      // device is available, fall back to another backend. Otherwise request a
      // compiler context first, and create the dispatch context only when the
      // compiler context is available.
      OrtHardwareDeviceType device_type =
          ort::WebnnToOrtDeviceType(options->device);
      std::optional<EpDeviceInfo> selected_device =
          env->SelectEpDeviceForCompiler(device_type);
      if (selected_device.has_value()) {
        scoped_trace.AddStep("RequestWebNNCompilerContext");

        // Compute the context properties so the compiler context can be
        // requested before the dispatch context is created.
        const EpWorkarounds ep_workarounds = env->GetEpWorkarounds(device_type);
        ContextProperties properties =
            ort::ContextImplOrt::GetContextProperties(
                ep_workarounds.resample2d_limit_to_nchw);

        // Create the compiler context pipe (renderer->Compiler) and the
        // model loader pipe (Compiler->GPU).
        mojo::PendingRemote<mojom::WebNNCompilerContext>
            compiler_context_remote;
        auto compiler_context_receiver =
            compiler_context_remote.InitWithNewPipeAndPassReceiver();
        mojo::PendingRemote<mojom::WebNNModelLoader> model_loader_remote;
        auto model_loader_receiver =
            model_loader_remote.InitWithNewPipeAndPassReceiver();

        // Defer creating the dispatch context (which would consume the GPU
        // resources) until the compiler context confirms availability. If it is
        // unavailable, the context can still fall back to another backend using
        // these resources.
        auto reply = base::BindOnce(
            &WebNNContextProviderImpl::OnCompilerContextRequested, AsWeakPtr(),
            std::move(scoped_trace), options->Clone(),
            std::move(gpu_task_scheduler), std::move(task_runner),
            std::move(callback), is_incognito, std::move(memory_tracker),
            std::move(env), *selected_device,
            std::move(compiler_context_remote),
            std::move(model_loader_receiver), sequence_id, command_buffer_id);

        webnn_browser_host_->RequestCompilerContext(
            std::move(options), properties, *selected_device,
            std::move(compiler_context_receiver),
            std::move(model_loader_remote), std::move(reply));
        return;
      }
    } else {
      // Create session options before posting context creation, so that
      // if no EP device is available we can fall back to TFLite/LiteRT.
      auto session_options_result =
          ort::SessionOptions::Create(options.Clone(), env);
      if (!session_options_result.has_value()) {
        LOG(ERROR) << "[WebNN] Failed to create ONNX Runtime session options: "
                   << session_options_result.error();
      } else {
        scoped_trace.AddStep("ort::ContextImplOrt::Create");
        mojo::PendingRemote<mojom::WebNNContext> remote;
        auto receiver = remote.InitWithNewPipeAndPassReceiver();
        TensorDataPipes pipes = CreateTensorDataPipes();
        // Safe to use base::Unretained for shared_image_manager_ since it
        // lives on the GPU service, which is guaranteed to outlive the provider
        // and its contexts.
        task_runner->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                &ort::ContextImplOrt::Create, std::move(receiver), AsWeakPtr(),
                std::move(options), std::move(pipes.write_consumer),
                std::move(pipes.read_producer), std::move(env),
                std::move(session_options_result.value()),
                std::move(gpu_task_scheduler), std::move(memory_tracker),
                task_runner, base::Unretained(shared_image_manager_.get()),
                main_thread_task_runner_, std::move(scoped_trace)),
            base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                           AsWeakPtr(), std::move(callback), std::move(remote),
                           std::move(pipes.write_producer),
                           std::move(pipes.read_consumer), sequence_id,
                           command_buffer_id));
        return;
      }
    }
  }

  FallbackToTFLite(std::move(scoped_trace), std::move(options),
                   std::move(gpu_task_scheduler), std::move(task_runner),
                   std::move(callback), is_incognito, std::move(memory_tracker),
                   sequence_id, command_buffer_id);
}

void WebNNContextProviderImpl::OnCompilerContextRequested(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<ort::Environment> env,
    EpDeviceInfo target_device,
    mojo::PendingRemote<mojom::WebNNCompilerContext> compiler_context_remote,
    mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver,
    gpu::SequenceId sequence_id,
    gpu::CommandBufferId command_buffer_id,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (!success) {
    VLOG(1) << "[WebNN] Compiler context unavailable. Falling back to another "
               "backend.";
    FallbackToTFLite(std::move(scoped_trace), std::move(options),
                     std::move(gpu_task_scheduler), std::move(task_runner),
                     std::move(callback), is_incognito,
                     std::move(memory_tracker), sequence_id, command_buffer_id);
    return;
  }

  scoped_trace.AddStep("ort::DispatchContextImplOrt::Create");

  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  TensorDataPipes pipes = CreateTensorDataPipes();

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ort::DispatchContextImplOrt::Create, std::move(receiver),
                     std::move(model_loader_receiver), AsWeakPtr(),
                     std::move(options), std::move(pipes.write_consumer),
                     std::move(pipes.read_producer), std::move(env),
                     std::move(gpu_task_scheduler), std::move(memory_tracker),
                     task_runner, base::Unretained(shared_image_manager_.get()),
                     main_thread_task_runner_, std::move(target_device)),
      base::BindOnce(&WebNNContextProviderImpl::OnDispatchContextCreated,
                     AsWeakPtr(), std::move(scoped_trace), std::move(callback),
                     std::move(remote), std::move(pipes.write_producer),
                     std::move(pipes.read_consumer), sequence_id,
                     command_buffer_id, std::move(compiler_context_remote)));
}

void WebNNContextProviderImpl::OnDispatchContextCreated(
    ScopedTrace scoped_trace,
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    gpu::SequenceId sequence_id,
    gpu::CommandBufferId command_buffer_id,
    mojo::PendingRemote<mojom::WebNNCompilerContext> compiler_context_remote,
    WebNNContextImplPtr context_impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  scoped_trace.AddStep("OnDispatchContextCreated");

  CHECK(context_impl);

  pending_sequences_.erase(sequence_id);
  sequences_.emplace(context_impl->handle(), sequence_id);

  auto context_success = mojom::CreateContextSuccess::New(
      std::move(remote), std::move(compiler_context_remote),
      context_impl->properties(), context_impl->handle(),
      std::move(write_tensor_producer), std::move(read_tensor_consumer),
      command_buffer_id.GetUnsafeValue());
  context_impls_.emplace(std::move(context_impl));

  UpdateWebNNServiceIntrospection();

  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(context_success)));
}

void WebNNContextProviderImpl::ReconnectCompilerContext(
    mojom::CreateContextOptionsPtr options,
    ContextProperties properties,
    EpDeviceInfo target_device,
    mojo::PendingReceiver<mojom::WebNNCompilerContext>
        compiler_context_receiver,
    mojo::PendingRemote<mojom::WebNNModelLoader> model_loader_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // This is a reconnect for an already-created context, so it cannot fall back
  // to another backend at this point.
  webnn_browser_host_->RequestCompilerContext(
      std::move(options), properties, target_device,
      std::move(compiler_context_receiver), std::move(model_loader_remote),
      base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success)
            << "[WebNN] Compiler context failed to reconnect.";
      }));
}

void WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  scoped_trace.AddStep("ort::Environment::GetInstance");

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ort::Environment::GetOrCreateInstance,
                     std::move(ep_package_info)),
      base::BindOnce(&WebNNContextProviderImpl::OnOrtEnvCreated, AsWeakPtr(),
                     std::move(scoped_trace), std::move(options),
                     std::move(gpu_task_scheduler), task_runner,
                     std::move(callback), is_incognito,
                     std::move(memory_tracker)));
}

void WebNNContextProviderImpl::ForceOrtEnvironmentCreationForIntrospection(
    ForceOrtEnvironmentCreationForIntrospectionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<scoped_refptr<ort::Environment>> environment =
      ort::Environment::GetInstance();
  // If the ORT environment is already created, there is no need to force its
  // creation.
  if (environment.has_value()) {
    std::move(callback).Run(environment.value()->GetAvailableEpDetails());
    return;
  }

  if (base::win::GetVersion() < kWinAppRuntimeSupportedMinVersion) {
    DidEnsureWebNNExecutionProvidersReadyForIntrospection(
        main_thread_task_runner_, std::move(callback),
        /*ep_package_info=*/{});
  } else {
    webnn_browser_host_->EnsureExecutionProvidersReady(base::BindOnce(
        &WebNNContextProviderImpl::
            DidEnsureWebNNExecutionProvidersReadyForIntrospection,
        AsWeakPtr(), main_thread_task_runner_, std::move(callback)));
  }
}

void WebNNContextProviderImpl::
    DidEnsureWebNNExecutionProvidersReadyForIntrospection(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        ForceOrtEnvironmentCreationForIntrospectionCallback callback,
        base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ort::Environment::GetOrCreateInstance,
                     std::move(ep_package_info)),
      base::BindOnce(&WebNNContextProviderImpl::OnOrtEnvCreatedForIntrospection,
                     AsWeakPtr(), std::move(callback)));
}

void WebNNContextProviderImpl::OnOrtEnvCreatedForIntrospection(
    ForceOrtEnvironmentCreationForIntrospectionCallback callback,
    base::expected<scoped_refptr<ort::Environment>, std::string>
        env_creation_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<scoped_refptr<ort::Environment>> environment =
      ort::Environment::GetInstance();
  if (!environment.has_value()) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(environment.value()->GetAvailableEpDetails());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
