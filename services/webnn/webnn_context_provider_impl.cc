// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include <string>

#include "base/types/expected_macros.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "services/webnn/dml/context_provider_dml.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/context_provider_ort.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_session_options.h"
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

namespace webnn {

namespace {

// Whether to use mojo data pipe for transferring tensor data between processes.
BASE_FEATURE(kWebNNUseDataPipe, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether to allow multiple threads.
BASE_FEATURE(kWebNNAllowMultipleThreads, base::FEATURE_ENABLED_BY_DEFAULT);

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

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    int32_t client_id,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host)
    : shared_context_state_(std::move(shared_context_state)),
      gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      shared_image_manager_(shared_image_manager),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)),
      scheduler_(scheduler),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      client_id_(client_id),
      gpu_host_(std::move(gpu_host)) {
  CHECK_NE(scheduler_, nullptr);
  CHECK_NE(main_thread_task_runner_, nullptr);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (shared_context_state_) {
    memory_tracker_ = shared_context_state_->memory_tracker();
  }
  // `gpu_host_` is used to ensure that the execution providers used by the ORT
  // backend are ready. It should be connected to the browser process.
  CHECK(gpu_host_.is_bound());
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    gpu::SharedImageManager* shared_image_manager,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    int32_t client_id,
    mojo::SharedRemote<viz::mojom::GpuHost> gpu_host) {
  // `shared_context_state` is only used by DirectML backend for GPU context. It
  // may be nullptr when GPU acceleration is not available. For such case, WebNN
  // GPU feature (`gpu::GPU_FEATURE_TYPE_WEBNN`) is not enabled and creating a
  // GPU context will result in a not-supported error.
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(shared_context_state), std::move(gpu_feature_info),
      std::move(gpu_info), shared_image_manager,
      std::move(lose_all_contexts_callback), std::move(main_thread_task_runner),
      scheduler, client_id, std::move(gpu_host)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver) {
  provider_receivers_.Add(this, std::move(receiver));
}

void WebNNContextProviderImpl::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
}

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::DestroyAllContextsAndKillGpuProcess(
    const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Send the contexts lost reason to the renderer process.
  for (const auto& impl : context_impls_) {
    impl->OnLost(reason);
  }

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Generates unique IDs for WebNNContextImpl.
  static base::AtomicSequenceNumber g_next_route_id;

  // WebNN IPC operations without a SyncToken are re-posted to the scheduled
  // task runner to ensure they execute in the same sequence and order as those
  // with a SyncToken.
  const gpu::CommandBufferId command_buffer_id =
      gpu::CommandBufferIdFromChannelAndRoute(client_id_,
                                              g_next_route_id.GetNext());

  auto sequence = std::make_unique<ScopedSequence>(
      *scheduler_, main_thread_task_runner_, command_buffer_id);

  ScopedTrace scoped_trace("WebNNContextProviderImpl::CreateWebNNContext");

  if (g_backend_for_testing) {
    context_impls_.emplace(g_backend_for_testing->CreateWebNNContext(
        AsWeakPtr(), std::move(options), command_buffer_id, std::move(sequence),
        memory_tracker_, main_thread_task_runner_, shared_image_manager_,
        main_thread_task_runner_, std::move(callback)));
    return;
  }

  // Task runner used to create the context on sequence.
  // Only the main thread task runner is used unless the feature is enabled and
  // the backend has support.
  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      main_thread_task_runner_;

  if (base::FeatureList::IsEnabled(kWebNNAllowMultipleThreads)) {
    owning_task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
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
  if (ort::ShouldCreateOrtContext(*options)) {
    scoped_trace.AddStep("EnsureWebNNExecutionProvidersReady");

    gpu_host_->EnsureWebNNExecutionProvidersReady(base::BindOnce(
        &WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady,
        AsWeakPtr(), std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(sequence), std::move(owning_task_runner),
        std::move(receiver), std::move(remote), std::move(callback)));
    return;
  } else if (dml::ShouldCreateDmlContext(*options)) {
    base::expected<WebNNContextImplPtr, mojom::ErrorPtr>
        context_creation_results = dml::CreateContextFromOptions(
            std::move(options), std::move(write_tensor_consumer),
            std::move(read_tensor_producer), gpu_feature_info_, gpu_info_,
            shared_context_state_.get(), std::move(receiver), AsWeakPtr(),
            command_buffer_id, std::move(sequence), memory_tracker_,
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
    if (base::FeatureList::IsEnabled(mojom::features::kWebNNCoreML)
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
          command_buffer_id, std::move(sequence), memory_tracker_,
          main_thread_task_runner_, shared_image_manager_,
          main_thread_task_runner_);
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    CreateTFLiteContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(sequence), std::move(owning_task_runner),
        std::move(receiver), std::move(remote), std::move(callback));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!context_impl) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = context_impls_.find(handle);
  if (it == context_impls_.end()) {
    mojo::ReportBadMessage(kBadMessageInvalidContext);
    return std::nullopt;
  }
  return it->get();
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
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback) {
  if (!task_runner->BelongsToCurrentThread()) {
    sequence.reset();
    sequence = std::make_unique<ScopedSequence>(*scheduler_, task_runner,
                                                command_buffer_id);

    scoped_trace.AddStep("Create on sequence");

    task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &tflite::ContextImplTflite::Create, std::move(receiver),
            AsWeakPtr(), std::move(options), std::move(write_tensor_consumer),
            std::move(read_tensor_producer), command_buffer_id,
            std::move(sequence), std::move(memory_tracker_), task_runner,
            base::Unretained(shared_image_manager_.get()),
            main_thread_task_runner_, std::move(scoped_trace)),
        base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                       AsWeakPtr(), std::move(callback), std::move(remote),
                       std::move(write_tensor_producer),
                       std::move(read_tensor_consumer)));
    return;
  }
  WebNNContextImplPtr context_impl = tflite::ContextImplTflite::Create(
      std::move(receiver), AsWeakPtr(), std::move(options),
      std::move(write_tensor_consumer), std::move(read_tensor_producer),
      command_buffer_id, std::move(sequence), memory_tracker_,
      std::move(task_runner), shared_image_manager_, main_thread_task_runner_,
      std::move(scoped_trace));

  OnCreateWebNNContextImpl(
      std::move(callback), std::move(remote), std::move(write_tensor_producer),
      std::move(read_tensor_consumer), std::move(context_impl));
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::DidEnsureWebNNExecutionProvidersReady(
    ScopedTrace scoped_trace,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeProducerHandle write_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    mojo::ScopedDataPipeConsumerHandle read_tensor_consumer,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    CreateWebNNContextCallback callback,
    base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info) {
  WebNNContextImplPtr context_impl(
      nullptr, OnTaskRunnerDeleter(main_thread_task_runner_));

  scoped_trace.AddStep("ort::Environment::GetInstance");

  base::expected<scoped_refptr<ort::Environment>, std::string>
      env_creation_results =
          ort::Environment::GetInstance(gpu_info_, ep_package_info);
  if (!env_creation_results.has_value()) {
    LOG(ERROR) << "[WebNN] Failed to create ONNX Runtime context: "
               << env_creation_results.error();
  } else {
    mojom::Device device_type = options->device;
    // Falls back to GPU if the device type is NPU but NPU is disabled.
    if (device_type == mojom::Device::kNpu &&
        gpu_feature_info_.IsWorkaroundEnabled(gpu::DISABLE_WEBNN_FOR_NPU)) {
      device_type = mojom::Device::kGpu;
      LOG(WARNING) << "[WebNN] [WARNING] NPU device is disabled to create "
                      "ONNX Runtime context. Falling back to GPU.";
    }

    const EpWorkarounds ep_workarounds =
        env_creation_results.value()->GetEpWorkarounds(device_type);

    if (!task_runner->BelongsToCurrentThread()) {
      // Re-create sequence for the new task runner. Destroying the old
      // sequence is safe since it has no scheduled tasks yet.
      sequence.reset();
      sequence = std::make_unique<ScopedSequence>(*scheduler_, task_runner,
                                                  command_buffer_id);

      scoped_trace.AddStep("Create on sequence");

      // Safe to use base::Unretained for shared_image_manager_ since it
      // lives on the GPU service, which is guaranteed to outlive the provider
      // and its contexts.
      task_runner->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &ort::ContextImplOrt::Create, std::move(receiver), AsWeakPtr(),
              ep_workarounds, std::move(options), device_type,
              std::move(write_tensor_consumer), std::move(read_tensor_producer),
              std::move(env_creation_results.value()), command_buffer_id,
              std::move(sequence), std::move(memory_tracker_), task_runner,
              base::Unretained(shared_image_manager_.get()),
              main_thread_task_runner_, std::move(scoped_trace)),
          base::BindOnce(&WebNNContextProviderImpl::OnCreateWebNNContextImpl,
                         AsWeakPtr(), std::move(callback), std::move(remote),
                         std::move(write_tensor_producer),
                         std::move(read_tensor_consumer)));
      return;
    }
    context_impl = ort::ContextImplOrt::Create(
        std::move(receiver), AsWeakPtr(), ep_workarounds, std::move(options),
        device_type, std::move(write_tensor_consumer),
        std::move(read_tensor_producer),
        std::move(env_creation_results.value()), command_buffer_id,
        std::move(sequence), memory_tracker_, std::move(task_runner),
        shared_image_manager_, main_thread_task_runner_,
        std::move(scoped_trace));
  }

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    CreateTFLiteContext(
        std::move(scoped_trace), std::move(options),
        std::move(write_tensor_producer), std::move(write_tensor_consumer),
        std::move(read_tensor_producer), std::move(read_tensor_consumer),
        command_buffer_id, std::move(sequence), std::move(task_runner),
        std::move(receiver), std::move(remote), std::move(callback));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

  OnCreateWebNNContextImpl(
      std::move(callback), std::move(remote), std::move(write_tensor_producer),
      std::move(read_tensor_consumer), std::move(context_impl));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
