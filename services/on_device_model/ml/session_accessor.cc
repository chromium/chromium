// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/session_accessor.h"

#include "base/compiler_specific.h"
#include "services/on_device_model/ml/chrome_ml.h"

namespace ml {

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
    on_device_model::AdaptationAssets adaptation_assets) {
  Ptr handle(new SessionAccessor(chrome_ml, task_runner, model),
             base::OnTaskRunnerDeleter(task_runner));
  // SessionAccessor is deleted on `task_runner_` so base::Unretained is safe.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&SessionAccessor::CreateInternal,
                                       base::Unretained(handle.get()),
                                       std::move(adaptation_assets)));
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

ChromeMLCancelFn SessionAccessor::Execute(
    on_device_model::mojom::InputOptionsPtr input,
    ChromeMLExecutionOutputFn output_fn,
    ChromeMLContextSavedFn context_saved_fn) {
  auto canceler = base::MakeRefCounted<Canceler>(chrome_ml_.get());
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionAccessor::ExecuteInternal, base::Unretained(this),
                     std::move(input), std::move(output_fn),
                     std::move(context_saved_fn), canceler));
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
    on_device_model::AdaptationAssets adaptation_assets) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (adaptation_assets.weights.IsValid() ||
      !adaptation_assets.weights_path.empty()) {
    ChromeMLModelData data;
    std::string weights_path_str =
        adaptation_assets.weights_path.AsUTF8Unsafe();
    if (adaptation_assets.weights.IsValid()) {
      data.weights_file = adaptation_assets.weights.TakePlatformFile();
    } else {
      data.model_path = weights_path_str.data();
    }
    ChromeMLAdaptationDescriptor descriptor = {
        .model_data = &data,
    };
    session_ = chrome_ml_->api().CreateSession(model_, &descriptor);
  } else {
    session_ = chrome_ml_->api().CreateSession(model_, nullptr);
  }
}

DISABLE_CFI_DLSYM
void SessionAccessor::ExecuteInternal(
    on_device_model::mojom::InputOptionsPtr input,
    ChromeMLExecutionOutputFn output_fn,
    ChromeMLContextSavedFn context_saved_fn,
    scoped_refptr<Canceler> canceler) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeMLExecuteOptions options{
      .prompt = input->text.c_str(),
      .max_tokens = input->max_tokens.value_or(0),
      .token_offset = input->token_offset.value_or(0),
      .max_output_tokens = input->max_output_tokens.value_or(0),
      .top_k = input->top_k.value_or(1),
      .temperature = input->temperature.value_or(0),
  };
  if (input->input) {
    options.input = input->input->pieces.data();
    options.input_size = input->input->pieces.size();
  }
  if (context_saved_fn) {
    options.context_saved_fn = &context_saved_fn;
  }
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
