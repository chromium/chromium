// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/scoped_gpu_sequence.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include <string>

#include "base/win/windows_version.h"
#include "services/webnn/ort/context_impl_ort.h"      // nogncheck
#include "services/webnn/ort/context_provider_ort.h"  // nogncheck
#include "services/webnn/ort/environment.h"           // nogncheck
#include "services/webnn/ort/ort_session_options.h"   // nogncheck
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/webnn_switches.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/context_impl_coreml.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_TFLITE)
#include "services/webnn/tflite/context_impl_tflite.h"  // nogncheck
#endif

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_impl_litert.h"  // nogncheck
#endif

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>

#include "base/debug/asan_service.h"
#endif

namespace webnn {

namespace {

// Whether to use mojo data pipe for transferring tensor data between processes.
BASE_FEATURE(kWebNNUseDataPipe, base::FEATURE_ENABLED_BY_DEFAULT);

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

static constexpr gpu::CommandBufferNamespace kWebNNContextImplNamespaceId =
    gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE;

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
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host)
    : gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      shared_image_manager_(shared_image_manager),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)),
      scheduler_(scheduler),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      peak_memory_monitor_(std::move(peak_memory_monitor)),
      gpu_host_(std::move(gpu_host)) {
  CHECK_NE(scheduler_, nullptr);
  CHECK_NE(main_thread_task_runner_, nullptr);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // `gpu_host_` is used to ensure that the execution providers used by the ORT
  // backend are ready. It should be connected to the browser process.
  CHECK(gpu_host_.is_bound());

#if defined(ADDRESS_SANITIZER)
  LOG(ERROR) << "WebMachineLearningNeuralNetwork is an unsafe feature.";
  base::debug::AsanService::GetInstance()->AddErrorCallback(
      AsanUnsafeFeatureWarning);
#endif
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
}

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host) {
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(gpu_feature_info), std::move(gpu_info), shared_image_manager,
      std::move(peak_memory_monitor), std::move(lose_all_contexts_callback),
      std::move(main_thread_task_runner), scheduler, std::move(gpu_host)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    const WebNNReceiversParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.Add(this, std::move(receiver), params);
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

void WebNNContextProviderImpl::UpdateWebNNServiceIntrospection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!service_introspection_client_.is_bound()) {
    return;
  }
  auto contexts_details = PopulateContextsDetailsForIntrospection();
  service_introspection_client_->OnUpdateExistingContextDetails(
      std::move(contexts_details));
}

void WebNNContextProviderImpl::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
  UpdateWebNNServiceIntrospection();
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

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    WebNNContextProvider::CreateWebNNContextCallback callback) {
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

  auto gpu_sequence = std::make_unique<ScopedGpuSequence>(
      *scheduler_, owning_task_runner, command_buffer_id,
      kWebNNContextImplNamespaceId);

  scoped_refptr<gpu::MemoryTracker> memory_tracker =
      base::MakeRefCounted<gpu::MemoryTracker>(
          command_buffer_id, params.client_tracing_id, peak_memory_monitor_,
          gpu::GpuPeakMemoryAllocationSource::WEBNN);

  ScopedTrace scoped_trace("WebNNContextProviderImpl::CreateWebNNContext");

  if (g_backend_for_testing) {
    context_impls_.emplace(g_backend_for_testing->CreateWebNNContext(
        AsWeakPtr(), std::move(options), std::move(gpu_sequence),
        std::move(memory_tracker), owning_task_runner, shared_image_manager_,
        main_thread_task_runner_, std::move(callback)));
    return;
  }

  WebNNContextImplPtr context_impl(nullptr,
                                   OnTaskRunnerDeleter(owning_task_runner));
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  RecordDeviceType(options->device);

  mojo::ScopedDataPipeProducerHandle write_tensor_producer;
  mojo::ScopedDataPipeConsumerHandle write_tensor_consumer;
  mojo::ScopedDataPipeProducerHandle read_tensor_producer;
  mojo::ScopedDataPipeConsumerHandle read_tensor_consumer;
  if (base::FeatureList::IsEnabled(kWebNNUseDataPipe)) {
    constexpr base::ByteCount kDataPipeSize = base::MiB(16);
    MojoResult result = mojo::CreateDataPipe(
        kDataPipeSize.InBytes(), write_tensor_producer, write_tensor_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for WriteTensor.";
    }
    result = mojo::CreateDataPipe(kDataPipeSize.InBytes(), read_tensor_producer,
                                  read_tensor_consumer);
    if (result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Failed to create a mojo data pipe for ReadTensor.";
    }
  }

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
          std::move(write_tensor_producer), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(read_tensor_consumer),
          std::move(gpu_sequence), std::move(owning_task_runner),
          std::move(receiver), std::move(remote), std::move(callback),
          params.is_incognito, std::move(memory_tracker),
          /*ep_package_info=*/{});
      return;
    }

    gpu_host_->EnsureWebNNExecutionProvidersReady(base::BindOnce(
        &WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady,
        AsWeakPtr(), std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        std::move(gpu_sequence), std::move(owning_task_runner),
        std::move(receiver), std::move(remote), std::move(callback),
        params.is_incognito, std::move(memory_tracker)));
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
  if (should_create_coreml_context) {
    if (__builtin_available(macOS 14.4, *)) {
      // Using mojo data pipe is not yet implemented in CoreML backend.
      write_tensor_producer.reset();
      write_tensor_consumer.reset();
      read_tensor_producer.reset();
      read_tensor_consumer.reset();
      context_impl = coreml::ContextImplCoreml::Create(
          std::move(receiver), AsWeakPtr(), std::move(options),
          std::move(gpu_sequence), std::move(memory_tracker),
          owning_task_runner, shared_image_manager_, main_thread_task_runner_);
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(WEBNN_USE_LITERT)
  if (!context_impl &&
      base::FeatureList::IsEnabled(mojom::features::kWebNNLiteRT)) {
    CreateLiteRtContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        std::move(gpu_sequence), std::move(owning_task_runner),
        std::move(receiver), std::move(remote), std::move(callback),
        params.is_incognito, std::move(memory_tracker));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    CreateTFLiteContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        std::move(gpu_sequence), std::move(owning_task_runner),
        std::move(receiver), std::move(remote), std::move(callback),
        params.is_incognito, std::move(memory_tracker));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

  OnCreateWebNNContextImpl(
      std::move(callback), std::move(remote), std::move(write_tensor_producer),
      std::move(read_tensor_consumer), std::move(context_impl));
}

void WebNNContextProviderImpl::OnCreateWebNNContextImpl(
    WebNNContextProvider::CreateWebNNContextCallback callback,
    mojo::PendingRemote<::webnn::mojom::WebNNContext> remote,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    WebNNContextImplPtr context_impl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (!context_impl) {
    WebNNContextImpl::RecordContextBackendUma(
        WebNNContextImpl::ContextBackendUma::kNotSupported);
    // TODO(crbug.com/40206287): Supporting WebNN on the platform.
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported on this platform."));
    LOG(ERROR) << "WebNN is not supported on this platform.";
    return;
  }

  gpu::CommandBufferId command_buffer_id =
      context_impl->gpu_sequence()->command_buffer_id();
  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();
  context_impls_.emplace(std::move(context_impl));

  UpdateWebNNServiceIntrospection();

  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), std::move(context_properties),
      std::move(context_handle), std::move(write_tensor_producer),
      std::move(read_tensor_consumer), command_buffer_id.GetUnsafeValue());
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

void WebNNContextProviderImpl::CreateWeightsFile(
    viz::mojom::GpuHost::CreateWebNNWeightsFileCallback callback) {
  gpu_host_->CreateWebNNWeightsFile(std::move(callback));
}

#if BUILDFLAG(WEBNN_USE_TFLITE)
void WebNNContextProviderImpl::CreateTFLiteContext(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &tflite::ContextImplTflite::Create, std::move(receiver), AsWeakPtr(),
          std::move(options), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(gpu_sequence),
          std::move(memory_tracker), task_runner,
          base::Unretained(shared_image_manager_.get()),
          main_thread_task_runner_, std::move(scoped_trace), is_incognito),
      base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                     AsWeakPtr(), std::move(callback), std::move(remote),
                     std::move(write_tensor_producer),
                     std::move(read_tensor_consumer)));
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

#if BUILDFLAG(WEBNN_USE_LITERT)
void WebNNContextProviderImpl::CreateLiteRtContext(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &litert::ContextImplLiteRt::Create, std::move(receiver), AsWeakPtr(),
          std::move(options), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(gpu_sequence),
          std::move(memory_tracker), task_runner,
          base::Unretained(shared_image_manager_.get()),
          main_thread_task_runner_, std::move(scoped_trace), is_incognito),
      base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                     AsWeakPtr(), std::move(callback), std::move(remote),
                     std::move(write_tensor_producer),
                     std::move(read_tensor_consumer)));
}
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::OnOrtEnvCreated(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    base::expected<scoped_refptr<ort::Environment>, std::string>
        env_creation_results) {
  if (env_creation_results.has_value()) {
    scoped_trace.AddStep("ort::ContextImplOrt::Create");
    // Safe to use base::Unretained for shared_image_manager_ since it
    // lives on the GPU service, which is guaranteed to outlive the provider
    // and its contexts.
    task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &ort::ContextImplOrt::Create, std::move(receiver), AsWeakPtr(),
            std::move(options), std::move(write_tensor_consumer),
            std::move(read_tensor_producer),
            std::move(env_creation_results.value()), std::move(gpu_sequence),
            std::move(memory_tracker), task_runner,
            base::Unretained(shared_image_manager_.get()),
            main_thread_task_runner_, std::move(scoped_trace)),
        base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                       AsWeakPtr(), std::move(callback), std::move(remote),
                       std::move(write_tensor_producer),
                       std::move(read_tensor_consumer)));
    return;
  }

  LOG(ERROR) << "[WebNN] Failed to create ONNX Runtime environment: "
             << env_creation_results.error();

#if BUILDFLAG(WEBNN_USE_LITERT)
  if (base::FeatureList::IsEnabled(mojom::features::kWebNNLiteRT)) {
    CreateLiteRtContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        std::move(gpu_sequence), std::move(task_runner), std::move(receiver),
        std::move(remote), std::move(callback), is_incognito,
        std::move(memory_tracker));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  CreateTFLiteContext(
      std::move(scoped_trace), std::move(options),
      std::move(write_tensor_producer), std::move(write_tensor_consumer),
      std::move(read_tensor_producer), std::move(read_tensor_consumer),
      std::move(gpu_sequence), std::move(task_runner), std::move(receiver),
      std::move(remote), std::move(callback), is_incognito,
      std::move(memory_tracker));
  return;
#else
  WebNNContextImplPtr context_impl(nullptr, OnTaskRunnerDeleter(task_runner));

  OnCreateWebNNContextImpl(
      std::move(callback), std::move(remote), std::move(write_tensor_producer),
      std::move(read_tensor_consumer), std::move(context_impl));
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)
}

void WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info) {
  scoped_trace.AddStep("ort::Environment::GetInstance");

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ort::Environment::GetInstance, gpu_feature_info_,
                     std::move(ep_package_info)),
      base::BindOnce(
          &WebNNContextProviderImpl::OnOrtEnvCreated, AsWeakPtr(),
          std::move(scoped_trace), std::move(options),
          std::move(write_tensor_producer), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(read_tensor_consumer),
          std::move(gpu_sequence), task_runner, std::move(receiver),
          std::move(remote), std::move(callback), is_incognito,
          std::move(memory_tracker)));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
