// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/on_device_model_executor.h"

#include "base/check.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

using on_device_model::mojom::LoadModelResult;

namespace ml {
namespace {

const base::FeatureParam<std::string> kGpuBlockList{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_gpu_block_list", ""};

// Helper to bind object methods as weak task-posting callback functions.
template <typename R, typename C, typename... Args>
std::function<R(Args...)> CreateWeakCallbackFn(R (C::*method)(Args...),
                                               C* that) {
  return [weak_ptr = that->AsWeakPtr(), method,
          task_runner =
              base::SequencedTaskRunner::GetCurrentDefault()](Args&&... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(method, weak_ptr, std::forward<Args>(args)...));
  };
}

// Handles sending and canceling responses.
class Responder : public base::SupportsWeakPtr<Responder> {
 public:
  explicit Responder(
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> responder)
      : responder_(std::move(responder)) {
    responder_.set_disconnect_handler(
        base::BindOnce(&Responder::Cancel, base::Unretained(this)));
  }
  ~Responder() { Cancel(); }

  ChromeMLCancelFn* GetCancelFn() { return &cancel_; }

  ChromeMLOutputFn CreateOutputFn() {
    return CreateWeakCallbackFn(&Responder::OnResponse, this);
  }

  ChromeMLCompletionFn CreateCompletionFn() {
    return CreateWeakCallbackFn(&Responder::OnComplete, this);
  }

 private:
  void OnResponse(const std::optional<std::string>& token) {
    if (token.has_value()) {
      responder_->OnResponse(*token);
    } else {
      // If the model invokes OnResponse() with no token, this implies
      // completion without retraction.
      OnComplete(ChromeMLExecutionResult{.retracted = false});
    }
  }

  void OnComplete(const ChromeMLExecutionResult& result) {
    if (!responder_) {
      return;
    }

    using ResponseStatus = on_device_model::mojom::ResponseStatus;
    if (result.retracted) {
      responder_->OnComplete(ResponseStatus::kRetracted);
    } else {
      responder_->OnComplete(ResponseStatus::kOk);
    }
  }

  void Cancel() {
    if (cancel_) {
      cancel_();
    }
  }

  mojo::Remote<on_device_model::mojom::StreamingResponder> responder_;
  ChromeMLCancelFn cancel_;
};

// Handles calling the ContextClient on completion and canceling the context
// request.
class ContextHolder : public base::SupportsWeakPtr<ContextHolder> {
 public:
  explicit ContextHolder(
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
      base::OnceCallback<void(ContextHolder*)> on_disconnect)
      : client_(std::move(client)), on_disconnect_(std::move(on_disconnect)) {
    if (client_) {
      client_.set_disconnect_handler(
          base::BindOnce(&ContextHolder::OnDisconnect, base::Unretained(this)));
    }
  }
  ~ContextHolder() {
    if (cancel_) {
      cancel_();
    }
  }

  ChromeMLCancelFn* GetCancelFn() { return &cancel_; }

  ChromeMLContextSavedFn CreateContextSavedFn() {
    return [weak_ptr = AsWeakPtr(),
            task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
               int tokens_processed) {
      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(&ContextHolder::OnComplete, weak_ptr,
                                           tokens_processed));
    };
  }

 private:
  void OnComplete(int tokens_processed) {
    if (client_) {
      client_->OnComplete(tokens_processed);
    }
    OnDisconnect();
  }

  void OnDisconnect() {
    if (on_disconnect_) {
      std::move(on_disconnect_).Run(this);
    }
    // this may be deleted.
  }

  mojo::Remote<on_device_model::mojom::ContextClient> client_;
  base::OnceCallback<void(ContextHolder*)> on_disconnect_;
  ChromeMLCancelFn cancel_;
};

class SessionImpl : public on_device_model::OnDeviceModel::Session {
 public:
  SessionImpl(const ChromeML& chrome_ml, ChromeMLModel model)
      : chrome_ml_(chrome_ml), model_(model) {}
  ~SessionImpl() override = default;

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  void AddContext(on_device_model::mojom::InputOptionsPtr input,
                  mojo::PendingRemote<on_device_model::mojom::ContextClient>
                      client) override {
    auto context_holder = std::make_unique<ContextHolder>(
        std::move(client),
        base::BindOnce(&SessionImpl::RemoveContext, base::Unretained(this)));
    ChromeMLContextSavedFn context_saved_fn =
        context_holder->CreateContextSavedFn();
    ChromeMLExecuteOptions options{
        .prompt = input->text.c_str(),
        .context_mode = GetContextMode(*input) | ContextMode::kSave,
        .max_tokens = input->max_tokens.value_or(0),
        .token_offset = input->token_offset.value_or(0),
        .context_saved_fn = &context_saved_fn};
    chrome_ml_->api().ExecuteModel(model_, &options,
                                   context_holder->GetCancelFn());
    context_holders_.insert(std::move(context_holder));
  }

  void Execute(on_device_model::mojom::InputOptionsPtr input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   response) override {
    responder_ = std::make_unique<Responder>(std::move(response));
    ChromeMLOutputFn output_fn = responder_->CreateOutputFn();
    ChromeMLCompletionFn completion_fn = responder_->CreateCompletionFn();
    std::string adjusted_input = input->text + " <ctrl23>";
    ChromeMLExecuteOptions options{
        .prompt = adjusted_input.c_str(),
        .context_mode = GetContextMode(*input),
        .max_tokens = input->max_tokens.value_or(0),
        .token_offset = input->token_offset.value_or(0),
        .output_fn = &output_fn,
        .completion_fn = &completion_fn,
    };
    chrome_ml_->api().ExecuteModel(model_, &options, responder_->GetCancelFn());
  }

 private:
  void RemoveContext(ContextHolder* context) {
    std::erase_if(context_holders_, base::MatchesUniquePtr(context));
  }

  int GetContextMode(const on_device_model::mojom::InputOptions& input) {
    int context_mode = ContextMode::kNone;
    if (input.ignore_context) {
      context_mode |= ContextMode::kIgnoreContext;
    }
    if (first_prompt_) {
      first_prompt_ = false;
      return context_mode | ContextMode::kReset;
    }
    return context_mode;
  }

  bool first_prompt_ = true;
  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLModel model_;
  std::unique_ptr<Responder> responder_;
  std::set<std::unique_ptr<ContextHolder>> context_holders_;
};

}  // namespace

OnDeviceModelExecutor::OnDeviceModelExecutor(
    base::PassKey<OnDeviceModelExecutor>,
    const ChromeML& chrome_ml)
    : chrome_ml_(chrome_ml),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

OnDeviceModelExecutor::~OnDeviceModelExecutor() {
  if (model_ != 0) {
    chrome_ml_->api().DestroyModel(model_);
  }
}

// static
base::expected<std::unique_ptr<OnDeviceModelExecutor>, LoadModelResult>
OnDeviceModelExecutor::CreateWithResult(
    const ChromeML& chrome_ml,
    on_device_model::mojom::LoadModelParamsPtr params) {
  auto executor = std::make_unique<OnDeviceModelExecutor>(
      base::PassKey<OnDeviceModelExecutor>(), chrome_ml);
  auto load_model_result = executor->Init(std::move(params));
  if (load_model_result == LoadModelResult::kSuccess) {
    return base::ok<std::unique_ptr<OnDeviceModelExecutor>>(
        std::move(executor));
  }
  return base::unexpected(load_model_result);
}

std::unique_ptr<on_device_model::OnDeviceModel::Session>
OnDeviceModelExecutor::CreateSession() {
  return std::make_unique<SessionImpl>(*chrome_ml_, model_);
}

bool OnDeviceModelExecutor::IsGpuBlocked() {
  GpuConfig gpu_config;
  if (!chrome_ml_->api().GetGpuConfig(gpu_config)) {
    LOG(ERROR) << "Unable to get gpu config";
    return true;
  }
  WGPUAdapterProperties wgpu_adapter_properties = {};
  wgpu_adapter_properties.vendorID = gpu_config.vendor_id;
  wgpu_adapter_properties.deviceID = gpu_config.device_id;
  wgpu_adapter_properties.architecture = gpu_config.architecture;
  wgpu_adapter_properties.driverDescription = gpu_config.driver_description;
  wgpu_adapter_properties.adapterType = gpu_config.adapter_type;
  wgpu_adapter_properties.backendType = gpu_config.backend_type;
  if (gpu::IsWebGPUAdapterBlocklisted(wgpu_adapter_properties,
                                      kGpuBlockList.Get())) {
    LOG(ERROR) << "WebGPU blocked on this device";
    return true;
  }
  return false;
}

LoadModelResult OnDeviceModelExecutor::Init(
    on_device_model::mojom::LoadModelParamsPtr params) {
  if (IsGpuBlocked()) {
    return LoadModelResult::kGpuBlocked;
  }
  on_device_model::ModelAssets assets = std::move(params->assets);
  sentencepiece_model_proto_ = std::make_unique<base::MemoryMappedFile>();
  if (!assets.sp_model.IsValid() ||
      !sentencepiece_model_proto_->Initialize(std::move(assets.sp_model))) {
    LOG(ERROR) << "Unable to load sentencepiece model";
    return LoadModelResult::kFailedToLoadLibrary;
  }

  model_proto_ = std::make_unique<base::MemoryMappedFile>();
  if (!assets.model.IsValid() ||
      !model_proto_->Initialize(std::move(assets.model))) {
    LOG(ERROR) << "Unable to load model";
    return LoadModelResult::kFailedToLoadLibrary;
  }

  weights_ = std::make_unique<base::MemoryMappedFile>();
  if (!assets.weights.IsValid() ||
      !weights_->Initialize(std::move(assets.weights),
                            base::MemoryMappedFile::READ_WRITE_COPY)) {
    LOG(ERROR) << "Unable to load weights";
    return LoadModelResult::kFailedToLoadLibrary;
  }

  if (assets.ts_data.IsValid()) {
    if (!ts_data_.Initialize(std::move(assets.ts_data)) ||
        !assets.ts_sp_model.IsValid() ||
        !ts_sp_model_.Initialize(std::move(assets.ts_sp_model))) {
      LOG(ERROR) << "Invalid TS model data supplied";
      return LoadModelResult::kFailedToLoadLibrary;
    }
  }

  auto model_proto_dispose =
      CreateWeakCallbackFn(&OnDeviceModelExecutor::DisposeModelProto, this);
  auto weights_dispose =
      CreateWeakCallbackFn(&OnDeviceModelExecutor::DisposeWeights, this);
  const ChromeMLModelData data = {
      .model_proto_data = model_proto_->data(),
      .model_proto_size = model_proto_->length(),
      .model_proto_dispose = &model_proto_dispose,
      .weights_data = weights_->mutable_bytes().data(),
      .weights_size = weights_->length(),
      .weights_dispose = &weights_dispose,
  };
  auto sentencepiece_model_proto_dispose =
      CreateWeakCallbackFn(&OnDeviceModelExecutor::DisposeSentencepiece, this);
  ChromeMLModelDescriptor descriptor = {
      .sentencepiece_model_proto_data = sentencepiece_model_proto_->data(),
      .sentencepiece_model_proto_size = sentencepiece_model_proto_->length(),
      .sentencepiece_model_proto_dispose = &sentencepiece_model_proto_dispose,
      .model_data = &data,
      .max_tokens = params->max_tokens,
  };
  if (ts_data_.IsValid()) {
    CHECK(ts_sp_model_.IsValid());
    descriptor.ts_data = ts_data_.data();
    descriptor.ts_size = ts_data_.length();
    descriptor.ts_spm_data = ts_sp_model_.data();
    descriptor.ts_spm_size = ts_sp_model_.length();
  };
  model_ = chrome_ml_->api().CreateModel(&descriptor,
                                         reinterpret_cast<uintptr_t>(this),
                                         OnDeviceModelExecutor::Schedule);
  return (model_ != 0) ? LoadModelResult::kSuccess
                       : LoadModelResult::kFailedToLoadLibrary;
}

void OnDeviceModelExecutor::DisposeSentencepiece() {
  sentencepiece_model_proto_ = nullptr;
}

void OnDeviceModelExecutor::DisposeModelProto() {
  model_proto_ = nullptr;
}

void OnDeviceModelExecutor::DisposeWeights() {
  weights_ = nullptr;
}

// static
void OnDeviceModelExecutor::Schedule(uintptr_t context,
                                     std::function<void()>* fn) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce([](std::function<void()> fn) { fn(); }, std::move(*fn)));
}

}  // namespace ml
