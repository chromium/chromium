// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/session_accessor.h"

#include "base/compiler_specific.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_types.h"

namespace ml {

namespace {

namespace odmm = ::on_device_model::mojom;

float GetTemperature(std::optional<float> temperature) {
  return std::max(kMinTemperature, temperature.value_or(kMinTemperature));
}

uint32_t GetTopK(std::optional<uint32_t> top_k) {
  return std::min(static_cast<uint32_t>(
                      optimization_guide::features::GetOnDeviceModelMaxTopK()),
                  std::max(kMinTopK, top_k.value_or(kMinTopK)));
}

}  // namespace

// Wrapper for the ChromeMLCancel object.
class SessionAccessor::Canceler : public base::RefCountedThreadSafe<Canceler> {
 public:
  DISABLE_CFI_DLSYM
  explicit Canceler(const ChromeML& chrome_ml) : chrome_ml_(chrome_ml) {
    cancel_ = chrome_ml_->api().CreateCancel();
  }

  DISABLE_CFI_DLSYM
  void Cancel() { chrome_ml_->api().CancelExecuteModel(cancel_); }

  ChromeMLCancel get() const { return cancel_; }

 private:
  friend class base::RefCountedThreadSafe<Canceler>;

  DISABLE_CFI_DLSYM
  virtual ~Canceler() { chrome_ml_->api().DestroyCancel(cancel_); }

  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLCancel cancel_;
};

// static
SessionAccessor::Ptr SessionAccessor::Create(
    const ChromeML& chrome_ml,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ChromeMLModel model,
    on_device_model::mojom::SessionParamsPtr params,
    on_device_model::mojom::LoadAdaptationParamsPtr adaptation_params,
    std::optional<uint32_t> adaptation_id) {
  Ptr handle(new SessionAccessor(chrome_ml, task_runner, model),
             base::OnTaskRunnerDeleter(task_runner));
  // SessionAccessor is deleted on `task_runner_` so base::Unretained is safe.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::CreateInternal,
                     base::Unretained(handle.get()), std::move(params),
                     std::move(adaptation_params), adaptation_id));
  return handle;
}

DISABLE_CFI_DLSYM
SessionAccessor::~SessionAccessor() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().DestroySession(session_);
}

SessionAccessor::SessionAccessor(
    const ChromeML& chrome_ml,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ChromeMLModel model)
    : chrome_ml_(chrome_ml),
      task_runner_(std::move(task_runner)),
      model_(model) {}

SessionAccessor::Ptr SessionAccessor::Clone() {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Clone");
  Ptr handle(new SessionAccessor(chrome_ml_.get(), task_runner_, model_),
             base::OnTaskRunnerDeleter(task_runner_));
  // SessionAccessor is deleted on `task_runner_` so base::Unretained is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::CloneFrom,
                     base::Unretained(handle.get()), base::Unretained(this)));
  return handle;
}

ChromeMLCancelFn SessionAccessor::Append(
    on_device_model::mojom::AppendOptionsPtr options,
    ChromeMLContextSavedFn context_saved_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Append");
  DCHECK(context_saved_fn);
  auto canceler = base::MakeRefCounted<Canceler>(chrome_ml_.get());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::AppendInternal,
                                base::Unretained(this), std::move(options),
                                std::move(context_saved_fn), canceler));
  return [canceler] { canceler->Cancel(); };
}

ChromeMLCancelFn SessionAccessor::Generate(
    on_device_model::mojom::GenerateOptionsPtr options,
    ChromeMLConstraint constraint,
    ChromeMLExecutionOutputFn output_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Generate");
  DCHECK(output_fn);
  auto canceler = base::MakeRefCounted<Canceler>(chrome_ml_.get());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::GenerateInternal,
                                base::Unretained(this), std::move(options),
                                constraint, std::move(output_fn), canceler));
  return [canceler] { canceler->Cancel(); };
}

void SessionAccessor::Score(const std::string& text, ChromeMLScoreFn score_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Score");
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::ScoreInternal, base::Unretained(this),
                     text, std::move(score_fn)));
}

void SessionAccessor::GetProbabilitiesBlocking(
    const std::string& input,
    ChromeMLGetProbabilitiesBlockingFn get_prob_fn) {
  TRACE_EVENT("optimization_guide",
              "SessionAccessor::GetProbabilitiesBlocking");
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::GetProbabilitiesBlockingInternal,
                     base::Unretained(this), input, std::move(get_prob_fn)));
}

void SessionAccessor::SizeInTokens(on_device_model::mojom::InputPtr input,
                                   ChromeMLSizeInTokensFn size_in_tokens_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::SizeInTokens");
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::SizeInTokensInternal,
                                base::Unretained(this), std::move(input),
                                std::move(size_in_tokens_fn)));
}

void SessionAccessor::CreateAsrStream(
    odmm::AsrStreamOptionsPtr options,
    const ChromeMLASRStreamOutputFn output_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CreateAsrStream");
  DCHECK(output_fn);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::CreateAsrStreamInternal,
                                base::Unretained(this), std::move(options),
                                std::move(output_fn)));
}

void SessionAccessor::AsrAddAudioChunk(odmm::AudioDataPtr data) {
  TRACE_EVENT("optimization_guide.debug", "SessionAccessor::AsrAddAudioChunk");
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::AsrAddAudioChunkInternal,
                                base::Unretained(this), std::move(data)));
}

DISABLE_CFI_DLSYM
void SessionAccessor::CloneFrom(SessionAccessor* other) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CloneFrom");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  session_ = chrome_ml_->api().CloneSession(other->session_);
}

DISABLE_CFI_DLSYM
void SessionAccessor::CreateInternal(
    on_device_model::mojom::SessionParamsPtr params,
    on_device_model::mojom::LoadAdaptationParamsPtr adaptation_params,
    std::optional<uint32_t> adaptation_id) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CreateInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/403383823): Require `params` to be non-null and remove
  // this fallback path.
  if (!params) {
    params = on_device_model::mojom::SessionParams::New();
    params->top_k = GetTopK(std::nullopt);
    params->temperature = GetTemperature(std::nullopt);
  } else {
    // Clamp sampling params.
    params->top_k = GetTopK(params->top_k);
    params->temperature = GetTemperature(params->temperature);
  }
  ChromeMLAdaptationDescriptor descriptor = {
      .max_tokens = params->max_tokens,
      .top_k = params->top_k,
      .temperature = params->temperature,
      .enable_image_input = params->capabilities.Has(
          on_device_model::CapabilityFlags::kImageInput),
      .enable_audio_input = params->capabilities.Has(
          on_device_model::CapabilityFlags::kAudioInput),
  };
  ChromeMLModelData data;
  std::string weights_path_str;
  if (adaptation_params) {
    weights_path_str = adaptation_params->assets.weights_path.AsUTF8Unsafe();
    if (adaptation_params->assets.weights.IsValid() ||
        !weights_path_str.empty()) {
      if (adaptation_params->assets.weights.IsValid()) {
        data.weights_file =
            adaptation_params->assets.weights.TakePlatformFile();
      } else {
        data.model_path = weights_path_str.data();
      }
      data.file_id = adaptation_id;
      descriptor.model_data = &data;
    }
  }
  session_ = chrome_ml_->api().CreateSession(model_, &descriptor);
}

DISABLE_CFI_DLSYM
void SessionAccessor::AppendInternal(
    on_device_model::mojom::AppendOptionsPtr append_options,
    ChromeMLContextSavedFn context_saved_fn,
    scoped_refptr<Canceler> canceler) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::AppendInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLAppendOptions options{
      .input = append_options->input->pieces.data(),
      .input_size = append_options->input->pieces.size(),
      .max_tokens = append_options->max_tokens,
      .context_saved_fn = &context_saved_fn,
  };
  chrome_ml_->api().SessionAppend(session_, &options, canceler->get());
}

DISABLE_CFI_DLSYM
void SessionAccessor::GenerateInternal(
    on_device_model::mojom::GenerateOptionsPtr generate_options,
    ChromeMLConstraint constraint,
    ChromeMLExecutionOutputFn output_fn,
    scoped_refptr<Canceler> canceler) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::GenerateInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLGenerateOptions options{
      .max_output_tokens = generate_options->max_output_tokens,
      .constraint = constraint,
      .output_fn = &output_fn,
  };
  chrome_ml_->api().SessionGenerate(session_, &options, canceler->get());
}

DISABLE_CFI_DLSYM
void SessionAccessor::ScoreInternal(const std::string& text,
                                    ChromeMLScoreFn score_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::ScoreInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().SessionScore(session_, text, score_fn);
}

DISABLE_CFI_DLSYM
void SessionAccessor::GetProbabilitiesBlockingInternal(
    const std::string& input,
    ChromeMLGetProbabilitiesBlockingFn get_prob_fn) {
  TRACE_EVENT("optimization_guide",
              "SessionAccessor::GetProbabilitiesBlockingInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().SessionGetProbabilitiesBlocking(session_, input,
                                                    get_prob_fn);
}

DISABLE_CFI_DLSYM
void SessionAccessor::SizeInTokensInternal(
    on_device_model::mojom::InputPtr input,
    ChromeMLSizeInTokensFn size_in_tokens_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::SizeInTokensInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().SessionSizeInTokensInputPiece(
      session_, model_, input->pieces.data(), input->pieces.size(),
      size_in_tokens_fn);
}

DISABLE_CFI_DLSYM
void SessionAccessor::CreateAsrStreamInternal(
    odmm::AsrStreamOptionsPtr asr_options,
    const ChromeMLASRStreamOutputFn output_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CreateAsrStreamInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK_EQ(asr_stream_, 0u);  // Multiple streams on a session is not supported.
  ChromeMLASRStreamOptions options{
      .sample_rate_hz = asr_options->sample_rate_hz,
      .output_fn = &output_fn,
  };
  asr_stream_ = chrome_ml_->api().asr_api.CreateStream(session_, &options);
}

DISABLE_CFI_DLSYM
void SessionAccessor::AsrAddAudioChunkInternal(odmm::AudioDataPtr data) {
  TRACE_EVENT("optimization_guide.debug",
              "SessionAccessor::AsrAddAudioChunkInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK_NE(asr_stream_, 0u) << "ASR stream must be created first.";
  ml::AudioBuffer audio;
  audio.sample_rate_hz = data->sample_rate;
  audio.num_channels = data->channel_count;
  audio.num_frames = data->frame_count;
  audio.data = std::move(data->data);
  chrome_ml_->api().asr_api.AddAudioChunk(asr_stream_, &audio);
}

}  // namespace ml
