// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/scoped_gpu_sequence.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include <string>

#include "services/webnn/dml/context_provider_dml.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/context_provider_ort.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/webnn_switches.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/context_impl_coreml.h"
#endif

#if BUILDFLAG(WEBNN_USE_TFLITE)
#include "services/webnn/tflite/context_impl_tflite.h"
#endif

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_impl_litert.h"
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

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host)
    : shared_context_state_(std::move(shared_context_state)),
      gpu_feature_info_(std::move(gpu_feature_info)),
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
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
}

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host) {
  // `shared_context_state` is only used by DirectML backend for GPU context. It
  // may be nullptr when GPU acceleration is not available. For such case, WebNN
  // GPU feature (`gpu::GPU_FEATURE_TYPE_WEBNN`) is not enabled and creating a
  // GPU context will result in a not-supported error.
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(shared_context_state), std::move(gpu_feature_info),
      std::move(gpu_info), shared_image_manager, std::move(peak_memory_monitor),
      std::move(lose_all_contexts_callback), std::move(main_thread_task_runner),
      scheduler, std::move(gpu_host)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    const WebNNReceiversParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  provider_receivers_.Add(this, std::move(receiver), params);
}

void WebNNContextProviderImpl::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
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

  // TODO(crbug.com/474940915): Create the `gpu_sequence` with
  // `owning_task_runner` instead of `main_thread_task_runner_` to avoid
  // resetting it later.
  auto gpu_sequence = std::make_unique<ScopedGpuSequence>(
      *scheduler_, main_thread_task_runner_, command_buffer_id,
      kWebNNContextImplNamespaceId);

  scoped_refptr<gpu::MemoryTracker> memory_tracker =
      base::MakeRefCounted<gpu::MemoryTracker>(
          command_buffer_id, params.client_tracing_id, peak_memory_monitor_,
          gpu::GpuPeakMemoryAllocationSource::WEBNN);

  ScopedTrace scoped_trace("WebNNContextProviderImpl::CreateWebNNContext");

  if (g_backend_for_testing) {
    context_impls_.emplace(g_backend_for_testing->CreateWebNNContext(
        AsWeakPtr(), std::move(options), std::move(gpu_sequence),
        std::move(memory_tracker), main_thread_task_runner_,
        shared_image_manager_, main_thread_task_runner_, std::move(callback)));
    return;
  }

  // Task runner used to create the context on gpu sequence.
  // Backend must support multi-threading to use a separate task runner.
  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

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
  if (ort::ShouldCreateOrtContext(*options)) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();

    scoped_trace.AddStep("EnsureWebNNExecutionProvidersReady");

    // If ignore IHV EPs, use empty `ep_package_info` to create the ORT context.
    if (command_line->HasSwitch(switches::kWebNNOrtIgnoreIhvEps)) {
      DidEnsureWebNNExecutionProvidersReady(
          std::move(scoped_trace), std::move(options),
          std::move(write_tensor_producer), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(read_tensor_consumer),
          command_buffer_id, std::move(gpu_sequence),
          std::move(owning_task_runner), std::move(receiver), std::move(remote),
          std::move(callback), params.is_incognito, std::move(memory_tracker),
          /*ep_package_info=*/{});
      return;
    }

    gpu_host_->EnsureWebNNExecutionProvidersReady(base::BindOnce(
        &WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady,
        AsWeakPtr(), std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(gpu_sequence),
        std::move(owning_task_runner), std::move(receiver), std::move(remote),
        std::move(callback), params.is_incognito, std::move(memory_tracker)));
    return;
  } else if (dml::ShouldCreateDmlContext(*options)) {
    base::expected<WebNNContextImplPtr, mojom::ErrorPtr>
        context_creation_results = dml::CreateContextFromOptions(
            std::move(options), std::move(write_tensor_consumer),
            std::move(read_tensor_producer), gpu_feature_info_, gpu_info_,
            shared_context_state_.get(), std::move(receiver), AsWeakPtr(),
            std::move(gpu_sequence), std::move(memory_tracker),
            main_thread_task_runner_, shared_image_manager_,
            main_thread_task_runner_);
    if (!context_creation_results.has_value()) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          std::move(context_creation_results.error())));
      return;
    }
    context_impl = std::move(context_creation_results.value());
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
  if (__builtin_available(macOS 14.4, *)) {
    if (base::FeatureList::IsEnabled(mojom::features::kWebNNCoreML) &&
        !params.is_incognito
#if BUILDFLAG(IS_MAC)
        && base::mac::GetCPUType() == base::mac::CPUType::kArm
#endif  // BUILDFLAG(IS_MAC)
    ) {
      // Using mojo data pipe is not yet implemented in CoreML backend.
      write_tensor_producer.reset();
      write_tensor_consumer.reset();
      read_tensor_producer.reset();
      read_tensor_consumer.reset();
      context_impl = coreml::ContextImplCoreml::Create(
          std::move(receiver), AsWeakPtr(), std::move(options),
          std::move(gpu_sequence), std::move(memory_tracker),
          main_thread_task_runner_, shared_image_manager_,
          main_thread_task_runner_);
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(WEBNN_USE_LITERT)
  if (!context_impl) {
    CreateLiteRtContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(gpu_sequence),
        std::move(owning_task_runner), std::move(receiver), std::move(remote),
        std::move(callback));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    CreateTFLiteContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(gpu_sequence),
        std::move(owning_task_runner), std::move(receiver), std::move(remote),
        std::move(callback), params.is_incognito, std::move(memory_tracker));
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

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();
  context_impls_.emplace(std::move(context_impl));

  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), std::move(context_properties),
      std::move(context_handle), std::move(write_tensor_producer),
      std::move(read_tensor_consumer));
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

base::optional_ref<WebNNContextImpl>
WebNNContextProviderImpl::GetWebNNContextImplForTesting(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  const auto it = context_impls_.find(handle);
  if (it == context_impls_.end()) {
    mojo::ReportBadMessage(kBadMessageInvalidContext);
    return std::nullopt;
  }
  return it->get();
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
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker) {
  gpu_sequence.reset();
  gpu_sequence = std::make_unique<ScopedGpuSequence>(
      *scheduler_, task_runner, command_buffer_id,
      kWebNNContextImplNamespaceId);

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
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback) {
  gpu_sequence.reset();
  gpu_sequence = std::make_unique<ScopedGpuSequence>(
      *scheduler_, task_runner, command_buffer_id,
      kWebNNContextImplNamespaceId);

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&litert::ContextImplLiteRt::Create, std::move(receiver),
                     AsWeakPtr(), std::move(options),
                     std::move(write_tensor_consumer),
                     std::move(read_tensor_producer), std::move(gpu_sequence),
                     std::move(memory_tracker_), task_runner,
                     base::Unretained(shared_image_manager_.get()),
                     main_thread_task_runner_, std::move(scoped_trace)),
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
    gpu::CommandBufferId command_buffer_id,
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
  CreateLiteRtContext(
      std::move(scoped_trace), std::move(options),
      std::move(write_tensor_producer), std::move(write_tensor_consumer),
      std::move(read_tensor_producer), std::move(read_tensor_consumer),
      command_buffer_id, std::move(gpu_sequence), std::move(task_runner),
      std::move(receiver), std::move(remote), std::move(callback));
  return;
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  CreateTFLiteContext(
      std::move(scoped_trace), std::move(options),
      std::move(write_tensor_producer), std::move(write_tensor_consumer),
      std::move(read_tensor_producer), std::move(read_tensor_consumer),
      command_buffer_id, std::move(gpu_sequence), std::move(task_runner),
      std::move(receiver), std::move(remote), std::move(callback), is_incognito,
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
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    bool is_incognito,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info) {
  scoped_trace.AddStep("ort::Environment::GetInstance");

  // Re-create gpu sequence for the new task runner. Destroying the old
  // gpu sequence is safe since it has no scheduled tasks yet.
  gpu_sequence.reset();
  gpu_sequence = std::make_unique<ScopedGpuSequence>(
      *scheduler_, task_runner, command_buffer_id,
      kWebNNContextImplNamespaceId);

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ort::Environment::GetInstance, gpu_feature_info_,
                     std::move(ep_package_info)),
      base::BindOnce(
          &WebNNContextProviderImpl::OnOrtEnvCreated, AsWeakPtr(),
          std::move(scoped_trace), std::move(options),
          std::move(write_tensor_producer), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(read_tensor_consumer),
          command_buffer_id, std::move(gpu_sequence), task_runner,
          std::move(receiver), std::move(remote), std::move(callback),
          is_incognito, std::move(memory_tracker)));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
