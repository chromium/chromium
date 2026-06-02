// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/session_accessor.h"

#include <optional>
#include <thread>
#include <vector>

#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/ml/constraint_factory.h"

namespace ml {

namespace {

// Whether to respect the constrained decoding hint for early tokenizer
// initialization.
BASE_FEATURE(kOnDeviceModelConstrainedDecodingHint,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace odmm = ::on_device_model::mojom;

float GetTemperature(std::optional<float> temperature) {
  return std::max(kMinTemperature, temperature.value_or(kMinTemperature));
}

uint32_t GetTopK(std::optional<uint32_t> top_k) {
  return std::min(static_cast<uint32_t>(
                      optimization_guide::features::GetOnDeviceModelMaxTopK()),
                  std::max(kMinTopK, top_k.value_or(kMinTopK)));
}

std::optional<ml::InputPiece> ConvertMojomInputPieceToMlInputPiece(
    odmm::InputPiecePtr piece) {
  switch (piece->which()) {
    case odmm::InputPiece::Tag::kToken:
      return piece->get_token();
    case odmm::InputPiece::Tag::kText:
      return std::move(piece->get_text());
    case odmm::InputPiece::Tag::kBitmap:
      return std::move(piece->get_bitmap());
    case odmm::InputPiece::Tag::kAudio: {
      auto& audio = piece->get_audio();
      ml::AudioBuffer audio_buffer;
      audio_buffer.sample_rate_hz = audio->sample_rate;
      audio_buffer.num_channels = audio->channel_count;
      audio_buffer.num_frames = audio->frame_count;
      audio_buffer.data = std::move(audio->data);
      return audio_buffer;
    }
    case odmm::InputPiece::Tag::kToolDeclaration: {
      auto& input = piece->get_tool_declaration();
      ml::ToolDeclaration output;
      output.name = std::move(input->name);
      output.description = std::move(input->description);
      if (!base::JSONWriter::Write(input->input_schema,
                                   &output.input_schema_json)) {
        LOG(WARNING) << "Failed to serialize tool declaration input_schema.";
        return std::nullopt;
      }
      return output;
    }
    case odmm::InputPiece::Tag::kToolResponse: {
      auto& input = piece->get_tool_response();
      bool has_error =
          input->error_message.has_value() && !input->error_message->empty();
      bool has_result = input->result.has_value() && !input->result->is_none();
      if ((has_error && has_result) || (!has_error && !has_result)) {
        LOG(WARNING) << "Tool response must have exactly one of result or "
                        "error_message.";
        return std::nullopt;
      }

      ml::ToolResponse output;
      output.call_id = std::move(input->call_id);
      output.name = std::move(input->name);
      if (has_error) {
        output.error_message = std::move(*input->error_message);
      } else if (!base::JSONWriter::Write(*input->result,
                                          &output.result_json)) {
        LOG(WARNING) << "Failed to serialize tool response result.";
        return std::nullopt;
      }
      return output;
    }
    case odmm::InputPiece::Tag::kUnknownType:
      return piece->get_unknown_type();
  }
  NOTREACHED();
}

std::optional<std::vector<ml::InputPiece>> ConvertMojomInputToMlInputPieces(
    odmm::InputPtr input) {
  std::vector<ml::InputPiece> pieces;
  pieces.reserve(input->pieces.size());
  for (auto& piece : input->pieces) {
    std::optional<ml::InputPiece> converted =
        ConvertMojomInputPieceToMlInputPiece(std::move(piece));
    if (!converted) {
      return std::nullopt;
    }
    pieces.push_back(std::move(*converted));
  }
  return pieces;
}

}  // namespace

// Wrapper for the ChromeMLCancel object.
class SessionAccessor::Canceler : public base::RefCountedThreadSafe<Canceler> {
 public:
  Canceler(const ChromeML& chrome_ml,
           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : chrome_ml_(chrome_ml), task_runner_(task_runner) {}

  void Cancel() {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionAccessor::Canceler::CancelInternal,
                                  base::RetainedRef(this)));
  }

  ChromeMLCancel get() {
    if (cancel_ == 0) {
      CreateInternal();
    }
    return cancel_;
  }

 private:
  friend class base::RefCountedThreadSafe<Canceler>;

  static void DestroyChromeMLCancel(const raw_ref<const ChromeML> chrome_ml,
                                    ChromeMLCancel cancel = 0) {
    if (cancel == 0) {
      return;
    }

    chrome_ml->DestroyCancel(cancel);
  }

  virtual ~Canceler() {
    // Ensure that `ChromeMLCancel` is destroyed on the `task_runner_`.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DestroyChromeMLCancel,
                                  // Safe because `chrome_ml_` will never be
                                  // destroyed, and `cancel_` should only be
                                  // destroyed in this callback.
                                  chrome_ml_, cancel_));
  }

  void CreateInternal() { cancel_ = chrome_ml_->CreateCancel(); }

  void CancelInternal() { chrome_ml_->CancelExecuteModel(get()); }

  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLCancel cancel_ = 0;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
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

SessionAccessor::~SessionAccessor() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->DestroySession(session_);
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
    const perfetto::Track& perfetto_id,
    on_device_model::mojom::AppendOptionsPtr options,
    ChromeMLContextSavedFn context_saved_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Append");
  DCHECK(context_saved_fn);
  auto canceler =
      base::MakeRefCounted<Canceler>(chrome_ml_.get(), task_runner_);

  TRACE_EVENT_BEGIN("optimization_guide", "Queued", perfetto_id);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::AppendInternal, base::Unretained(this),
                     perfetto_id, std::move(options),
                     std::move(context_saved_fn), canceler));
  return [canceler] { canceler->Cancel(); };
}

ChromeMLCancelFn SessionAccessor::Generate(
    const perfetto::Track& perfetto_id,
    on_device_model::mojom::GenerateOptionsPtr options,
    ConstraintFactory* constraint_factory,
    const std::optional<std::string>& model_response_prefix,
    ChromeMLExecutionOutputFn output_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Generate");
  DCHECK(output_fn);
  auto canceler =
      base::MakeRefCounted<Canceler>(chrome_ml_.get(), task_runner_);

  TRACE_EVENT_BEGIN("optimization_guide", "Queued", perfetto_id);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::GenerateInternal, base::Unretained(this),
                     perfetto_id, std::move(options),
                     // Unretained safe since `constrained_factory` is deleted
                     // on the sequence.
                     base::Unretained(constraint_factory),
                     model_response_prefix, std::move(output_fn), canceler));
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

void SessionAccessor::Hint(on_device_model::mojom::HintOptionsPtr options,
                           ConstraintFactory* constraint_factory) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::Hint");
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::HintInternal, base::Unretained(this),
                     std::move(options), base::Unretained(constraint_factory)));
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
    const ChromeMLASRStreamOutputFn output_fn,
    base::OnceCallback<void(std::optional<odmm::AsrError>)> done_callback) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CreateAsrStream");
  DCHECK(output_fn);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::CreateAsrStreamInternal,
                     base::Unretained(this), std::move(options),
                     std::move(output_fn)),
      std::move(done_callback));
}

void SessionAccessor::AsrAddAudioChunk(odmm::AudioDataPtr data) {
  TRACE_EVENT("optimization_guide.debug", "SessionAccessor::AsrAddAudioChunk");
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::AsrAddAudioChunkInternal,
                                base::Unretained(this), std::move(data)));
}

void SessionAccessor::CloneFrom(SessionAccessor* other) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CloneFrom");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  session_ = chrome_ml_->CloneSession(other->session_);
}

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
  session_ = chrome_ml_->CreateSession(model_, &descriptor);
}

void SessionAccessor::AppendInternal(
    perfetto::Track perfetto_id,
    on_device_model::mojom::AppendOptionsPtr append_options,
    ChromeMLContextSavedFn context_saved_fn,
    scoped_refptr<Canceler> canceler) {
  // Ends the `Queued` trace.
  TRACE_EVENT_END("optimization_guide", perfetto_id);
  TRACE_EVENT("optimization_guide", "SessionAccessor::AppendInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  InputSource source;
  switch (append_options->input_source) {
    case on_device_model::mojom::InputSource::kUserInput:
      source = InputSource::kUserInput;
      break;
    case on_device_model::mojom::InputSource::kModelOutputFeedback:
      source = InputSource::kModelOutputFeedback;
      break;
    case on_device_model::mojom::InputSource::kUnknown:
      source = InputSource::kUnknown;
      LOG(WARNING) << "AppendOptions called with kUnknown InputSource";
      break;
  }

  std::optional<std::vector<ml::InputPiece>> input =
      ConvertMojomInputToMlInputPieces(std::move(append_options->input));
  if (!input) {
    // TODO(crbug.com/422803232): Report invalid input with
    // mojo::ReportBadMessage().
    // Complete local request bookkeeping with 0 tokens so the caller's
    // ContextHolder can clean up.
    LOG(WARNING) << "AppendInternal: failed to convert input pieces; "
                    "completing with 0 tokens appended.";
    context_saved_fn(0);
    return;
  }
  ChromeMLAppendOptions options{
      .input = input->data(),
      .input_size = input->size(),
      .max_tokens = append_options->max_tokens,
      .context_saved_fn = &context_saved_fn,
      .input_source = source,
  };
  chrome_ml_->SessionAppend(session_, &options, canceler->get());
}

void SessionAccessor::GenerateInternal(
    perfetto::Track perfetto_id,
    on_device_model::mojom::GenerateOptionsPtr generate_options,
    ConstraintFactory* constraint_factory,
    std::optional<std::string> model_response_prefix,
    ChromeMLExecutionOutputFn output_fn,
    scoped_refptr<Canceler> canceler) {
  // Ends the `Queued` trace.
  TRACE_EVENT_END("optimization_guide", perfetto_id);
  TRACE_EVENT("optimization_guide", "SessionAccessor::GenerateInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLConstraint constraint = 0;
  if (generate_options->constraint) {
    constraint = constraint_factory->CreateConstraint(
        session_, model_, *generate_options->constraint, model_response_prefix);
    if (!constraint) {
      ChromeMLGenerateOutput output{ChromeMLGenerateStatus::kInvalidConstraint,
                                    nullptr};
      output_fn(&output);
      return;
    }
  }
  ChromeMLGenerateOptions options{
      .max_output_tokens = generate_options->max_output_tokens,
      .constraint = constraint,
      .output_fn = &output_fn,
  };
  chrome_ml_->SessionGenerate(session_, &options, canceler->get());
}

void SessionAccessor::ScoreInternal(const std::string& text,
                                    ChromeMLScoreFn score_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::ScoreInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->SessionScore(session_, text, score_fn);
}

void SessionAccessor::HintInternal(
    on_device_model::mojom::HintOptionsPtr options,
    ConstraintFactory* constraint_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (options->constrained_decoding_hint &&
      base::FeatureList::IsEnabled(kOnDeviceModelConstrainedDecodingHint)) {
    constraint_factory->InitializeTokenizer(model_, session_);
  }
}

void SessionAccessor::GetProbabilitiesBlockingInternal(
    const std::string& input,
    ChromeMLGetProbabilitiesBlockingFn get_prob_fn) {
  TRACE_EVENT("optimization_guide",
              "SessionAccessor::GetProbabilitiesBlockingInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->SessionGetProbabilitiesBlocking(session_, input, get_prob_fn);
}

void SessionAccessor::SizeInTokensInternal(
    on_device_model::mojom::InputPtr input,
    ChromeMLSizeInTokensFn size_in_tokens_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::SizeInTokensInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::optional<std::vector<ml::InputPiece>> converted_input =
      ConvertMojomInputToMlInputPieces(std::move(input));
  if (!converted_input) {
    // TODO(crbug.com/422803232): Report invalid input with
    // mojo::ReportBadMessage().
    // Complete local request bookkeeping with size 0 so the caller's reply
    // callback fires.
    LOG(WARNING) << "SizeInTokensInternal: failed to convert input pieces; "
                    "reporting size 0.";
    size_in_tokens_fn(0);
    return;
  }
  chrome_ml_->SessionSizeInTokensInputPiece(
      session_, model_, converted_input->data(), converted_input->size(),
      size_in_tokens_fn);
}

std::optional<odmm::AsrError> SessionAccessor::CreateAsrStreamInternal(
    odmm::AsrStreamOptionsPtr asr_options,
    const ChromeMLASRStreamOutputFn output_fn) {
  TRACE_EVENT("optimization_guide", "SessionAccessor::CreateAsrStreamInternal");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK_EQ(asr_stream_, 0u);  // Multiple streams on a session is not supported.
  ChromeMLASRStreamOptions options{
      .sample_rate_hz = asr_options->sample_rate_hz,
      .output_fn = &output_fn,
  };
  asr_stream_ = chrome_ml_->ASRCreateStream(session_, &options);
  if (asr_stream_ == 0) {
    return odmm::AsrError::kInitializationFailed;
  }
  return std::nullopt;
}

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
  chrome_ml_->ASRAddAudioChunk(asr_stream_, &audio);
}

}  // namespace ml
