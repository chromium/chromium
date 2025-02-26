// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/session_accessor.h"

#include "base/compiler_specific.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/ml/chrome_ml.h"

namespace ml {

namespace {

// TODO(crbug.com/385173789): Pass enable_image_input via LoadAdaptationParams.
const base::FeatureParam<bool> kImageInput{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_image_input", false};

// TODO(crbug.com/385173368): Pass enable_audio_input via LoadAdaptationParams.
const base::FeatureParam<bool> kAudioInput{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_audio_input", false};

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
    on_device_model::mojom::LoadAdaptationParamsPtr params) {
  Ptr handle(new SessionAccessor(chrome_ml, task_runner, model),
             base::OnTaskRunnerDeleter(task_runner));
  // SessionAccessor is deleted on `task_runner_` so base::Unretained is safe.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::CreateInternal,
                     base::Unretained(handle.get()), std::move(params)));
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
  auto canceler = base::MakeRefCounted<Canceler>(chrome_ml_.get());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::AppendInternal,
                                base::Unretained(this), std::move(options),
                                std::move(context_saved_fn), canceler));
  return [canceler] { canceler->Cancel(); };
}

ChromeMLCancelFn SessionAccessor::Generate(
    on_device_model::mojom::GenerateOptionsPtr options,
    ChromeMLExecutionOutputFn output_fn) {
  auto canceler = base::MakeRefCounted<Canceler>(chrome_ml_.get());
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::GenerateInternal, base::Unretained(this),
                     std::move(options), std::move(output_fn), canceler));
  return [canceler] { canceler->Cancel(); };
}

void SessionAccessor::Score(const std::string& text, ChromeMLScoreFn score_fn) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::ScoreInternal, base::Unretained(this),
                     text, std::move(score_fn)));
}

void SessionAccessor::SizeInTokens(on_device_model::mojom::InputPtr input,
                                   ChromeMLSizeInTokensFn size_in_tokens_fn) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionAccessor::SizeInTokensInternal,
                                base::Unretained(this), std::move(input),
                                std::move(size_in_tokens_fn)));
}

DISABLE_CFI_DLSYM
void SessionAccessor::CloneFrom(SessionAccessor* other) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  session_ = chrome_ml_->api().CloneSession(other->session_);
}

DISABLE_CFI_DLSYM
void SessionAccessor::CreateInternal(
    on_device_model::mojom::LoadAdaptationParamsPtr params) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool enable_image_input = kImageInput.Get();
  const bool enable_audio_input = kAudioInput.Get();
  if (enable_image_input || enable_audio_input) {
    if (!params) {
      params = on_device_model::mojom::LoadAdaptationParams::New();
    }
    params->enable_image_input |= enable_image_input;
    params->enable_audio_input |= enable_audio_input;
  }
  if (params) {
    ChromeMLAdaptationDescriptor descriptor = {
        .max_tokens = params->max_tokens,
        .enable_image_input = params->enable_image_input,
        .enable_audio_input = params->enable_audio_input,
    };

    ChromeMLModelData data;
    std::string weights_path_str = params->assets.weights_path.AsUTF8Unsafe();
    if (params->assets.weights.IsValid() || !weights_path_str.empty()) {
      if (params->assets.weights.IsValid()) {
        data.weights_file = params->assets.weights.TakePlatformFile();
      } else {
        data.model_path = weights_path_str.data();
      }
      descriptor.model_data = &data;
    }
    session_ = chrome_ml_->api().CreateSession(model_, &descriptor);
  } else {
    session_ = chrome_ml_->api().CreateSession(model_, nullptr);
  }
}

DISABLE_CFI_DLSYM
void SessionAccessor::AppendInternal(
    on_device_model::mojom::AppendOptionsPtr append_options,
    ChromeMLContextSavedFn context_saved_fn,
    scoped_refptr<Canceler> canceler) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLExecuteOptions options{
      .max_tokens = append_options->max_tokens,
      .token_offset = append_options->token_offset,
  };
  options.input = append_options->input->pieces.data();
  options.input_size = append_options->input->pieces.size();
  if (context_saved_fn) {
    options.context_saved_fn = &context_saved_fn;
  }
  chrome_ml_->api().SessionExecuteModel(session_, model_, &options,
                                        canceler->get());
}

DISABLE_CFI_DLSYM
void SessionAccessor::GenerateInternal(
    on_device_model::mojom::GenerateOptionsPtr generate_options,
    ChromeMLExecutionOutputFn output_fn,
    scoped_refptr<Canceler> canceler) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLExecuteOptions options{
      .max_output_tokens = generate_options->max_output_tokens,
      .top_k = generate_options->top_k.value_or(1),
      .temperature = generate_options->temperature.value_or(0),
  };
  if (output_fn) {
    options.execution_output_fn = &output_fn;
  }
  chrome_ml_->api().SessionExecuteModel(session_, model_, &options,
                                        canceler->get());
}

DISABLE_CFI_DLSYM
void SessionAccessor::ScoreInternal(const std::string& text,
                                    ChromeMLScoreFn score_fn) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().SessionScore(session_, text, score_fn);
}

DISABLE_CFI_DLSYM
void SessionAccessor::SizeInTokensInternal(
    on_device_model::mojom::InputPtr input,
    ChromeMLSizeInTokensFn size_in_tokens_fn) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chrome_ml_->api().SessionSizeInTokensInputPiece(
      session_, model_, input->pieces.data(), input->pieces.size(),
      size_in_tokens_fn);
}

}  // namespace ml
