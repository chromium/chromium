// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/on_device_model_executor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/language_detector.h"
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

const base::FeatureParam<bool> kPreferTextureWeights{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_prefer_texture_weights", true};

const base::FeatureParam<bool> kEnableHostMappedPointer{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_enable_host_mapped_pointer", true};

const base::FeatureParam<bool> kUseLowPower{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_use_low_power", false};

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
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> responder,
      scoped_refptr<LanguageDetector> language_detector)
      : responder_(std::move(responder)),
        language_detector_(std::move(language_detector)) {
    responder_.set_disconnect_handler(
        base::BindOnce(&Responder::Cancel, base::Unretained(this)));
  }
  ~Responder() { Cancel(); }

  ChromeMLCancelFn* GetCancelFn() { return &cancel_; }

  ChromeMLExecutionOutputFn CreateOutputFn() {
    return [weak_ptr = AsWeakPtr(),
            task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
               const ChromeMLExecutionOutput* output) {
      std::optional<std::string> text;
      std::optional<std::vector<float>> class_scores;
      switch (output->status) {
        case ChromeMLExecutionStatus::kInProgress:
          CHECK(output->text);
          text.emplace(output->text);
          break;
        case ChromeMLExecutionStatus::kComplete:
          DCHECK(!output->text);
          break;
      }

      if (output->ts_scores) {
        class_scores.emplace(output->ts_scores,
                             output->ts_scores + output->num_ts_scores);
      }

      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&Responder::OnOutput, weak_ptr,
                                    std::move(text), std::move(class_scores)));
    };
  }

 private:
  void OnOutput(std::optional<std::string> text,
                std::optional<std::vector<float>> class_scores) {
    if (text) {
      num_tokens_++;
      output_so_far_ += *text;
      if (first_token_time_ == base::TimeTicks()) {
        first_token_time_ = base::TimeTicks::Now();
      }

      auto chunk = on_device_model::mojom::ResponseChunk::New();
      chunk->text = *text;
      chunk->safety_info = CreateSafetyInfo(output_so_far_, class_scores);
      responder_->OnResponse(std::move(chunk));
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

      auto summary = on_device_model::mojom::ResponseSummary::New();
      summary->safety_info = CreateSafetyInfo(output_so_far_, class_scores);
      responder_->OnComplete(std::move(summary));
    }
  }

  on_device_model::mojom::SafetyInfoPtr CreateSafetyInfo(
      std::string_view text,
      std::optional<std::vector<float>>& class_scores) {
    if (!class_scores) {
      return nullptr;
    }

    auto safety_info = on_device_model::mojom::SafetyInfo::New();
    safety_info->class_scores = std::move(*class_scores);
    if (language_detector_) {
      safety_info->language = language_detector_->DetectLanguage(text);
    }
    return safety_info;
  }

  void Cancel() {
    if (cancel_) {
      cancel_();
    }
  }

  base::TimeTicks first_token_time_;
  int num_tokens_ = 0;
  std::string output_so_far_;
  mojo::Remote<on_device_model::mojom::StreamingResponder> responder_;
  const scoped_refptr<LanguageDetector> language_detector_;
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
  SessionImpl(const ChromeML& chrome_ml,
              ChromeMLModel model,
              scoped_refptr<LanguageDetector> language_detector,
              std::optional<uint32_t> adaptation_id)
      : chrome_ml_(chrome_ml),
        model_(model),
        language_detector_(std::move(language_detector)),
        adaptation_id_(adaptation_id) {}
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
    if (adaptation_id_) {
      options.adaptation_id = &adaptation_id_.value();
    }
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
    responder_ =
        std::make_unique<Responder>(std::move(response), language_detector_);
    ChromeMLExecutionOutputFn output_fn = responder_->CreateOutputFn();
    int32_t ts_interval = -1;
    if (input->safety_interval.has_value()) {
      ts_interval =
          base::saturated_cast<int32_t>(input->safety_interval.value());
    }
    ChromeMLExecuteOptions options{
        .prompt = input->text.c_str(),
        .context_mode = GetContextMode(*input),
        .max_tokens = input->max_tokens.value_or(0),
        .token_offset = input->token_offset.value_or(0),
        .max_output_tokens = input->max_output_tokens.value_or(0),
        .score_ts_interval = ts_interval,
        .execution_output_fn = &output_fn};
    if (adaptation_id_) {
      options.adaptation_id = &adaptation_id_.value();
    }
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
  const scoped_refptr<LanguageDetector> language_detector_;
  std::unique_ptr<Responder> responder_;
  std::set<std::unique_ptr<ContextHolder>> context_holders_;
  std::optional<uint32_t> adaptation_id_;
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
OnDeviceModelExecutor::CreateSession(std::optional<uint32_t> adaptation_id) {
  return std::make_unique<SessionImpl>(*chrome_ml_, model_, language_detector_,
                                       adaptation_id);
}

DISABLE_CFI_DLSYM
base::expected<uint32_t, LoadModelResult> OnDeviceModelExecutor::LoadAdaptation(
    on_device_model::mojom::LoadAdaptationParamsPtr params) {
  if (!chrome_ml_->api().CreateAdaptation) {
    return base::unexpected(LoadModelResult::kFailedToLoadLibrary);
  }

  on_device_model::AdaptationAssets assets = std::move(params->assets);
  auto model_proto = std::make_unique<base::MemoryMappedFile>();
  if (!assets.model.IsValid() ||
      !model_proto->Initialize(std::move(assets.model))) {
    LOG(ERROR) << "Unable to load model";
    return base::unexpected(LoadModelResult::kFailedToLoadLibrary);
  }

  uint32_t id;
  const ChromeMLModelData data = {
      .model_proto_data = model_proto->data(),
      .model_proto_size = model_proto->length(),
      .weights_file = assets.weights.TakePlatformFile(),
  };
  ChromeMLAdaptationDescriptor descriptor = {
      .model_data = &data,
  };
  if (!chrome_ml_->api().CreateAdaptation(model_, &descriptor, id)) {
    return base::unexpected(LoadModelResult::kFailedToLoadLibrary);
  }
  adaptation_data_.push_back(std::move(model_proto));
  return base::ok(id);
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

  if (assets.ts_data.IsValid()) {
    if (!ts_data_.Initialize(std::move(assets.ts_data)) ||
        !assets.ts_sp_model.IsValid() ||
        !ts_sp_model_.Initialize(std::move(assets.ts_sp_model))) {
      LOG(ERROR) << "Invalid TS model data supplied";
      return LoadModelResult::kFailedToLoadLibrary;
    }
  }

  if (assets.language_detection_model.IsValid()) {
    language_detector_ =
        LanguageDetector::Create(std::move(assets.language_detection_model));
    if (!language_detector_) {
      LOG(ERROR) << "Failed to initialize language detection";
      return LoadModelResult::kFailedToLoadLibrary;
    }
  }

  auto model_proto_dispose =
      CreateWeakCallbackFn(&OnDeviceModelExecutor::DisposeModelProto, this);
  const ChromeMLModelData data = {
      .model_proto_data = model_proto_->data(),
      .model_proto_size = model_proto_->length(),
      .model_proto_dispose = &model_proto_dispose,
      .weights_file = assets.weights.TakePlatformFile(),
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
      .ts_dimension = params->ts_dimension.value_or(0),
      .adaptation_ranks = params->adaptation_ranks.data(),
      .adaptation_ranks_size = params->adaptation_ranks.size(),
      .prefer_texture_weights = kPreferTextureWeights.Get(),
      .enable_host_mapped_pointer = kEnableHostMappedPointer.Get(),
      .use_low_power = kUseLowPower.Get(),
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

// static
void OnDeviceModelExecutor::Schedule(uintptr_t context,
                                     std::function<void()>* fn) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce([](std::function<void()> fn) { fn(); }, std::move(*fn)));
}

}  // namespace ml
