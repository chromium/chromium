// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/on_device_model_executor.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

using on_device_model::mojom::LoadModelResult;

namespace ml {
namespace {

const base::FeatureParam<double> kTemperature{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_temperature", 0.2};

const base::FeatureParam<int> kTopK{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_topk", 3};

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

int CalculateTokensPerSecond(int num_tokens, base::TimeDelta duration) {
  if (duration.InMicroseconds() <= 0) {
    return 0;
  }
  return (num_tokens / static_cast<float>(duration.InMicroseconds())) *
         base::Time::kMicrosecondsPerSecond;
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
      num_tokens_++;
      if (first_token_time_ == base::TimeTicks()) {
        first_token_time_ = base::TimeTicks::Now();
      }
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
      base::UmaHistogramCounts10000("OnDeviceModel.TokenCount.Output",
                                    num_tokens_);
      if (num_tokens_ > 1) {
        // Time starts at the first token to avoid counting input processing
        // time, so calculate using num_tokens_ - 1.
        base::UmaHistogramCounts1000(
            "OnDeviceModel.TokensPerSecond.Output",
            CalculateTokensPerSecond(
                num_tokens_ - 1, base::TimeTicks::Now() - first_token_time_));
      }
      responder_->OnComplete(ResponseStatus::kOk);
    }
  }

  void Cancel() {
    if (cancel_) {
      cancel_();
    }
  }

  base::TimeTicks first_token_time_;
  int num_tokens_ = 0;
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
    return CreateWeakCallbackFn(&ContextHolder::OnComplete, this);
  }

 private:
  void OnComplete(int tokens_processed) {
    if (tokens_processed > 0) {
      base::UmaHistogramCounts10000("OnDeviceModel.TokenCount.Context",
                                    tokens_processed);
      base::UmaHistogramCounts10000(
          "OnDeviceModel.TokensPerSecond.Context",
          CalculateTokensPerSecond(tokens_processed, timer_.Elapsed()));
    }
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

  base::ElapsedTimer timer_;
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

  DISABLE_CFI_DLSYM
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
    // Once we have added context, it should not be cleared.
    clear_context_ = false;
  }

  DISABLE_CFI_DLSYM
  void Execute(on_device_model::mojom::InputOptionsPtr input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   response) override {
    responder_ = std::make_unique<Responder>(std::move(response));
    ChromeMLOutputFn output_fn = responder_->CreateOutputFn();
    ChromeMLCompletionFn completion_fn = responder_->CreateCompletionFn();
    ChromeMLExecuteOptions options{
        .prompt = input->text.c_str(),
        .context_mode = GetContextMode(*input),
        .max_tokens = input->max_tokens.value_or(0),
        .token_offset = input->token_offset.value_or(0),
        .max_output_tokens = input->max_output_tokens.value_or(0),
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
    if (clear_context_) {
      context_mode |= ContextMode::kReset;
    }
    return context_mode;
  }

  bool clear_context_ = true;
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
LoadModelResult OnDeviceModelExecutor::Init(
    on_device_model::mojom::LoadModelParamsPtr params) {
  if (chrome_ml_->IsGpuBlocked()) {
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
      .temperature = static_cast<float>(kTemperature.Get()),
      .top_k = kTopK.Get(),
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
