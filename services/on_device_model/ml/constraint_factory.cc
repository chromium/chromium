// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/constraint_factory.h"

#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/rust/chromium_crates_io/vendor/llguidance-v1/llguidance.h"

namespace ml {

namespace {

class TokenizerParamsCopy {
 public:
  static std::unique_ptr<TokenizerParamsCopy> Copy(
      const ChromeMLTokenizerParams& params) {
    TRACE_EVENT("optimization_guide", "TokenizerParamsCopy::Copy");
    auto params_copy = std::make_unique<TokenizerParamsCopy>();
    params_copy->vocab_size = params.vocab_size;
    params_copy->eos_token_id = params.eos_token_id;

    if (params.token_lens) {
      // SAFETY: The ChromeML API defines `token_lens` to have `vocab_size`
      // elements.
      auto lens_span =
          UNSAFE_BUFFERS(base::span(params.token_lens, params.vocab_size));
      params_copy->token_lens.assign(lens_span.begin(), lens_span.end());
    }

    if (params.token_bytes) {
      size_t bytes_len = 0;
      for (uint32_t len : params_copy->token_lens) {
        bytes_len += len;
      }
      // SAFETY: The ChromeML API defines `token_bytes` length as the sum of
      // `token_lens` elements.
      auto bytes_span =
          UNSAFE_BUFFERS(base::span(params.token_bytes, bytes_len));
      params_copy->token_bytes.assign(bytes_span.begin(), bytes_span.end());
    }
    if (params.tokenizer_json_file_content) {
      params_copy->tokenizer_json_file_content =
          params.tokenizer_json_file_content;
    }

    params_copy->tokenize_fn = params.tokenize_fn;
    params_copy->tokenize_user_data = params.tokenize_user_data;
    return params_copy;
  }

  LlgTokenizer* CreateTokenizer() const {
    TRACE_EVENT("optimization_guide", "llg_new_tokenizer");
    LlgTokenizerInit tokenizer_init{
        .vocab_size = vocab_size,
        .tok_eos = eos_token_id,
        .token_lens = token_lens.empty() ? nullptr : token_lens.data(),
        .token_bytes = token_bytes.empty() ? nullptr : token_bytes.data(),
        .tokenizer_json = tokenizer_json_file_content.has_value()
                              ? tokenizer_json_file_content->c_str()
                              : nullptr,
        .tokenize_fn = tokenize_fn,
        .tokenize_user_data = tokenize_user_data,
    };

    std::string error;
    error.resize(256);
    LlgTokenizer* tokenizer =
        llg_new_tokenizer(&tokenizer_init, error.data(), error.size());
    if (!tokenizer) {
      LOG(ERROR) << "Error creating tokenizer: " << error;
    }
    return tokenizer;
  }

 private:
  uint32_t vocab_size;
  uint32_t eos_token_id;
  std::vector<uint32_t> token_lens;
  std::vector<uint8_t> token_bytes;
  std::optional<std::string> tokenizer_json_file_content;
  ChromeMLTokenizeFn tokenize_fn;
  raw_ptr<const void> tokenize_user_data;
};

#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
ChromeMLConstraint MakeConstraint(
    LlgTokenizer* tokenizer,
    const on_device_model::mojom::ResponseConstraint& response_constraint,
    const std::optional<std::string>& prefix) {
  LlgConstraintInit init;
  llg_constraint_init_set_defaults(&init, tokenizer);
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
        tokenizer, reinterpret_cast<const uint8_t*>(prefix->data()),
        prefix->size(), tokens.data(), 0);
    tokens.resize(token_size);
    // Then tokenize into `tokens`.
    llg_tokenize_bytes(tokenizer,
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
}
#endif

}  // namespace

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

void ConstraintFactory::InitializeTokenizer(ChromeMLModel model,
                                            ChromeMLSession session) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (is_initialization_started_) {
    return;
  }
  is_initialization_started_ = true;
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  TRACE_EVENT("optimization_guide", "ConstraintFactory::InitializeTokenizer");

  std::unique_ptr<TokenizerParamsCopy> params_copy;

  TRACE_EVENT_BEGIN("optimization_guide", "GetTokenizerParams");
  GetTokenizerParams(model, session,
                     [&](const ChromeMLTokenizerParams& params) {
                       TRACE_EVENT_END("optimization_guide");
                       params_copy = TokenizerParamsCopy::Copy(params);
                     });

  if (!params_copy) {
    tokenizer_initialized_.Signal();
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          [](ConstraintFactory* factory,
             std::unique_ptr<TokenizerParamsCopy> params) {
            factory->tokenizer_ = params->CreateTokenizer();
            factory->tokenizer_initialized_.Signal();
          },
          // Unretained is safe because `ConstraintFactory`'s destructor will
          // wait for this task to complete if it has been launched. The
          // destructor calls `tokenizer_initialized_.Wait()` which is only
          // Signaled at the end of this task.
          base::Unretained(this), std::move(params_copy)));
#else
  tokenizer_initialized_.Signal();
#endif
}

LlgTokenizer* ConstraintFactory::GetTokenizer() {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  CHECK(is_initialization_started_);
  tokenizer_initialized_.Wait();
  return tokenizer_;
#else
  return nullptr;
#endif
}

ConstraintFactory::~ConstraintFactory() {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  if (is_initialization_started_) {
    if (LlgTokenizer* tokenizer = GetTokenizer()) {
      llg_free_tokenizer(tokenizer);
    }
  }
#endif
}

bool ConstraintFactory::GetTokenizerParams(
    ChromeMLModel model,
    ChromeMLSession session,
    const ChromeMLGetTokenizerParamsFn& fn) {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  return chrome_ml_->GetTokenizerParams(model, session, fn);
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
  InitializeTokenizer(model, session);

  LlgTokenizer* tokenizer = GetTokenizer();
  if (!tokenizer) {
    return 0;
  }

  return MakeConstraint(tokenizer, response_constraint, prefix);
#else
  return 0;
#endif
}

}  // namespace ml
