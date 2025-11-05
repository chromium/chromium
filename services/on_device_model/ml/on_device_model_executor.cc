// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/on_device_model_executor.h"

#include <algorithm>
#include <cstdint>
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
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"
#include "services/on_device_model/ml/performance_class.h"
#include "services/on_device_model/ml/session_accessor.h"
#include "services/on_device_model/public/cpp/cpu.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
#include "third_party/rust/chromium_crates_io/vendor/llguidance-v1/llguidance.h"
#endif

namespace ml {
namespace {

namespace odmm = ::on_device_model::mojom;
using odmm::LoadModelResult;

constexpr uint32_t kReserveTokensForSafety = 2;

const base::FeatureParam<bool> kPreferTextureWeights{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_prefer_texture_weights", true};

const base::FeatureParam<bool> kEnableHostMappedPointer{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_enable_host_mapped_pointer", true};

const base::FeatureParam<bool> kUseLowPower{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_use_low_power", false};

const base::FeatureParam<bool> kAllowFp16{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_allow_fp16", true};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// This is a copy of the XNNPackCacheHeader struct from
// third_party/tflite/src/tensorflow/lite/delegates/xnnpack/weight_cache.h
// We can't include that header directly because of linter issues.
// TODO(crbug.com/447174993): Remove once xnnpack includes cb018b2d.
struct XNNPackCacheHeader {
  enum : uint64_t { kInvalidHeader = 0, kVersion = 1 };
  uint64_t version;
  uint8_t xnnpack_build_identifier[32];
  uint64_t buffer_list_offset;
  uint64_t buffer_list_size;
};

// Truncates the given xnnpack cache file if it is not compatible with the
// current build.
void MaybeDeleteCacheFile(base::File& cache_file) {
  if (!cache_file.IsValid()) {
    return;
  }
  XNNPackCacheHeader header;
  if (cache_file.GetLength() < static_cast<int64_t>(sizeof(header))) {
    return;
  }
  // SAFETY: `header` is stack-allocated and guaranteed to be non-null.
  auto header_span = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<uint8_t*>(&header), sizeof(header)));
  if (!cache_file.ReadAndCheck(0, header_span)) {
    return;
  }
  if (header.version == XNNPackCacheHeader::kVersion &&
      xnn_experimental_check_build_identifier(
          header.xnnpack_build_identifier,
          sizeof(header.xnnpack_build_identifier))) {
    return;
  }
  cache_file.SetLength(0);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

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

// Helper to convert a OnceCallback to std::function.
template <typename R, typename... Args>
std::function<R(Args...)> ConvertCallbackToFn(
    base::OnceCallback<R(Args...)> callback) {
  auto shared_callback =
      std::make_shared<base::OnceCallback<R(Args...)>>(std::move(callback));
  return [shared_callback = shared_callback,
          task_runner =
              base::SequencedTaskRunner::GetCurrentDefault()](Args&&... args) {
    if (!shared_callback->is_null()) {
      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(std::move(*shared_callback),
                                           std::forward<Args>(args)...));
    }
  };
}

int CalculateTokensPerSecond(int num_tokens, base::TimeDelta duration) {
  if (duration.InMicroseconds() <= 0) {
    return 0;
  }
  return (num_tokens / static_cast<float>(duration.InMicroseconds())) *
         base::Time::kMicrosecondsPerSecond;
}

// If `pieces` ends with ml::Token::kModel and then a text piece, returns the
// final text piece.
std::optional<std::string> GetModelResponsePrefix(
    const std::vector<InputPiece>& pieces) {
  if (pieces.size() < 2) {
    return std::nullopt;
  }
  if (const ml::Token* token =
          std::get_if<ml::Token>(&pieces[pieces.size() - 2]);
      !token || *token != ml::Token::kModel) {
    return std::nullopt;
  }
  if (const std::string* text = std::get_if<std::string>(&pieces.back())) {
    return *text;
  }
  return std::nullopt;
}

}  // namespace

// Handles sending and canceling responses.
class Responder final {
 public:
  explicit Responder(
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> responder,
      base::OnceClosure on_complete,
      SessionAccessor::Ptr session)
      : responder_(std::move(responder)),
        on_complete_(std::move(on_complete)),
        session_(std::move(session)) {
    responder_.set_disconnect_handler(
        base::BindOnce(&Responder::Cancel, base::Unretained(this)));
  }
  ~Responder() { Cancel(); }

  ChromeMLCancelFn* GetCancelFn() { return &cancel_; }

  ChromeMLExecutionOutputFn CreateOutputFn() {
    return [weak_ptr = weak_ptr_factory_.GetWeakPtr(),
            task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
               const ChromeMLExecutionOutput* output) {
      std::optional<std::string> text;
      switch (output->status) {
        case ChromeMLExecutionStatus::kInProgress:
          CHECK(output->text);
          text.emplace(output->text);
          break;
        case ChromeMLExecutionStatus::kComplete:
          DCHECK(!output->text);
          break;
      }

      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&Responder::OnOutput, weak_ptr, std::move(text)));
    };
  }

  base::WeakPtr<Responder> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnOutput(std::optional<std::string> text) {
    if (text) {
      num_output_tokens_++;
      output_so_far_ += *text;
      if (first_token_time_ == base::TimeTicks()) {
        first_token_time_ = base::TimeTicks::Now();
      }

      auto chunk = on_device_model::mojom::ResponseChunk::New();
      chunk->text = *text;
      responder_->OnResponse(std::move(chunk));
    } else if (session_) {
      // Empty text means the output is finished. Delete the session immediately
      // to free up any resources.
      session_ = nullptr;
      base::UmaHistogramCounts10000("OnDeviceModel.TokenCount.Output",
                                    num_output_tokens_);
      if (num_output_tokens_ > 1) {
        // Time starts at the first token to avoid counting input processing
        // time, so calculate using num_output_tokens_ - 1.
        base::UmaHistogramCounts1000(
            "OnDeviceModel.TokensPerSecond.Output",
            CalculateTokensPerSecond(
                num_output_tokens_ - 1,
                base::TimeTicks::Now() - first_token_time_));
      }

      auto summary = on_device_model::mojom::ResponseSummary::New();
      summary->output_token_count = num_output_tokens_;
      responder_->OnComplete(std::move(summary));
      if (!on_complete_.is_null()) {
        std::move(on_complete_).Run();
      }
    }
  }

  void Cancel() {
    session_ = nullptr;
    if (cancel_) {
      cancel_();
    }
    if (!on_complete_.is_null()) {
      std::move(on_complete_).Run();
    }
  }

  base::ElapsedTimer timer_;
  base::TimeTicks first_token_time_;
  int num_output_tokens_ = 0;
  std::string output_so_far_;
  mojo::Remote<on_device_model::mojom::StreamingResponder> responder_;
  ChromeMLCancelFn cancel_;
  base::OnceClosure on_complete_;
  SessionAccessor::Ptr session_;
  base::WeakPtrFactory<Responder> weak_ptr_factory_{this};
};

// Handles sending and cancelling ASR streams.
class AsrStreamResponder final {
 public:
  explicit AsrStreamResponder(
      mojo::PendingRemote<odmm::AsrStreamResponder> responder,
      SessionAccessor::Ptr session)
      : responder_(std::move(responder)), session_(std::move(session)) {
    responder_.set_disconnect_handler(
        base::BindOnce(&AsrStreamResponder::Cancel, base::Unretained(this)));
  }
  ~AsrStreamResponder() { Cancel(); }
  SessionAccessor* session() { return session_.get(); }
  ChromeMLASRStreamOutputFn CreateOutputFn() {
    return [weak_ptr = weak_ptr_factory_.GetWeakPtr(),
            task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
               const ChromeMLASRStreamOutput& output) {
      std::vector<odmm::SpeechRecognitionResultPtr> result;
      result.reserve(output.size());
      for (const auto& t : output) {
        result.push_back(
            odmm::SpeechRecognitionResult::New(t.transcript, t.is_final));
      }
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&AsrStreamResponder::OnOutput, weak_ptr,
                                    std::move(result)));
    };
  }

  base::WeakPtr<AsrStreamResponder> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void Cancel() { session_ = nullptr; }
  void OnOutput(std::vector<odmm::SpeechRecognitionResultPtr> output) {
    responder_->OnResponse(std::move(output));
  }
  mojo::Remote<odmm::AsrStreamResponder> responder_;
  SessionAccessor::Ptr session_;
  base::WeakPtrFactory<AsrStreamResponder> weak_ptr_factory_{this};
};

// Handles calling the ContextClient on completion and canceling the context
// request.
class ContextHolder final {
 public:
  explicit ContextHolder(
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
      base::OnceCallback<void(ContextHolder*)> on_disconnect,
      base::OnceClosure on_complete)
      : client_(std::move(client)),
        on_disconnect_(std::move(on_disconnect)),
        on_complete_(std::move(on_complete)) {
    if (client_) {
      client_.set_disconnect_handler(
          base::BindOnce(&ContextHolder::OnDisconnect, base::Unretained(this)));
    }
  }
  ~ContextHolder() {
    if (cancel_) {
      cancel_();
    }
    if (!on_complete_.is_null()) {
      std::move(on_complete_).Run();
    }
  }

  ChromeMLCancelFn* GetCancelFn() { return &cancel_; }

  ChromeMLContextSavedFn CreateContextSavedFn() {
    return CreateWeakCallbackFn(&ContextHolder::OnComplete, this);
  }

  base::WeakPtr<ContextHolder> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnComplete(int tokens_processed) {
    TRACE_EVENT("optimization_guide", "ContextHolder::OnComplete");
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
    if (!on_complete_.is_null()) {
      std::move(on_complete_).Run();
    }
    OnDisconnect();
  }

  void OnDisconnect() {
    TRACE_EVENT("optimization_guide", "ContextHolder::OnDisconnect");
    if (on_disconnect_) {
      std::move(on_disconnect_).Run(this);
    }
    // this may be deleted.
  }

  base::ElapsedTimer timer_;
  mojo::Remote<on_device_model::mojom::ContextClient> client_;
  base::OnceCallback<void(ContextHolder*)> on_disconnect_;
  ChromeMLCancelFn cancel_;
  base::OnceClosure on_complete_;
  base::WeakPtrFactory<ContextHolder> weak_ptr_factory_{this};
};

BackendImpl::BackendImpl(const ml::ChromeML* chrome_ml)
    : chrome_ml_(chrome_ml), ts_holder_(ml::TsHolder::Create(*chrome_ml_)) {}

base::expected<void, on_device_model::ServiceDisconnectReason>
BackendImpl::CanCreate() {
  if (!chrome_ml_) {
    return base::unexpected(
        on_device_model::ServiceDisconnectReason::kFailedToLoadLibrary);
  }
  ml::DeviceInfo device_info =
      ml::QueryDeviceInfo(chrome_ml_->api(), /*log_histogram=*/false);
  if (!on_device_model::IsCpuCapable() &&
      device_info.gpu_blocked_reason != GpuBlockedReason::kNotBlocked) {
    return base::unexpected(
        on_device_model::ServiceDisconnectReason::kGpuBlocked);
  }
  return base::ok();
}

DISABLE_CFI_DLSYM
on_device_model::Capabilities BackendImpl::GetCapabilities(
    on_device_model::ModelFile model_file) {
  TRACE_EVENT("optimization_guide", "BackendImpl::GetCapabilities");
  on_device_model::Capabilities result;
  if (!chrome_ml_->api().GetCapabilities) {
    return result;
  }

  PlatformFile platform_file;
  if (model_file.IsFile()) {
    platform_file = model_file.file().TakePlatformFile();
  } else {
    base::File file(model_file.path(),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    platform_file = file.TakePlatformFile();
  }
  ChromeMLCapabilities capabilities;
  chrome_ml_->api().GetCapabilities(platform_file, capabilities);

  if (capabilities.image_input) {
    result.Put(on_device_model::CapabilityFlags::kImageInput);
  }
  if (capabilities.audio_input) {
    result.Put(on_device_model::CapabilityFlags::kAudioInput);
  }
  return result;
}

base::expected<std::unique_ptr<on_device_model::BackendModel>,
               on_device_model::mojom::LoadModelResult>
BackendImpl::CreateWithResult(on_device_model::mojom::LoadModelParamsPtr params,
                              base::OnceClosure on_complete) {
  return OnDeviceModelExecutor::CreateWithResult(*chrome_ml_, std::move(params),
                                                 std::move(on_complete));
}

void BackendImpl::LoadTextSafetyModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model) {
  TRACE_EVENT("optimization_guide", "BackendImpl::LoadTextSafetyModel");
  ts_holder_.AsyncCall(&ml::TsHolder::Reset)
      .WithArgs(std::move(params), std::move(model));
}

std::pair<on_device_model::mojom::DevicePerformanceInfoPtr,
          on_device_model::mojom::DeviceInfoPtr>
BackendImpl::GetDeviceAndPerformanceInfo() {
  return ml::GetDeviceAndPerformanceInfo(*chrome_ml_);
}

BackendImpl::~BackendImpl() = default;

SessionImpl::SessionImpl(const ChromeML& chrome_ml,
                         OnDeviceModelExecutor& executor,
                         SessionAccessor::Ptr session,
                         uint32_t max_tokens,
                         std::optional<uint32_t> adaptation_id)
    : chrome_ml_(chrome_ml),
      executor_(executor),
      session_(std::move(session)),
      max_tokens_(max_tokens),
      adaptation_id_(adaptation_id) {}
SessionImpl::~SessionImpl() = default;

DISABLE_CFI_DLSYM
void SessionImpl::Append(
    on_device_model::mojom::AppendOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide", "SessionImpl::Append");
  model_response_prefix_ = GetModelResponsePrefix(options->input->pieces);
  auto context_holder = std::make_unique<ContextHolder>(
      std::move(client),
      base::BindOnce(&SessionImpl::RemoveContext, base::Unretained(this)),
      std::move(on_complete));
  if (options->max_tokens == 0 || options->max_tokens > max_tokens_) {
    options->max_tokens = max_tokens_;
  }
  ChromeMLContextSavedFn context_saved_fn =
      context_holder->CreateContextSavedFn();
  *context_holder->GetCancelFn() =
      session_->Append(std::move(options), context_saved_fn);
  context_holders_.insert(std::move(context_holder));
}

DISABLE_CFI_DLSYM
void SessionImpl::Generate(
    on_device_model::mojom::GenerateOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide", "SessionImpl::Generate");
  auto cloned = session_->Clone();
  auto cloned_raw = cloned.get();  // For Generate after std::move
  responder_ = std::make_unique<Responder>(
      std::move(response), std::move(on_complete), std::move(cloned));
  ChromeMLExecutionOutputFn output_fn = responder_->CreateOutputFn();
  ChromeMLConstraint constraint = 0;
  if (options->constraint) {
    constraint = executor_->CreateConstraint(*options->constraint,
                                             model_response_prefix_);
    if (!constraint) {
      // TODO(crbug.com/391919456): Propagate error.
      responder_.reset();
      return;
    }
  }
  *responder_->GetCancelFn() =
      cloned_raw->Generate(std::move(options), constraint, output_fn);
}

DISABLE_CFI_DLSYM
void SessionImpl::SizeInTokens(on_device_model::mojom::InputPtr input,
                               base::OnceCallback<void(uint32_t)> callback) {
  TRACE_EVENT("optimization_guide", "SessionImpl::SizeInTokens");
  session_->SizeInTokens(std::move(input),
                         ConvertCallbackToFn(std::move(callback)));
}

DISABLE_CFI_DLSYM
void SessionImpl::Score(const std::string& text,
                        base::OnceCallback<void(float)> callback) {
  TRACE_EVENT("optimization_guide", "SessionImpl::Score");
  session_->Score(text, ConvertCallbackToFn(std::move(callback)));
}

DISABLE_CFI_DLSYM
void SessionImpl::GetProbabilitiesBlocking(
    const std::string& input,
    base::OnceCallback<void(const std::vector<float>&)> callback) {
  TRACE_EVENT("optimization_guide", "SessionImpl::GetProbabilitiesBlocking");
  session_->GetProbabilitiesBlocking(input,
                                     ConvertCallbackToFn(std::move(callback)));
}

DISABLE_CFI_DLSYM
void SessionImpl::AsrStream(
    odmm::AsrStreamOptionsPtr options,
    mojo::PendingRemote<odmm::AsrStreamResponder> responder) {
  TRACE_EVENT("optimization_guide", "SessionImpl::AsrStream");
  DCHECK_EQ(asr_responder_, nullptr);
  auto cloned = session_->Clone();
  auto cloned_raw = cloned.get();  // For CreateAsrStream after std::move
  asr_responder_ = std::make_unique<AsrStreamResponder>(std::move(responder),
                                                        std::move(cloned));
  ChromeMLASRStreamOutputFn output_fn = asr_responder_->CreateOutputFn();
  cloned_raw->CreateAsrStream(std::move(options), output_fn);
}

DISABLE_CFI_DLSYM
void SessionImpl::AsrAddAudioChunk(odmm::AudioDataPtr data) {
  TRACE_EVENT("optimization_guide.debug", "SessionImpl::AsrAddAudioChunk");
  if (!asr_responder_) {
    return;
  }
  asr_responder_->session()->AsrAddAudioChunk(std::move(data));
}

std::unique_ptr<on_device_model::BackendSession> SessionImpl::Clone() {
  TRACE_EVENT("optimization_guide", "SessionImpl::Clone");
  return std::make_unique<SessionImpl>(chrome_ml_.get(), *executor_,
                                       session_->Clone(), max_tokens_,
                                       adaptation_id_);
}

void SessionImpl::RemoveContext(ContextHolder* context) {
  TRACE_EVENT("optimization_guide", "SessionImpl::RemoveContext");
  std::erase_if(context_holders_, base::MatchesUniquePtr(context));
}

DISABLE_CFI_DLSYM
void DestroyModel(const ChromeML* chrome_ml, ChromeMLModel model) {
  TRACE_EVENT("optimization_guide", "DestroyModel");
  chrome_ml->api().DestroyModel(model);
}

OnDeviceModelExecutor::OnDeviceModelExecutor(
    base::PassKey<OnDeviceModelExecutor>,
    const ChromeML& chrome_ml)
    : chrome_ml_(chrome_ml),
      model_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

OnDeviceModelExecutor::~OnDeviceModelExecutor() {
  TRACE_EVENT("optimization_guide",
              "OnDeviceModelExecutor::~OnDeviceModelExecutor");
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (tokenizer_ != nullptr) {
    llg_free_tokenizer(tokenizer_);
  }
#endif
  if (model_ != 0) {
    model_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DestroyModel, &chrome_ml_.get(), model_));
  }
}

// static
base::expected<std::unique_ptr<OnDeviceModelExecutor>, LoadModelResult>
OnDeviceModelExecutor::CreateWithResult(
    const ChromeML& chrome_ml,
    on_device_model::mojom::LoadModelParamsPtr params,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::CreateWithResult");
  auto executor = std::make_unique<OnDeviceModelExecutor>(
      base::PassKey<OnDeviceModelExecutor>(), chrome_ml);
  auto load_model_result =
      executor->Init(std::move(params), std::move(on_complete));
  if (load_model_result == LoadModelResult::kSuccess) {
    return base::ok<std::unique_ptr<OnDeviceModelExecutor>>(
        std::move(executor));
  }
  return base::unexpected(load_model_result);
}

std::unique_ptr<on_device_model::BackendSession>
OnDeviceModelExecutor::CreateSession(
    const ScopedAdaptation* adaptation,
    on_device_model::mojom::SessionParamsPtr params) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::CreateSession");
  std::optional<uint32_t> adaptation_id;
  on_device_model::mojom::LoadAdaptationParamsPtr adaptation_params;
  if (adaptation) {
    adaptation_id = adaptation->adaptation_id();
    auto it = adaptation_params_.find(*adaptation_id);
    CHECK(it != adaptation_params_.end());
    adaptation_params = it->second->Clone();
  }
  auto session = SessionAccessor::Create(
      *chrome_ml_, model_task_runner_, model_, std::move(params),
      std::move(adaptation_params), adaptation_id);
  return std::make_unique<SessionImpl>(*chrome_ml_, *this, std::move(session),
                                       max_tokens_ - kReserveTokensForSafety,
                                       adaptation_id);
}

std::unique_ptr<OnDeviceModelExecutor::ScopedAdaptation>
OnDeviceModelExecutor::LoadAdaptation(
    on_device_model::mojom::LoadAdaptationParamsPtr params) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::LoadAdaptation");
  adaptation_params_.insert({next_adaptation_id_, std::move(params)});
  return std::make_unique<ScopedAdaptation>(weak_ptr_factory_.GetWeakPtr(),
                                            next_adaptation_id_++);
}

void OnDeviceModelExecutor::UnloadAdaptation(uint32_t adaptation_id) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::UnloadAdaptation");
  adaptation_params_.erase(adaptation_id);
}

DISABLE_CFI_DLSYM
ChromeMLConstraint OnDeviceModelExecutor::CreateConstraint(
    const on_device_model::mojom::ResponseConstraint& response_constraint,
    const std::optional<std::string>& prefix) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::CreateConstraint");
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (!tokenizer_) {
    CHECK(chrome_ml_->api().GetTokenizerParams(
        model_, [&](const ChromeMLTokenizerParams& params) {
          LlgTokenizerInit tokenizer_init{
              .vocab_size = params.vocab_size,
              .tok_eos = params.eos_token_id,
              .token_lens = params.token_lens,
              .token_bytes = params.token_bytes,
              .tokenizer_json = params.tokenizer_json_file_content,
              .tokenize_fn = params.tokenize_fn,
              .tokenize_user_data = params.tokenize_user_data,
          };

          std::string error;
          error.resize(256);
          tokenizer_ =
              llg_new_tokenizer(&tokenizer_init, error.data(), error.size());
          if (!tokenizer_) {
            LOG(ERROR) << "Error creating tokenizer: " << error;
          }
        }));
  }

  if (!tokenizer_) {
    return 0;
  }

  LlgConstraintInit init;
  llg_constraint_init_set_defaults(&init, tokenizer_);
  LlgConstraint* constraint = nullptr;
  switch (response_constraint.which()) {
    case on_device_model::mojom::ResponseConstraint::Tag::kJsonSchema:
      constraint = llg_new_constraint_json(
          &init, response_constraint.get_json_schema().c_str());
      break;
    case on_device_model::mojom::ResponseConstraint::Tag::kRegex:
      constraint = llg_new_constraint_regex(
          &init, response_constraint.get_regex().c_str());
      break;
    case on_device_model::mojom::ResponseConstraint::Tag::kUnknownType:
      LOG(ERROR) << "Unknown constraint type.";
      return 0;
  }
  const char* error = llg_get_error(constraint);
  if (error) {
    LOG(ERROR) << "Error creating constraint: " << error;
    llg_free_constraint(constraint);
    return 0;
  }
  // Now apply any model prefix to the constraint so the generated model
  // response continues with the correct constraint state.
  if (prefix) {
    std::vector<uint32_t> tokens;
    // First get the total number of tokens needed.
    size_t token_size = llg_tokenize_bytes(
        tokenizer_, reinterpret_cast<const uint8_t*>(prefix->data()),
        prefix->size(), tokens.data(), 0);
    tokens.resize(token_size);
    // Then tokenize into `tokens`.
    llg_tokenize_bytes(tokenizer_,
                       reinterpret_cast<const uint8_t*>(prefix->data()),
                       prefix->size(), tokens.data(), tokens.size());
    // Apply each token to the constraint.
    for (uint32_t token : tokens) {
      LlgMaskResult mask_res;
      if (llg_compute_mask(constraint, &mask_res) < 0) {
        LOG(ERROR) << "Error computing mask for prompt prefix.";
        llg_free_constraint(constraint);
        return 0;
      }
      LlgCommitResult res;
      if (llg_commit_token(constraint, token, &res) < 0) {
        LOG(ERROR) << "Error matching prompt prefix.";
        llg_free_constraint(constraint);
        return 0;
      }
    }
  }
  return reinterpret_cast<ChromeMLConstraint>(constraint);
#else
  return 0;
#endif
}

DISABLE_CFI_DLSYM
LoadModelResult OnDeviceModelExecutor::Init(
    on_device_model::mojom::LoadModelParamsPtr params,
    base::OnceClosure on_complete) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::Init");
  ml::DeviceInfo device_info =
      ml::QueryDeviceInfo(chrome_ml_->api(), /*log_histogram=*/false);
  if (params->backend_type == ml::ModelBackendType::kGpuBackend &&
      device_info.gpu_blocked_reason != GpuBlockedReason::kNotBlocked) {
    return LoadModelResult::kGpuBlocked;
  }
  on_device_model::ModelAssets assets = std::move(params->assets);

  max_tokens_ = std::max(params->max_tokens, kReserveTokensForSafety);

  ChromeMLModelData data;
  std::string weights_path_str;
  std::string sp_model_path_str = assets.sp_model_path.AsUTF8Unsafe();
  // Prefer to load the model from a file descriptor if possible.
  if (assets.weights.IsFile()) {
    data.weights_file = assets.weights.file().TakePlatformFile();
  } else {
    weights_path_str = assets.weights.path().AsUTF8Unsafe();
    data.model_path = weights_path_str.data();
    data.sentencepiece_model_path = sp_model_path_str.data();
  }

  // Xnnpack doesn't delete the old cache file when it needs to be rebuilt
  // due to its build identifier changing, which will happen during browser
  // updates. This manually checks if the cache's build identifier matches the
  // current build, and if not, truncates it. This is a temporary fix until
  // xnnpack is updated with cb018b2d.
  // TODO(crbug.com/447174993): Remove once xnnpack includes cb018b2d.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (params->backend_type == ml::ModelBackendType::kCpuBackend) {
    MaybeDeleteCacheFile(assets.cache);
  }
  MaybeDeleteCacheFile(assets.encoder_cache);
  MaybeDeleteCacheFile(assets.adapter_cache);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

  // TODO(crbug.com/400998489): Cache files are experimental for now.
  data.cache_file = params->backend_type == ml::ModelBackendType::kCpuBackend &&
                            assets.cache.IsValid()
                        ? assets.cache.TakePlatformFile()
                        : base::kInvalidPlatformFile;
  if (assets.encoder_cache.IsValid()) {
    data.encoder_cache_file = assets.encoder_cache.TakePlatformFile();
  }
  if (assets.adapter_cache.IsValid()) {
    data.adapter_cache_file = assets.adapter_cache.TakePlatformFile();
  }
  ChromeMLModelDescriptor descriptor = {
      .backend_type = params->backend_type,
      .model_data = &data,
      .max_tokens = max_tokens_,
      .temperature = 0.0f,
      .top_k = optimization_guide::features::GetOnDeviceModelMaxTopK(),
      .adaptation_ranks = params->adaptation_ranks.data(),
      .adaptation_ranks_size = params->adaptation_ranks.size(),
      .prefer_texture_weights = kPreferTextureWeights.Get(),
      .enable_host_mapped_pointer = kEnableHostMappedPointer.Get(),
      .use_low_power = kUseLowPower.Get(),
      .allow_fp16 = kAllowFp16.Get(),
      .performance_hint = params->performance_hint,
  };
  model_ = chrome_ml_->api().SessionCreateModel(
      &descriptor, reinterpret_cast<uintptr_t>(this),
      OnDeviceModelExecutor::Schedule);
  model_task_runner_->PostTask(FROM_HERE, std::move(on_complete));
  return (model_ != 0) ? LoadModelResult::kSuccess
                       : LoadModelResult::kFailedToLoadLibrary;
}

// static
void OnDeviceModelExecutor::Schedule(uintptr_t context,
                                     std::function<void()>* fn) {
  TRACE_EVENT("optimization_guide", "OnDeviceModelExecutor::Schedule");
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce([](std::function<void()> fn) { fn(); }, std::move(*fn)));
}

}  // namespace ml
