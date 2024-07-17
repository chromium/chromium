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
  Canceler() { cancel_ = ChromeML::Get()->api().CreateCancel(); }

  DISABLE_CFI_DLSYM
  void Cancel() { ChromeML::Get()->api().CancelExecuteModel(cancel_); }

  ChromeMLCancel get() const { return cancel_; }

 private:
  friend class base::RefCountedThreadSafe<Canceler>;

  DISABLE_CFI_DLSYM
  virtual ~Canceler() { ChromeML::Get()->api().DestroyCancel(cancel_); }

  ChromeMLCancel cancel_;
};

// static
SessionAccessor::Ptr SessionAccessor::Empty() {
  return SessionAccessor::Ptr(nullptr, base::OnTaskRunnerDeleter(nullptr));
}

// static
SessionAccessor::Ptr SessionAccessor::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ChromeMLModel model,
    base::File adaptation_data) {
  Ptr handle(new SessionAccessor(task_runner, model),
             base::OnTaskRunnerDeleter(task_runner));
  // SessionAccessor is deleted on `task_runner_` so base::Unretained is safe.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&SessionAccessor::CreateInternal,
                                       base::Unretained(handle.get()),
                                       std::move(adaptation_data)));
  return handle;
}

DISABLE_CFI_DLSYM
SessionAccessor::~SessionAccessor() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeML::Get()->api().DestroySession(session_);
}

SessionAccessor::SessionAccessor(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ChromeMLModel model)
    : task_runner_(std::move(task_runner)), model_(model) {}

SessionAccessor::Ptr SessionAccessor::Clone() {
  Ptr handle(new SessionAccessor(task_runner_, model_),
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
  auto canceler = base::MakeRefCounted<Canceler>();
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

void SessionAccessor::SizeInTokens(const std::string& text,
                                   ChromeMLSizeInTokensFn size_in_tokens_fn) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SessionAccessor::SizeInTokensInternal,
                                        base::Unretained(this), text,
                                        std::move(size_in_tokens_fn)));
}

DISABLE_CFI_DLSYM
void SessionAccessor::CloneFrom(SessionAccessor* other) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  session_ = ChromeML::Get()->api().CloneSession(other->session_);
}

DISABLE_CFI_DLSYM
void SessionAccessor::CreateInternal(base::File adaptation_data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (adaptation_data.IsValid()) {
    ChromeMLModelData data = {
        .weights_file = adaptation_data.TakePlatformFile(),
    };
    ChromeMLAdaptationDescriptor descriptor = {
        .model_data = &data,
    };
    session_ = ChromeML::Get()->api().CreateSession(model_, &descriptor);
  } else {
    session_ = ChromeML::Get()->api().CreateSession(model_, nullptr);
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
      .score_ts_interval = -1,
      .context_saved_fn = &context_saved_fn,
      .top_k = input->top_k.value_or(1),
      .temperature = input->temperature.value_or(0),
  };
  if (context_saved_fn) {
    options.context_saved_fn = &context_saved_fn;
  }
  if (output_fn) {
    options.execution_output_fn = &output_fn;
  }
  ChromeML::Get()->api().SessionExecuteModel(session_, model_, &options,
                                             canceler->get());
}

DISABLE_CFI_DLSYM
void SessionAccessor::ScoreInternal(const std::string& text,
                                    ChromeMLScoreFn score_fn) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeML::Get()->api().SessionScore(session_, text, score_fn);
}

DISABLE_CFI_DLSYM
void SessionAccessor::SizeInTokensInternal(
    const std::string& text,
    ChromeMLSizeInTokensFn size_in_tokens_fn) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ChromeML::Get()->api().SessionSizeInTokens(session_, text, size_in_tokens_fn);
}

}  // namespace ml
