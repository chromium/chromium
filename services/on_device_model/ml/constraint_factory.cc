// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/constraint_factory.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/rust/chromium_crates_io/vendor/llguidance-v1/llguidance.h"

namespace ml {

// static
ConstraintFactory::Ptr ConstraintFactory::Create(
    const ChromeML& chrome_ml,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return Ptr(new ConstraintFactory(chrome_ml, task_runner),
             base::OnTaskRunnerDeleter(task_runner));
}

ConstraintFactory::ConstraintFactory(
    const ChromeML& chrome_ml,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : chrome_ml_(chrome_ml), task_runner_(std::move(task_runner)) {}

ConstraintFactory::~ConstraintFactory() {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (tokenizer_ != nullptr) {
    llg_free_tokenizer(tokenizer_);
  }
#endif
}

DISABLE_CFI_DLSYM
bool ConstraintFactory::GetTokenizerParams(
    ChromeMLModel model,
    ChromeMLSession session,
    const ChromeMLGetTokenizerParamsFn& fn) {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (chrome_ml_->api().GetTokenizerParamsV2) {
    return chrome_ml_->api().GetTokenizerParamsV2(model, session, fn);
  } else {
    return chrome_ml_->api().GetTokenizerParams(model, fn);
  }
#else
  return false;
#endif
}

ChromeMLConstraint ConstraintFactory::CreateConstraint(
    ChromeMLSession session,
    ChromeMLModel model,
    const on_device_model::mojom::ResponseConstraint& response_constraint,
    const std::optional<std::string>& prefix) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT("optimization_guide", "ConstraintFactory::CreateConstraint");
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (!tokenizer_) {
    CHECK(GetTokenizerParams(
        model, session, [&](const ChromeMLTokenizerParams& params) {
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

}  // namespace ml
