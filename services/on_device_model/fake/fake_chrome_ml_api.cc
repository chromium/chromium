// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/fake/fake_chrome_ml_api.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace fake_ml {
namespace {
std::string PieceToString(const ml::InputPiece& piece) {
  if (std::holds_alternative<std::string>(piece)) {
    return std::get<std::string>(piece);
  }
  switch (std::get<ml::Token>(piece)) {
    case ml::Token::kSystem:
      return "System: ";
    case ml::Token::kModel:
      return "Model: ";
    case ml::Token::kUser:
      return "User: ";
    case ml::Token::kEnd:
      return " End.";
  }
}

int g_active_non_clone_sessions = 0;

}  // namespace

int GetActiveNonCloneSessions() {
  return g_active_non_clone_sessions;
}

void InitDawnProcs(const DawnProcTable& procs) {}

void SetMetricsFns(const ChromeMLMetricsFns* fns) {}

void SetFatalErrorFn(ChromeMLFatalErrorFn error_fn) {}

void SetFatalErrorNonGpuFn(ChromeMLFatalErrorFn error_fn) {}

bool GetEstimatedPerformance(ChromeMLPerformanceInfo* performance_info) {
  return false;
}

bool QueryGPUAdapter(void (*adapter_callback_fn)(WGPUAdapter adapter,
                                                 void* userdata),
                     void* userdata) {
  return false;
}

struct FakeModelInstance {
  ModelBackendType backend_type_;
  std::string model_data_;
};

struct FakeSessionInstance {
  std::string adaptation_data_;
  std::vector<std::string> context_;
  bool cloned;
};

struct FakeTsModelInstance {
  std::string model_data_;
};

struct FakeCancelInstance {
  bool cancelled = false;
};

std::string ReadFile(PlatformFile api_file) {
  base::File file(static_cast<base::PlatformFile>(api_file));
  std::vector<uint8_t> contents;
  contents.resize(file.GetLength());
  if (!file.ReadAndCheck(0, contents)) {
    return std::string();
  }
  return std::string(contents.begin(), contents.end());
}

ChromeMLModel SessionCreateModel(const ChromeMLModelDescriptor* descriptor,
                                 uintptr_t context,
                                 ChromeMLScheduleFn schedule) {
  return reinterpret_cast<ChromeMLModel>(
      new FakeModelInstance{.backend_type_ = descriptor->backend_type});
}

void DestroyModel(ChromeMLModel model) {
  auto* instance = reinterpret_cast<FakeModelInstance*>(model);
  delete instance;
}

ChromeMLSafetyResult ClassifyTextSafety(ChromeMLModel model,
                                        const char* text,
                                        float* scores,
                                        size_t* num_scores) {
  return ChromeMLSafetyResult::kNoClassifier;
}

ChromeMLSession CreateSession(ChromeMLModel model,
                              const ChromeMLAdaptationDescriptor* descriptor) {
  g_active_non_clone_sessions++;
  auto* model_instance = reinterpret_cast<FakeModelInstance*>(model);
  auto* instance = new FakeSessionInstance{};
  if (descriptor) {
    if (model_instance->backend_type_ == ModelBackendType::kGpuBackend) {
      instance->adaptation_data_ =
          ReadFile(descriptor->model_data->weights_file);
    } else if (model_instance->backend_type_ == ModelBackendType::kApuBackend) {
      base::ReadFileToString(
          base::FilePath::FromUTF8Unsafe(descriptor->model_data->model_path),
          &instance->adaptation_data_);
    }
  }
  return reinterpret_cast<ChromeMLSession>(instance);
}

ChromeMLSession CloneSession(ChromeMLSession session) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  return reinterpret_cast<ChromeMLSession>(new FakeSessionInstance{
      .adaptation_data_ = instance->adaptation_data_,
      .context_ = instance->context_,
      .cloned = true,
  });
}

void DestroySession(ChromeMLSession session) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  if (!instance->cloned) {
    g_active_non_clone_sessions--;
  }
  delete instance;
}

bool SessionExecuteModel(ChromeMLSession session,
                         ChromeMLModel model,
                         const ChromeMLExecuteOptions* options,
                         ChromeMLCancel cancel) {
  auto* instance = reinterpret_cast<FakeSessionInstance*>(session);
  std::string text = options->prompt;
  if (options->token_offset) {
    text.erase(text.begin(), text.begin() + options->token_offset);
  }
  if (options->max_tokens && options->max_tokens < text.size()) {
    text.resize(options->max_tokens);
  }
  for (size_t i = 0; i < options->input_size; i++) {
    // SAFETY: `options->input_size` describes how big `options->input` is.
    text += UNSAFE_BUFFERS(PieceToString(options->input[i]));
  }
  if (!text.empty()) {
    instance->context_.push_back(text);
  }
  if (options->context_saved_fn) {
    (*options->context_saved_fn)(static_cast<int>(text.size()));
  }

  if (!options->execution_output_fn) {
    return true;
  }

  auto OutputChunk =
      [output_fn = *options->execution_output_fn](const std::string& chunk) {
        ChromeMLExecutionOutput output = {};
        if (chunk.empty()) {
          output.status = ChromeMLExecutionStatus::kComplete;
          output_fn(&output);
          return;
        }
        output.status = ChromeMLExecutionStatus::kInProgress;
        output.text = chunk.c_str();
        output_fn(&output);
      };

  if (!instance->adaptation_data_.empty()) {
    OutputChunk("Adaptation: " + instance->adaptation_data_ + "\n");
  }
  if (!instance->context_.empty()) {
    const std::string last = instance->context_.back();
    instance->context_.pop_back();
    for (const std::string& context : instance->context_) {
      OutputChunk("Context: " + context + "\n");
    }
    OutputChunk("Input: " + last + "\n");
  }
  OutputChunk("");
  return true;
}

void SessionSizeInTokensInputPiece(ChromeMLSession session,
                                   ChromeMLModel model,
                                   const ml::InputPiece* input,
                                   size_t input_size,
                                   const ChromeMLSizeInTokensFn& fn) {
  std::string text;
  for (size_t i = 0; i < input_size; i++) {
    // SAFETY: `input_size` describes how big `input` is.
    text += UNSAFE_BUFFERS(PieceToString(input[i]));
  }
  fn(text.size());
}

void SessionScore(ChromeMLSession session,
                  const std::string& text,
                  const ChromeMLScoreFn& fn) {
  fn(static_cast<float>(text[0]));
}

ChromeMLCancel CreateCancel() {
  return reinterpret_cast<ChromeMLCancel>(new FakeCancelInstance());
}

void DestroyCancel(ChromeMLCancel cancel) {
  delete reinterpret_cast<FakeCancelInstance*>(cancel);
}

void CancelExecuteModel(ChromeMLCancel cancel) {
  auto* instance = reinterpret_cast<FakeCancelInstance*>(cancel);
  instance->cancelled = true;
}

ChromeMLTSModel CreateTSModel(const ChromeMLTSModelDescriptor* descriptor) {
  auto* instance = new FakeTsModelInstance{};
  return reinterpret_cast<ChromeMLTSModel>(instance);
}

void DestroyTSModel(ChromeMLTSModel model) {
  auto* instance = reinterpret_cast<FakeTsModelInstance*>(model);
  delete instance;
}

ChromeMLSafetyResult TSModelClassifyTextSafety(ChromeMLTSModel model,
                                               const char* text,
                                               float* scores,
                                               size_t* num_scores) {
  if (*num_scores != 2) {
    *num_scores = 2;
    return ChromeMLSafetyResult::kInsufficientStorage;
  }
  bool has_unsafe = std::string(text).find("unsafe") != std::string::npos;
  // SAFETY: Follows a C-API, num_scores checked above, test-only code.
  UNSAFE_BUFFERS(scores[0]) = has_unsafe ? 0.8 : 0.2;
  bool has_reasonable =
      std::string(text).find("reasonable") != std::string::npos;
  // SAFETY: Follows a C-API, num_scores checked above, test-only code.
  UNSAFE_BUFFERS(scores[1]) = has_reasonable ? 0.2 : 0.8;
  return ChromeMLSafetyResult::kOk;
}

const ChromeMLAPI g_api = {
    .InitDawnProcs = &InitDawnProcs,
    .SetMetricsFns = &SetMetricsFns,
    .SetFatalErrorFn = &SetFatalErrorFn,
    .ClassifyTextSafety = &ClassifyTextSafety,
    .DestroyModel = &DestroyModel,
    .GetEstimatedPerformance = &GetEstimatedPerformance,
    .QueryGPUAdapter = &QueryGPUAdapter,
    .SetFatalErrorNonGpuFn = &SetFatalErrorNonGpuFn,

    .SessionCreateModel = &SessionCreateModel,
    .SessionExecuteModel = &SessionExecuteModel,
    .SessionSizeInTokensInputPiece = &SessionSizeInTokensInputPiece,
    .SessionScore = &SessionScore,
    .CreateSession = &CreateSession,
    .CloneSession = &CloneSession,
    .DestroySession = &DestroySession,
    .CreateCancel = &CreateCancel,
    .DestroyCancel = &DestroyCancel,
    .CancelExecuteModel = &CancelExecuteModel,
    .ts_api =
        {
            .CreateModel = &CreateTSModel,
            .DestroyModel = &DestroyTSModel,
            .ClassifyTextSafety = &TSModelClassifyTextSafety,
        },
};

const ChromeMLAPI* GetFakeMlApi() {
  return &g_api;
}

}  // namespace fake_ml
